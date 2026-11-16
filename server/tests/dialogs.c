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

/* Dialogs shown by hiped for a top-level client (DIALOG, DIALOG_INPUT, FIFO_GET_PEER's file dialog): hiped keeps
 * serving instructions while one is open, a window shows one dialog at a time and later requests wait their turn,
 * each kind of answer is replied, the dialog blocks its own window but not other clients' windows, and a client
 * killed with dialogs open or waiting takes its window and dialogs with it. Run by run.sh against a private hiped
 * with no window manager, as a top-level client; answers and clicks use xdotool on $DISPLAY. A forked second
 * top-level window ("dialog-other") reports clicks over a pipe, and a forked "dialog-victim" is killed.
 * Prints PASS/FAIL lines and exits non-zero if anything failed. */
#include <hipe.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdarg.h>

static hipe_session s;
static int fails = 0;
static char v[400], v2[400];
static hipe_instruction last;

#define CHECK(name, cond) do { int ok_ = (cond); if(!ok_) fails++; \
    printf("%s %s  [%.80s|%.40s]\n", ok_ ? "PASS" : "FAIL", name, v, v2); fflush(stdout); } while(0)

static void hung(int sig) { (void)sig; printf("FAIL timed out waiting for hiped\n"); fflush(stdout); _exit(1); }

/* round trip: fails the test by timeout if hiped has stopped serving instructions */
static int sync_() {
    hipe_instruction_clear(&last);
    alarm(10);
    hipe_send(s, HIPE_OP_GET_ATTRIBUTE, 0, 0, 1, "x");
    int ok = hipe_await_instruction(s, &last, HIPE_OP_ATTRIBUTE_RETURN) > 0;
    alarm(0);
    return ok;
}
/* the next reply of the given kind; v and v2 get its first two arguments */
static uint64_t reply(char opcode) {
    hipe_instruction_clear(&last);
    alarm(10);
    hipe_await_instruction(s, &last, opcode);
    alarm(0);
    snprintf(v, sizeof v, "%.*s", (int)last.arg_length[0], last.arg[0] ? last.arg[0] : "");
    snprintf(v2, sizeof v2, "%.*s", (int)last.arg_length[1], last.arg[1] ? last.arg[1] : "");
    return last.requestor;
}

