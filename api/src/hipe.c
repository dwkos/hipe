/*  Copyright (c) 2016-2026 Daniel Kos, General Development Systems

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of this Software library.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
*/

#include "hipe.h"
#include "common.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/un.h> /*for struct sockaddr_un*/
#include <pthread.h>
#include <sys/inotify.h>
#include <poll.h>
#include <time.h>

#define READ_BUFFER_SIZE 65536
/* Defines the size (in bytes) of the read buffer into which instruction data is
 * read in from the display server. Data read in a single read operation may
 * correspond to one or more instructions, or even a fragment of a single
 * large instruction. It is large so that big replies (e.g. HIPE_OP_GET_CONTENT of a
 * large document, image data) arrive in few system calls rather than thousands. */

#define SEND_BUFFER_FLUSH_SIZE 65536
/* With buffering enabled (hipe_set_buffered()), outgoing instructions are collected
 * until this many bytes are waiting, then sent in one go. An instruction at least this
 * large is sent directly after flushing what's waiting, without being copied. */

#define MAX_READS 50
/*the maximum number of consecutive read operations that can be completed
 *without a return.*/

#define HIPE_KEYFILE_REWRITE_RETRY_TIMEOUT_MS 3000
/*Overall wall-clock budget hipe_open_session() spends retrying a denied container request when
 *the key came from a keyfile, waiting for it to be rewritten with a fresh key between attempts.
 *Covers N clients launched in quick succession all reading the same not-yet-rotated single-use
 *key: a retry can itself lose to yet another late-arriving client claiming the newest key first,
 *so this may cover several retries, not just one - see hipe_open_session(). Whoever's rotating
 *the key is already running at this point (we got a real denial from it), so this only has to
 *cover a quick rewrite, not a cold start - see HIPE_KEYFILE_CREATE_TIMEOUT_MS for that.*/

#define HIPE_KEYFILE_CREATE_TIMEOUT_MS 10000
/*Separate, longer wall-clock budget for the case where the keyfile doesn't exist yet at all -
 *whatever creates it (a framing manager, hiped itself) may still be cold-starting, which is a
 *slower operation than rotating an already-running server's key, so this gets more room. Since
 *this can be a genuinely long, silent-looking wait, hipe_read_key_from_file() prints an stderr
 *notice once it starts waiting, so it doesn't look like the client has just hung.*/

#define HIPE_KEYFILE_OPEN_POLL_INTERVAL_MS 250
/*How often hipe_read_key_from_file() retries opening a keyfile that doesn't exist yet. Plain
 *polling, not event-driven like the rewrite-wait: inotify can't watch a path that doesn't exist
 *yet.*/


struct _hipe_session { /*all session-specific state variables go here!*/
    int connection_fd; /*File descriptor for the connection, or -1 when disconnected.*/

    pthread_mutex_t send_lock; //this mutex is used to make sending outgoing
    //instructions threadsafe.

    char readBuffer[READ_BUFFER_SIZE];
    instruction_encoder outgoingInstruction;

    int buffered; /*nonzero if outgoing instructions are buffered (see hipe_set_buffered()). Guarded by send_lock.*/
    char* sendBuffer; /*outgoing bytes not yet sent, when buffered. Guarded by send_lock.*/
    size_t sendLength, sendCapacity;
    instruction_decoder incomingInstruction;

    /*linked list queue of incoming instructions:*/
    hipe_instruction* oldestInstruction;
    hipe_instruction* newestInstruction;

    hipe_loc_pool locations; /*this session's location numbers. Guarded by send_lock.*/
    char lastError[256]; /*the server's reason for the last fatal error, or empty.*/
};

static __thread hipe_loc newestLocation = 0; /*the number this thread's last APPEND_TAG or INSERT_TAG allocated*/
static char openError[256]; /*the server's reason for refusing the last hipe_open_session(), or empty*/

