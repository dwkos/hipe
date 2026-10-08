/*  Copyright (c) 2026 General Development Systems

    SPDX-License-Identifier: 0BSD

    Permission to use, copy, modify, and/or distribute this software for any
    purpose with or without fee is hereby granted.

    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

/* Relaying of the FIFO instructions between a framing client and a framed child. hiped passes them on without
 * reading them: each one sent to the child's frame must arrive at the child from location 0, and each one the child
 * sends to location 0 must arrive here from the frame's location, with requestor and all four arguments unchanged.
 * Run by run.sh against a private hiped, as the top-level client; a forked child client fills an <iframe> and sends
 * back every FIFO instruction it receives. Prints PASS/FAIL lines and exits non-zero if anything failed. */

#include <hipe.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static hipe_session s;
static int fails = 0;
#define CHECK(name, cond) do { int ok_ = (cond); if(!ok_) fails++; \
    printf("%s %s\n", ok_ ? "PASS" : "FAIL", name); fflush(stdout); } while(0)

static int isFifo(char op) {
    return op == HIPE_OP_FIFO_ADD_ABILITY || op == HIPE_OP_FIFO_REMOVE_ABILITY || op == HIPE_OP_FIFO_GET_PEER
        || op == HIPE_OP_FIFO_DROP_PEER || op == HIPE_OP_FIFO_OPEN || op == HIPE_OP_FIFO_CLOSE
        || op == HIPE_OP_FIFO_RESPONSE || op == HIPE_OP_FIFO_INFO;
}

/* the framed child: sends every FIFO instruction from its parent back to it, unless it didn't come from location 0 */
static void childMain(const char* key) {
    hipe_session cs = hipe_open_session(key, 0, 0, "fifo-child");
    if(!cs) _exit(1);
    hipe_instruction in; hipe_instruction_init(&in);
    while(hipe_next_instruction(cs, &in, 1) > 0) {
        if(isFifo(in.opcode) && in.location == 0)
            hipe_send(cs, in.opcode, in.requestor, 0, 4, in.arg[0] ? in.arg[0] : "", in.arg[1] ? in.arg[1] : "",
                      in.arg[2] ? in.arg[2] : "", in.arg[3] ? in.arg[3] : "");
        hipe_instruction_clear(&in);
    }
    _exit(0);
}

static int argIs(hipe_instruction* r, int i, const char* want) {
    return r->arg_length[i] == strlen(want) && (!r->arg_length[i] || !memcmp(r->arg[i], want, r->arg_length[i]));
}

static void timedOut(int sig) { (void)sig; printf("FAIL timed out waiting for a relayed instruction\n"); _exit(1); }

int main() {
    signal(SIGALRM, timedOut);
    s = hipe_open_session(0, 0, 0, "fifo-test");
    if(!s) { fprintf(stderr, "no session: %s\n", hipe_last_error(0)); return 2; }

    hipe_send(s, HIPE_OP_APPEND_TAG, 0, 0, 4, "iframe", "", "", "");
    hipe_loc fr = hipe_newest_location();
    hipe_instruction r; hipe_instruction_init(&r);
    hipe_send(s, HIPE_OP_GET_FRAME_KEY, 0, fr, 0);
    hipe_await_instruction(s, &r, HIPE_OP_KEY_RETURN);
    char key[256]; snprintf(key, sizeof key, "%.*s", (int)r.arg_length[0], r.arg[0]);
    hipe_instruction_clear(&r);
    pid_t child = fork();
    if(!child) childMain(key);

    /* each instruction goes to the child and comes back; arguments include newlines, '=' and an empty one */
    struct { char op; const char* name; } ops[] = {
        {HIPE_OP_FIFO_INFO, "FIFO_INFO"}, {HIPE_OP_FIFO_RESPONSE, "FIFO_RESPONSE"}, {HIPE_OP_FIFO_OPEN, "FIFO_OPEN"},
        {HIPE_OP_FIFO_CLOSE, "FIFO_CLOSE"}, {HIPE_OP_FIFO_DROP_PEER, "FIFO_DROP_PEER"},
        {HIPE_OP_FIFO_GET_PEER, "FIFO_GET_PEER"}, {HIPE_OP_FIFO_ADD_ABILITY, "FIFO_ADD_ABILITY"},
        {HIPE_OP_FIFO_REMOVE_ABILITY, "FIFO_REMOVE_ABILITY"},
    };
    const char* a0 = "/tmp/fifo-test/pipe";
    const char* a1 = "ability=Edit in Appscape\npeer-title=notes.txt - Appscape";
    const char* a2 = "x-test-key=a=b";
    for(unsigned i = 0; i < sizeof ops / sizeof ops[0]; i++) {
        hipe_send(s, ops[i].op, 1000 + i, fr, 4, a0, a1, a2, "");
        alarm(10);
        hipe_await_instruction(s, &r, ops[i].op);
        alarm(0);
        char name[100]; snprintf(name, sizeof name, "%s relayed to the child and back unchanged", ops[i].name);
        int ok = r.location == fr && r.requestor == 1000 + i
              && argIs(&r, 0, a0) && argIs(&r, 1, a1) && argIs(&r, 2, a2) && argIs(&r, 3, "");
        CHECK(name, ok);
        if(!ok) printf("  got location %llu (frame %llu), requestor %llu, args [%.*s] [%.*s] [%.*s] [%.*s]\n",
            (unsigned long long)r.location, (unsigned long long)fr, (unsigned long long)r.requestor,
            (int)r.arg_length[0], r.arg[0], (int)r.arg_length[1], r.arg[1], (int)r.arg_length[2], r.arg[2],
            (int)r.arg_length[3], r.arg[3]);
        hipe_instruction_clear(&r);
    }

    /* at the top level there is no framing manager: FIFO_INFO to location 0 goes nowhere, and the session goes on */
    hipe_send(s, HIPE_OP_FIFO_INFO, 0, 0, 4, a0, "", "", "");
    hipe_send(s, HIPE_OP_APPEND_TAG, 0, 0, 4, "span", "", "", "still here");
    hipe_send(s, HIPE_OP_GET_CONTENT, 0, hipe_newest_location(), 1, "0");
    alarm(10);
    hipe_await_instruction(s, &r, HIPE_OP_CONTENT_RETURN);
    alarm(0);
    CHECK("FIFO_INFO at the top level is dropped without harm", argIs(&r, 0, "still here"));
    hipe_instruction_clear(&r);

    kill(child, SIGTERM);
    hipe_close_session(s);
    printf("%s: %d failed\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