/* the X window with this exact name, or "" */
static const char* xwindow(const char* name) {
    static char b[32]; char cmd[120];
    snprintf(cmd, sizeof cmd, "xdotool search --onlyvisible --name '^%s$' 2>/dev/null | head -1", name);
    FILE* p = popen(cmd, "r"); b[0] = 0; if(p) { if(!fgets(b, sizeof b, p)) b[0] = 0; pclose(p); }
    b[strcspn(b, "\n")] = 0; return b;
}
static int shown(const char* name) { return xwindow(name)[0] != 0; }
static int waitShown(const char* name, int want) {
    for(int i = 0; i < 40; i++) { if(shown(name) == want) return 1; usleep(100000); }
    return 0;
}
static void xdotool(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
static void xdotool(const char* fmt, ...) {
    char cmd[512], a[400]; va_list ap; va_start(ap, fmt); vsnprintf(a, sizeof a, fmt, ap); va_end(ap);
    snprintf(cmd, sizeof cmd, "xdotool %s", a);
    if(system(cmd) != 0) printf("  (xdotool failed: %s)\n", a);
}
/* keys typed into the named dialog */
static void answer(const char* dialog, const char* keys) {
    char id[32]; snprintf(id, sizeof id, "%s", xwindow(dialog));
    if(!id[0]) { printf("  (no dialog %s)\n", dialog); return; }
    xdotool("windowfocus --sync %s", id); usleep(200000);
    xdotool("%s", keys);
}
static void clickIn(const char* window, int x, int y) {
    char id[32]; snprintf(id, sizeof id, "%s", xwindow(window));
    xdotool("mousemove --window %s %d %d sleep 0.1 click 1", id, x, y); usleep(400000);
}

static char* readKey() {
    static char b[256]; FILE* f = fopen(getenv("HIPE_KEYFILE"), "r"); b[0] = 0;
    if(f) { if(!fgets(b, sizeof b, f)) b[0] = 0; fclose(f); } return b;
}
/* start a top-level client in a child process, and wait until hiped has given it the key */
static pid_t startTopLevel(void (*fn)(void)) {
    char key0[256]; snprintf(key0, sizeof key0, "%s", readKey());
    pid_t p = fork();
    if(!p) { fn(); _exit(0); }
    for(int i = 0; i < 100 && !strcmp(readKey(), key0); i++) usleep(50000);
    usleep(500000); /* window mapping */
    return p;
}

/* another top-level client: writes a line to the pipe for each mousedown in its window */
static int clicks[2];
static void otherMain() {
    hipe_session o = hipe_open_session(0, 0, 0, "dialog-other");
    if(!o) _exit(1);
    hipe_send(o, HIPE_OP_SET_TEXT, 0, 0, 1, "other window");
    hipe_send(o, HIPE_OP_EVENT_REQUEST, 0, 0, 1, "mousedown");
    hipe_instruction in; hipe_instruction_init(&in);
    while(hipe_next_instruction(o, &in, 1) > 0) {
        if(in.opcode == HIPE_OP_EVENT) write(clicks[1], "c\n", 2);
        hipe_instruction_clear(&in);
    }
}
static int countClicks() {
    char b[64]; int n = 0; ssize_t r;
    while((r = read(clicks[0], b, sizeof b)) > 0) for(int i = 0; i < r; i++) n += b[i] == '\n';
    return n;
}

/* a top-level client killed while its dialog is open and another waits */
static void victimMain() {
    hipe_session vs = hipe_open_session(0, 0, 0, "dialog-victim");
    if(!vs) _exit(1);
    hipe_send(vs, HIPE_OP_SET_TEXT, 0, 0, 1, "victim window");
    hipe_send(vs, HIPE_OP_DIALOG, 1, 0, 3, "Victim A", "Pick", "Apple\nPear");
    hipe_send(vs, HIPE_OP_DIALOG, 2, 0, 2, "Victim B", "Info");
    hipe_instruction in; hipe_instruction_init(&in);
    while(hipe_next_instruction(vs, &in, 1) > 0) hipe_instruction_clear(&in);
}

int main() {
    hipe_instruction_init(&last);
    signal(SIGALRM, hung);
    if(!getenv("HIPE_KEYFILE")) { fprintf(stderr, "set HIPE_KEYFILE\n"); return 2; }
    if(pipe(clicks)) return 2;
    fcntl(clicks[0], F_SETFL, O_NONBLOCK);
    pid_t other = startTopLevel(otherMain);

    s = hipe_open_session(0, 0, 0, "dialog-test");
    if(!s) { fprintf(stderr, "no session: %s\n", hipe_last_error(0)); return 2; }
    hipe_send(s, HIPE_OP_SET_TEXT, 0, 0, 1, "dialog test window");
    hipe_send(s, HIPE_OP_EVENT_REQUEST, 0, 0, 1, "mousedown");
    sync_();
    usleep(500000);

    /* ---- a dialog is open; hiped still serves instructions ---- */
    hipe_send(s, HIPE_OP_DIALOG, 11, 0, 3, "Dlg A", "Pick one", "Apple\nPear");
    CHECK("D1 instructions answered while a dialog is open", sync_());
    CHECK("D1 the dialog is shown", waitShown("Dlg A", 1));

    /* ---- a second request waits for the first to be answered ---- */
    hipe_send(s, HIPE_OP_DIALOG_INPUT, 12, 0, 2, "Dlg B", "Type something");
    sync_(); usleep(300000);
    CHECK("D2 a second dialog waits while the first is open", !shown("Dlg B"));

    /* ---- the dialog blocks its own window, not other clients' ---- */
    countClicks();
    xdotool("windowraise %s", xwindow("dialog-other"));
    clickIn("dialog-other", 50, 700);
    int n = countClicks();
    snprintf(v, sizeof v, "%d clicks", n); v2[0] = 0;
    CHECK("D3 another client's window still takes clicks", n == 1);
    xdotool("windowraise %s", xwindow("dialog-test"));
    clickIn("dialog-test", 50, 700);
    sync_();
    hipe_instruction e; hipe_instruction_init(&e); int own = 0;
    while(hipe_next_instruction(s, &e, 0) > 0) { if(e.opcode == HIPE_OP_EVENT) own++; hipe_instruction_clear(&e); }
    snprintf(v, sizeof v, "%d mousedowns", own);
    CHECK("D3 the window with the dialog takes no clicks", own == 0);
    CHECK("D3 the dialog is still shown", shown("Dlg A"));

    /* ---- answers: the first dialog, then the one that waited ---- */
    answer("Dlg A", "key Down Return");
    uint64_t r = reply(HIPE_OP_DIALOG_RETURN);
    CHECK("D4 first dialog answered: choice and its number", r == 11 && !strcmp(v, "Pear") && !strcmp(v2, "2"));
    CHECK("D4 the waiting dialog is shown next", waitShown("Dlg B", 1));
    answer("Dlg B", "type --delay 40 hello");
    xdotool("key Return");
    r = reply(HIPE_OP_DIALOG_RETURN);
    CHECK("D4 text entry answered: text and 1", r == 12 && !strcmp(v, "hello") && !strcmp(v2, "1"));
    CHECK("D4 no dialog left", !shown("Dlg A") && !shown("Dlg B"));

    /* ---- no choices: an OK box, replied as cancelled ---- */
    hipe_send(s, HIPE_OP_DIALOG, 13, 0, 2, "Dlg C", "Just so you know");
    waitShown("Dlg C", 1);
    answer("Dlg C", "key Return");
    r = reply(HIPE_OP_DIALOG_RETURN);
    CHECK("D5 OK box replied with no choice and 0", r == 13 && !strcmp(v, "") && !strcmp(v2, "0"));

    /* ---- file dialog: cancelled, then the dialog queued behind it ---- */
    hipe_send(s, HIPE_OP_FIFO_GET_PEER, 14, 0, 3, "x.txt", "r", "Pick a file\ntxt:Text");
    hipe_send(s, HIPE_OP_DIALOG, 15, 0, 3, "Dlg D", "After the file", "Yes");
    CHECK("D6 instructions answered while the file dialog is open", sync_());
    CHECK("D6 the file dialog is shown", waitShown("Pick a file", 1));
    answer("Pick a file", "key Escape");
    r = reply(HIPE_OP_FIFO_RESPONSE);
    CHECK("D6 cancelled file dialog replied with no file and mode r", r == 14 && !strcmp(v, "") && !strcmp(v2, "r"));
    CHECK("D6 the dialog queued behind it is shown next", waitShown("Dlg D", 1));
    answer("Dlg D", "key Escape");
    r = reply(HIPE_OP_DIALOG_RETURN);
    CHECK("D6 cancelled dialog replied with no choice and 0", r == 15 && !strcmp(v, "") && !strcmp(v2, "0"));

    /* ---- a client killed with a dialog open and another waiting ---- */
    pid_t victim = startTopLevel(victimMain);
    CHECK("D7 the victim's window and dialog are shown", waitShown("dialog-victim", 1) && waitShown("Victim A", 1));
    kill(victim, SIGKILL); waitpid(victim, 0, 0);
    CHECK("D7 its dialog closes when it is killed", waitShown("Victim A", 0));
    CHECK("D7 its window closes when it is killed", waitShown("dialog-victim", 0));
    usleep(500000);
    CHECK("D7 its waiting dialog is never shown", !shown("Victim B"));
    CHECK("D7 hiped still serves instructions", sync_());

    kill(other, SIGTERM); waitpid(other, 0, 0);
    printf("%s dialogs: %d failed\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