int read_to_queue(hipe_session session, int blocking);
/*blocking or nonblocking read from server. Receives the number of characters
 *available in the connection's input buffer, and begins assembling them into an
 *instruction. */

void hipe_session_init(struct _hipe_session* obj) {
/*contructor for a _hype_session struct instance.*/
    instruction_encoder_init(&obj->outgoingInstruction);
    instruction_decoder_init(&obj->incomingInstruction);
    pthread_mutex_init(&obj->send_lock, NULL);
    obj->buffered = 0;
    obj->sendBuffer = 0;
    obj->sendLength = obj->sendCapacity = 0;
    obj->oldestInstruction = 0;
    obj->newestInstruction = 0;
    hipe_loc_pool_init(&obj->locations);
    obj->lastError[0] = '\0';
}

void hipe_session_clear(struct _hipe_session* obj) {
/*destructor for a _hype_session struct instance.*/
    instruction_encoder_clear(&obj->outgoingInstruction);
    instruction_decoder_clear(&obj->incomingInstruction);
    free(obj->sendBuffer);
    obj->sendBuffer = 0;
    obj->sendLength = obj->sendCapacity = 0;
    hipe_loc_pool_clear(&obj->locations);
    pthread_mutex_destroy(&obj->send_lock);
}

void hipe_disconnect(hipe_session session) {
/* Private function to close a session's server connection without freeing the server struct.
 * The user doesn't need to call this, it is called automatically by other functions when the client
 * receives a critical error, the connection is broken, or the user calls hipe_close_session().
 * Postcondition: the session's file descriptor is set to -1. Other functions should check for this
 * before attempting to transmit or receive data. */

    if(session->connection_fd == -1) return; //already disconnected.
    shutdown(session->connection_fd, SHUT_RDWR);
    close(session->connection_fd);
    session->connection_fd = -1;
}

static long long hipe_monotonic_ms(void) {
/*Current time in milliseconds off an arbitrary, monotonically increasing clock, immune to
 *wall-clock adjustments - used only for measuring elapsed time against a budget, never for
 *anything resembling an absolute timestamp.*/

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long) ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int hipe_read_key_from_file(const char* keyPath, char* keyOut, long long deadline) {
/*Reads a single-use key from keyPath into keyOut (which must have room for 200 bytes, matching
 *the `key` buffer in hipe_open_session()). Returns 1 on success, 0 on failure (having already
 *printed an error).
 *
 *If the file can't even be opened yet - e.g. ENOENT because whatever creates it (a framing
 *manager, hiped itself) hasn't started up yet - polls for it to appear every
 *HIPE_KEYFILE_OPEN_POLL_INTERVAL_MS until deadline, rather than failing on the very first
 *attempt. A file that opens but can't be read from (rather than not existing at all) is treated
 *as a harder failure and not retried - that's not the "hasn't started up yet" race this is for.
 *
 *Callers may pass a deadline far enough out (HIPE_KEYFILE_CREATE_TIMEOUT_MS) that this ends up
 *waiting several seconds for a cold-starting framing manager - long enough to look like a hang
 *if we stayed silent, so an stderr notice is printed once, the first time an open attempt fails.*/

    int announcedWait = 0;
    for(;;) {
        FILE* keyfile = fopen(keyPath, "r");
        if(keyfile) {
            if(!fgets(keyOut, 200, keyfile)) {
                fprintf(stderr, "Hipe: Could not read key from keyfile: %s\n", keyPath);
                perror("Hipe");
                fclose(keyfile);
                return 0;
            }
            fclose(keyfile);
            return 1;
        }

        if(hipe_monotonic_ms() >= deadline) {
            fprintf(stderr, "Hipe: Could not open keyfile: %s\n", keyPath);
            perror("Hipe");
            return 0;
        }

        if(!announcedWait) {
            fprintf(stderr, "Hipe: Waiting for keyfile to appear: %s\n", keyPath);
            announcedWait = 1;
        }

        struct timespec ts = { 0, HIPE_KEYFILE_OPEN_POLL_INTERVAL_MS * 1000000L };
        nanosleep(&ts, NULL);
    }
}

