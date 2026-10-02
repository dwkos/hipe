/*  Copyright (c) 2015-2026 Daniel Kos, General Development Systems

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

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _HIPE_H
#define _HIPE_H

#include <sys/types.h>
#include "hipe_instruction.h"

struct _hipe_session;
typedef struct _hipe_session* hipe_session;


hipe_session hipe_open_session(const char* host_key, const char* socket_path, const char* key_path, const char* client_name);

short hipe_close_session(hipe_session);

int hipe_send_instruction(hipe_session session, hipe_instruction instruction);
/*encodes and transmits an instruction.*/

short hipe_next_instruction(hipe_session session, hipe_instruction* instruction_ret, short blocking);
/* optional blocking-wait for an instruction to be received from the server.
 * Returns 1 if a new instruction has been returned in instruction_ret, or 0 if nothing is available
 * and !blocking.
 */

short hipe_await_instruction(hipe_session session, hipe_instruction* instruction_ret, short opcode);
/* Awaits an instruction with a particular opcode, then returns that instruction without adding it
 * to the session queue.
 * Any other instructions that arrive in the meantime are placed in the session queue so they
 * can be retrieved as normal by hipe_next_instruction when checking for events.
 * Instructions already in the session queue are checked first, oldest first, so a reply that arrived
 * while something else was being awaited is still found. But a reply that hipe_next_instruction has
 * already retrieved is gone: don't call hipe_next_instruction between making a request and awaiting
 * its reply, or the wait will never end.
 */

int hipe_flush(hipe_session session);
/* Sends any instructions still waiting in the session's send buffer (see hipe_set_buffered()).
 * Returns 0, or -1 if the connection has failed. Only needed with buffering on, and only before
 * waiting on something other than libhipe: waiting or checking for instructions from the server
 * (hipe_next_instruction, hipe_await_instruction, hipe_queued_instructions) flushes first anyway,
 * as does hipe_close_session. */

void hipe_set_buffered(hipe_session session, int buffered);
/* Turns send buffering on (nonzero) or off. It's off by default: each instruction is sent as it is
 * made. With buffering on, instructions are collected and sent together, in one system call per
 * 64 KiB, which is much faster for clients that send many instructions at once (e.g. building a
 * large document). They are sent when the buffer fills, when the client waits or checks for
 * instructions from the server (so a request is always sent before its reply is awaited), on
 * hipe_flush(), and when buffering is turned off. A client that sends instructions and then waits
 * on something else without calling libhipe (a timer, its own pipe) must call hipe_flush() first,
 * or its last instructions won't reach the server until it next does.
 * The order of instructions is always preserved. */

int hipe_session_fd(hipe_session session);
/* Returns the file descriptor of the session's connection to the server, or -1 if disconnected.
 * This lets a client wait on Hipe and its own file descriptors (pipes, FIFOs, sockets) together
 * with poll(), select() or epoll, instead of polling on a timer. Only wait on it for readability;
 * never read from or write to it directly.
 *
 * IMPORTANT: libhipe queues instructions it has already received (a single read can take in
 * several, and hipe_await_instruction queues everything that isn't the awaited instruction).
 * Queued instructions are no longer in the socket, so they won't wake poll(). Always drain the
 * queue before waiting:
 *
 *     for(;;) {
 *         while(hipe_next_instruction(session, &instruction, 0) > 0) handle(&instruction);
 *         poll(fds, nfds, -1);  // fds includes hipe_session_fd(session)
 *         ...
 *     }
 *
 * hipe_queued_instructions() can be used to check that the queue is empty before waiting.
 * Both also send any instructions waiting in the send buffer (see hipe_set_buffered()), so with
 * this pattern buffered instructions always go out before the client waits.
 * A readable fd doesn't guarantee a complete instruction is available: hipe_next_instruction
 * may still return 0, in which case just wait again.
 */

int hipe_queued_instructions(hipe_session session);
/* Returns the number of instructions libhipe has received and queued, but that haven't yet
 * been retrieved with hipe_next_instruction. Doesn't read from the connection, so it won't
 * include data the server has sent that hasn't been read yet. Flushes the send buffer first.
 */

int hipe_send(hipe_session session, char opcode, uint64_t requestor, hipe_loc location, int n_args, ...);
/* Convenience function to send instructions when the arguments (0 or more) are null-terminated strings expressed
 * as char* or const char*
 */

hipe_loc hipe_newest_location();
 

#endif

#ifdef __cplusplus
}
#endif