static void hipe_wait_for_keyfile_update(const char* keyPath, int timeoutMs) {
/*Blocks until keyPath is rewritten (IN_CLOSE_WRITE - the whole point is to wait for a *complete*
 *rewrite, not catch a partial one mid-write) or timeoutMs elapses, whichever comes first. Also
 *wakes on IN_DELETE_SELF so a removed keyfile doesn't just hang until the timeout for no reason.
 *Never fails outright: if inotify itself is unavailable, this just falls back to sleeping out the
 *full timeout, which is still strictly better than not retrying at all.*/

    int inotifyFd = inotify_init1(IN_NONBLOCK);
    if(inotifyFd == -1) {
        struct timespec ts = { timeoutMs/1000, (timeoutMs%1000) * 1000000L };
        nanosleep(&ts, NULL);
        return;
    }

    /*A failed watch (e.g. the file doesn't exist yet) just means we fall through to poll()'s
     *timeout below rather than waking early - still correct, just not prompt about it.*/
    inotify_add_watch(inotifyFd, keyPath, IN_CLOSE_WRITE | IN_DELETE_SELF);

    struct pollfd pfd;
    pfd.fd = inotifyFd;
    pfd.events = POLLIN;
    poll(&pfd, 1, timeoutMs);
    /*Whichever of an event, a timeout, or a poll() error got us here, the caller just retries -
     *if the key genuinely hasn't changed it'll just see the same denial again.*/

    close(inotifyFd); /*implicitly removes the watch.*/
}

hipe_session hipe_open_session(const char* host_key, const char* socket_path, const char* key_path, const char* clientName) {
    openError[0] = '\0';
/*connect to the host socket. A custom socket file path may be specified.
  The parameters may be null pointers in which case default values are used instead.
*/

    int fd; /*file descriptor*/

    char sockPath[200];
    char keyPath[200];
    char themeIndex[4];
    char key[200];

    if(socket_path) { /*socket path specified by the caller*/
        strncpy(sockPath, socket_path, 199);
    } else if(getenv("HIPE_SOCKET")) { /*socket path specified by environment variable HIPE_SOCKET*/
        strncpy(sockPath, getenv("HIPE_SOCKET"), 199);
    } else { /*Infer default socket path*/
        default_runtime_dir(sockPath, 200);
        strncat(sockPath, "hipe.socket", 200-strlen(sockPath));
    }

    if(key_path) { /*keyfile path specified by caller*/
        strncpy(keyPath, key_path, 199);
    } else if(getenv("HIPE_KEYFILE")) { //keyfile path specified by environment variable HIPE_KEYFILE
        strncpy(keyPath, getenv("HIPE_KEYFILE"), 199);
    } else { /*Infer default keyfile path*/
        default_runtime_dir(keyPath, 200);
        strncat(keyPath, "hipe.hostkey", 200-strlen(keyPath));
    }

    int usingKeyfile = 0; /*whether key came from key_path/HIPE_KEYFILE rather than being handed
                            *to us directly - only then is there a file we can watch/poll and a
                            *rotating key worth retrying against, see the retry loop below.*/
    long long rewriteRetryDeadline = 0; /*only meaningful when usingKeyfile.*/

    if(host_key) { /*host key specified by user*/
        strncpy(key, host_key, 199);
    } else if(getenv("HIPE_HOSTKEY")) { /*key specified by environment [NB: each key can only be used once.]*/
        strncpy(key, getenv("HIPE_HOSTKEY"), 199);
    } else { /*need to load a key from the given key_path.*/
        usingKeyfile = 1;
        long long createDeadline = hipe_monotonic_ms() + HIPE_KEYFILE_CREATE_TIMEOUT_MS;
        if(!hipe_read_key_from_file(keyPath, key, createDeadline))
            return 0;
        /*Started fresh here rather than reusing whatever's left of createDeadline: whoever writes
         *this file is confirmed running now (we just read it), so a denial from here on is the
         *quick-rotation race, not the cold-start one - see HIPE_KEYFILE_REWRITE_RETRY_TIMEOUT_MS.*/
        rewriteRetryDeadline = hipe_monotonic_ms() + HIPE_KEYFILE_REWRITE_RETRY_TIMEOUT_MS;
    }

    /*Check for theme index specified in environment*/
    themeIndex[0]='\0'; //default unspecified
    if(getenv("HIPE_THEME")) {
        strncpy(themeIndex, getenv("HIPE_THEME"), 4);
        themeIndex[3] = '\0'; //null terminate in case strncpy ran out of space to null-terminate.
    }

    /*A container request naming a stale key gets denied - normally a hard failure, but when the
     *key came from a keyfile, a denial commonly just means we lost a race against whoever else
     *is meant to rotate that file (e.g. several clients launched in quick succession all reading
     *the same not-yet-rotated single-use key). So in that case, wait for the file to be rewritten
     *(or a fixed timeout, whichever comes first) and retry with whatever's there by then, within
     *rewriteRetryDeadline - a retry can itself lose to a further late-arriving client, so this may
     *take several rounds, not just one, before finally giving up. A key supplied directly
     *(host_key/HIPE_HOSTKEY) has no such file to wait on, so it only ever gets the one attempt.*/
    for(;;) {
        /*Allocate a socket endpoint file descriptor. SOCK_CLOEXEC: don't let programs this client
         *exec inherit the connection - an inherited copy keeps it open after this client exits.*/
        if ((fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0)) == -1) {
            perror("Hipe: socket");
            return 0; /*null pointer*/
        }

        struct sockaddr_un remote;
        remote.sun_family = AF_UNIX;
        strcpy(remote.sun_path, sockPath);
        int len = strlen(remote.sun_path) + sizeof(remote.sun_family);
        if (connect(fd, (struct sockaddr *)&remote, len) == -1) {
            close(fd);
            fprintf(stderr, "Hipe: Could not connect to socket: %s\n", sockPath);
            perror("Hipe");
            return 0; /*null pointer*/
        }

        /*Now request a container using a given host key:*/
        hipe_session session = (hipe_session) malloc(sizeof(struct _hipe_session));
        hipe_session_init(session);
        session->connection_fd = fd;
        hipe_instruction rq;
        hipe_instruction_init(&rq);
        rq.opcode = HIPE_OP_REQUEST_CONTAINER;
        rq.location = 0;
        rq.requestor = getpid();
        rq.arg[0] = key;
        rq.arg_length[0] = strlen(key);
        rq.arg[1] = (char*) clientName;
        rq.arg_length[1] = strlen(clientName);
        rq.arg[2] = themeIndex;
        rq.arg_length[2] = strlen(themeIndex);
        rq.arg[3] = HIPE_PROTOCOL_VERSION;
        rq.arg_length[3] = strlen(HIPE_PROTOCOL_VERSION);
        hipe_send_instruction(session, rq);

        /*Await response from server. If the container request is rejected then close the
        session immediately and don't return it.*/
        hipe_instruction incoming;
        hipe_instruction_init(&incoming);
        int result;
        result = hipe_await_instruction(session, &incoming, HIPE_OP_CONTAINER_GRANT);
        if(result<0) {
            fprintf(stderr, "Hipe: Bad connection.\n");
            hipe_close_session(session);
            return 0;
        }
        if(incoming.arg[0][0] != '1') {
            hipe_close_session(session);

            if(usingKeyfile) {
                long long remainingMs = rewriteRetryDeadline - hipe_monotonic_ms();
                if(remainingMs > 0) {
                    hipe_wait_for_keyfile_update(keyPath, (int) remainingMs);
                    if(!hipe_read_key_from_file(keyPath, key, rewriteRetryDeadline))
                        return 0;
                    continue; /*retry, with whatever key is in the file now.*/
                }
            }

            fprintf(stderr, "Hipe: Container request denied.\n");
            return 0; /*null pointer*/
        } else {
            //fprintf(stderr, "Hipe: Container request granted!\n"); /*uncomment for extra verbosity*/
        }
        hipe_instruction_clear(&incoming);

        return session; /*success*/
    }
}


/*Writes all of data to the connection, however many send() calls it takes (a stream socket may accept
 *fewer bytes than asked, e.g. when interrupted by a signal). Returns 0, or -1 if the connection failed.*/
static int write_all(int fd, const char* data, size_t length) {
    while(length > 0) {
        ssize_t sent = send(fd, data, length, MSG_NOSIGNAL);
        if(sent < 0) {
            if(errno == EINTR) continue;
            return -1;
        }
        data += sent;
        length -= (size_t) sent;
    }
    return 0;
}

/*Sends any buffered outgoing bytes. The caller must hold send_lock. Returns 0, or -1 if disconnected.*/
static int flush_locked(hipe_session session) {
    if(!session->sendLength) return 0;
    size_t length = session->sendLength;
    session->sendLength = 0;
    if(session->connection_fd == -1) return -1;
    if(write_all(session->connection_fd, session->sendBuffer, length) == -1) {
        hipe_disconnect(session);
        return -1;
    }
    return 0;
}

int hipe_flush(hipe_session session) {
    int result;
    if(!session) return -1;
    pthread_mutex_lock(&session->send_lock);
    result = flush_locked(session);
    pthread_mutex_unlock(&session->send_lock);
    return result;
}

void hipe_set_buffered(hipe_session session, int buffered) {
    if(!session) return;
    pthread_mutex_lock(&session->send_lock);
    if(!buffered) flush_locked(session);
    session->buffered = buffered;
    pthread_mutex_unlock(&session->send_lock);
}

/*Frees, in the pool, each number in a list such as "57,1000-1499" (FREE_LOCATION's list form).*/
static void free_location_list(hipe_loc_pool* pool, const char* list, size_t length) {
    size_t i = 0;
    while(i < length) {
        hipe_loc a = 0, b;
        while(i < length && list[i] >= '0' && list[i] <= '9') a = a*10 + (hipe_loc)(list[i++] - '0');
        b = a;
        if(i < length && list[i] == '-') {
            b = 0;
            i++;
            while(i < length && list[i] >= '0' && list[i] <= '9') b = b*10 + (hipe_loc)(list[i++] - '0');
        }
        for(hipe_loc n = a; n && n <= b; n++) hipe_loc_pool_free(pool, n);
        while(i < length && list[i] != ',') i++; /*the server rejects a malformed list; just skip to the next item*/
        i++;
    }
}

int hipe_send_instruction(hipe_session session, hipe_instruction instruction) {
/*encode and transmit an instruction (or, when buffered, queue it to be transmitted).*/
    int err = 0;
    if(session->connection_fd == -1) return -1; //not connected.

    pthread_mutex_lock(&session->send_lock);
    //enforce atomicity so that two threads can send instructions without
    //messing up the encoding. Note that receiving instructions is NOT
    //thread safe, so only one thread should require and be checking for
    //replies.

    //The client allocates the location of each tag it appends or inserts, and sends it in the requestor field.
    //Freed locations become reusable.
    if(instruction.opcode == HIPE_OP_APPEND_TAG || instruction.opcode == HIPE_OP_INSERT_TAG) {
        instruction.requestor = newestLocation = hipe_loc_pool_allocate(&session->locations);
    } else if(instruction.opcode == HIPE_OP_FREE_LOCATION) {
        if(instruction.arg_length[0]) free_location_list(&session->locations, instruction.arg[0], instruction.arg_length[0]);
        else hipe_loc_pool_free(&session->locations, instruction.location);
    }

    instruction_encoder_encodeinstruction(&session->outgoingInstruction, instruction);
    const char* encoded = (const char*) session->outgoingInstruction.encoded_output;
    size_t length = session->outgoingInstruction.encoded_length;

    if(session->buffered && length < SEND_BUFFER_FLUSH_SIZE) {
        if(session->sendLength + length > session->sendCapacity) {
            size_t capacity = session->sendCapacity ? session->sendCapacity : SEND_BUFFER_FLUSH_SIZE;
            while(capacity < session->sendLength + length) capacity *= 2;
            char* grown = (char*) realloc(session->sendBuffer, capacity);
            if(!grown) { //out of memory: send what's waiting, then this instruction, directly.
                err = flush_locked(session);
                if(!err && write_all(session->connection_fd, encoded, length) == -1) {
                    hipe_disconnect(session);
                    err = -1;
                }
                pthread_mutex_unlock(&session->send_lock);
                return err;
            }
            session->sendBuffer = grown;
            session->sendCapacity = capacity;
        }
        memcpy(session->sendBuffer + session->sendLength, encoded, length);
        session->sendLength += length;
        if(session->sendLength >= SEND_BUFFER_FLUSH_SIZE) err = flush_locked(session);
    } else {
        /*unbuffered, or too big to be worth copying: send what's waiting first, to keep the order.*/
        err = flush_locked(session);
        if(!err && write_all(session->connection_fd, encoded, length) == -1) {
            hipe_disconnect(session);
            err = -1;
        }
    }
    pthread_mutex_unlock(&session->send_lock);

    return err;
}


short hipe_next_instruction(hipe_session session, hipe_instruction* instruction_ret, short blocking)
{
    short result;

    hipe_flush(session); /*anything the client sent must be on its way before it looks for replies.*/
    hipe_instruction_clear(instruction_ret);
    /*clear any previous instruction so that the user doesn't have to.*/

    while(!session->oldestInstruction) {
    /*Only read something new from server if the queue is empty.*/
        result = read_to_queue(session, blocking);
        if(!blocking && result == 0) return 0;
        else if(result == -1) { /* Not connected to server */
            instruction_ret->opcode = HIPE_OP_SERVER_DENIED;
            return -1;
        }
    }

    /*pull next instruction from queue and update queue.*/
    *instruction_ret = *(session->oldestInstruction); /*shallow copy*/
    if(session->oldestInstruction == session->newestInstruction)
        session->newestInstruction = 0; /* if this was the only waiting instruction, reflect the now empty state of queue */
    free(session->oldestInstruction);   /*shallow clear. Any args now exist in instruction_ret only.*/
    session->oldestInstruction = instruction_ret->next;
    instruction_ret->next = 0;

    return 1; /*success*/
}


int read_to_queue(hipe_session session, int blocking)
/*If blocking is set, the function will not return until at least a partial
 *instruction has been read. This function processes zero or more complete
 *instructions before it returns. It then adds these to the session's incoming
 *instruction queue.
 *
 *Returns the number of completed instructions read into the session queue (if
 *any), or -1 on error.
 */
{
    if(session->connection_fd == -1) {
        /*Already-known disconnection (the initial detection below reports it instantly, no delay).
         *A well-behaved caller checks for this and stops calling; a caller that doesn't would
         *otherwise busy-spin here, since blocking mode has nothing left to block on. Penalise only
         *that repeat-call case with a short sleep instead of returning instantly forever.*/
        if(blocking) usleep(50000);
        return -1; /*not connected*/
    }

    int completedInstructions;
    completedInstructions = 0;
    ssize_t bufferedChars; /*number of characters that have been read into the buffer. Must be <=READ_BUFFER_SIZE */
    short n;
    for(n=0; n<MAX_READS; n++) {
    /*stay in this function for as long as characters are available to be read,
      or until the maximum number of allowed iterations is exhausted.
    */

        if(n>0) blocking=0; /*only enable blocking for the first iteration. Anything that follows is a freebie.*/

        /*attempt to read new characters*/
        bufferedChars = recv(session->connection_fd, session->readBuffer, READ_BUFFER_SIZE, (blocking ? 0 : MSG_DONTWAIT));
        /*can return -1 if connection closed, or 0 when no more ready.*/

        int p;
        if(bufferedChars < 0) { /*connection closed, or error. Or nothing to read right now.*/
            if(errno == EAGAIN) { /*nothing more to read right now*/
                return completedInstructions; /*success*/
            } else { /*disconnected by peer, broken pipe, etc.*/
                hipe_disconnect(session);
                return -1;
            }
        } else if(bufferedChars == 0) { /*connection closed by peer*/
            hipe_disconnect(session);
            return -1;
        } else for(p=0; p<bufferedChars;) { /*let's process our input! (p represents current offset from start of input buffer)*/

            p += instruction_decoder_feed(&session->incomingInstruction, 
                                          session->readBuffer + p, bufferedChars-p);
            if(instruction_decoder_iscomplete(&session->incomingInstruction)) {

                if(session->incomingInstruction.output.opcode == HIPE_OP_SERVER_NOTICE) {
                /*A non-fatal client bug reported by the server: tell the developer, don't queue it.*/
                    fprintf(stderr, "Hipe: Server notice: %.*s\n", (int) session->incomingInstruction.output.arg_length[0],
                            session->incomingInstruction.output.arg[0]);
                    instruction_decoder_clear(&session->incomingInstruction);
                    continue;
                }
                if(session->incomingInstruction.output.opcode == HIPE_OP_SERVER_DENIED) {
                /*Access to the server has been denied. Critical. Disconnect, keeping the server's reason.*/
                    hipe_instruction* denial = &session->incomingInstruction.output;
                    if(denial->arg_length[0]) {
                        snprintf(session->lastError, sizeof session->lastError, "%.*s", (int) denial->arg_length[0], denial->arg[0]);
                        snprintf(openError, sizeof openError, "%s", session->lastError);
                        fprintf(stderr, "Hipe: Disconnected by the server: %s\n", session->lastError);
                    }
                    hipe_disconnect(session);
                    if(completedInstructions) return completedInstructions;
                    else return -1; /*disconnected*/
                }

                completedInstructions++;

                /*Allocate the new instruction struct and add it to the session's queue of new instructions.*/
                hipe_instruction* newInstruction = (hipe_instruction*) malloc(sizeof(hipe_instruction));
                hipe_instruction_copy(newInstruction, &session->incomingInstruction.output);

                instruction_decoder_clear(&session->incomingInstruction);

                if(session->newestInstruction) session->newestInstruction->next = newInstruction;
                else session->oldestInstruction = newInstruction; /*if the queue is empty, then it's our oldest as well as our newest.*/
                session->newestInstruction = newInstruction;
            }
        }
    }
    return completedInstructions; /*success*/
}


short hipe_close_session(hipe_session session)
{
    hipe_flush(session);
    hipe_disconnect(session);
    hipe_session_clear(session);
    free(session);
    return 0;
}


short hipe_await_instruction(hipe_session session, hipe_instruction* instruction_ret, short opcode)
{
    hipe_instruction* current=session->oldestInstruction; /* the last instruction we have examined in the queue, or are about to examine. */
    hipe_instruction* previous=0; /* the instruction that points to current. */
    int fetched_instructions=0;

    hipe_flush(session); /*the request being awaited may still be in the send buffer.*/

    while(1) { /* we will either return the desired instruction eventually, or return an error condition, such as disconnection. */
        while(current) { /* when we run out of instructions to examine, we'll have to leave this loop to get more */
            /* examine current instruction */
            if(current->opcode == opcode) {
                /* we've found the element we're looking for. Splice it out of the queue and return it. */
                *instruction_ret = *current; /*return a shallow copy to use existing allocations. We'll delete the original */

                if(previous)
                    previous->next = current->next;
                else /* current is at start of queue */
                    session->oldestInstruction = current->next;

                if(current == session->newestInstruction) {
                    session->newestInstruction = previous;
                    if(previous)
                        session->newestInstruction->next = 0;
                }

                free(current);
                instruction_ret->next = 0;

                return 1; /* success */
            }

            /* traverse to next in queue */
            previous = current;
            current = current->next;
        }

        /* need to fetch more instructions */
        do {
            fetched_instructions = read_to_queue(session, 1);
            if(fetched_instructions < 0) /*bad. handle error.*/
                return -1;
        } while(fetched_instructions == 0);

        if(previous)
            current = previous->next;
        else
            current = session->oldestInstruction;
    }
}

int hipe_send(hipe_session session, char opcode, uint64_t requestor, hipe_loc location, int n_args, ...) {
    int result;
    hipe_instruction instruction;
    hipe_instruction_init(&instruction);
    instruction.opcode = opcode;
    instruction.requestor = requestor;
    instruction.location = location;
    int i;
    va_list args;
    const char* this_arg;
    va_start(args, n_args); //process variadic arguments.
    for(i=0; i<n_args && i<HIPE_NARGS; i++) {
        this_arg = va_arg(args, const char*);
        if(this_arg) {
            instruction.arg_length[i] = strlen(this_arg); //adding null terminator to length breaks certain behaviour. Why?
            //note that the encoded arguments in the instruction should *not* be null terminated.
            instruction.arg[i] = (char*) this_arg;
        }
    }
    va_end(args);
    result = hipe_send_instruction(session, instruction);
    //hipe_instruction_clear(&instruction);
    return result;
}


hipe_loc hipe_newest_location() {
    return newestLocation;
}

hipe_loc hipe_reserve_location(hipe_session session) {
    pthread_mutex_lock(&session->send_lock);
    hipe_loc n = hipe_loc_pool_allocate(&session->locations);
    pthread_mutex_unlock(&session->send_lock);
    return n;
}

hipe_loc hipe_reserve_locations(hipe_session session, size_t count) {
    pthread_mutex_lock(&session->send_lock);
    hipe_loc n = hipe_loc_pool_allocate_run(&session->locations, count);
    pthread_mutex_unlock(&session->send_lock);
    return n;
}

int hipe_send_markup(hipe_session session, hipe_loc where, const char* markup, int append,
                     const hipe_loc* reserved, size_t count) {
    /*build arg[2]: the numbers, with consecutive ones merged into ranges ("57,1000-1499").*/
    char* list = (char*) malloc(count * 42 + 1);
    size_t length = 0;
    list[0] = '\0';
    for(size_t i = 0; i < count; ) {
        size_t j = i;
        while(j+1 < count && reserved[j+1] == reserved[j] + 1) j++;
        if(j > i) length += sprintf(list + length, "%s%llu-%llu", length ? "," : "",
                                    (unsigned long long) reserved[i], (unsigned long long) reserved[j]);
        else length += sprintf(list + length, "%s%llu", length ? "," : "", (unsigned long long) reserved[i]);
        i = j + 1;
    }
    int result = hipe_send(session, append ? HIPE_OP_APPEND_TEXT : HIPE_OP_SET_TEXT, 0, where, 3, markup, "3", list);
    free(list);
    return result;
}

const char* hipe_last_error(hipe_session session) {
    return session ? session->lastError : openError;
}


int hipe_session_fd(hipe_session session) {
    if(!session) return -1;
    return session->connection_fd;
}


int hipe_queued_instructions(hipe_session session) {
    int count = 0;
    hipe_instruction* current;
    if(!session) return 0;
    hipe_flush(session); /*called from poll() loops, just before waiting: see hipe_session_fd().*/
    for(current = session->oldestInstruction; current; current = current->next)
        count++;
    return count;
}


