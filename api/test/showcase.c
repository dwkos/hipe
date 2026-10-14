/*  Copyright (c) 2026 Daniel Kos, General Development Systems

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

/* Showcase: "Layerline", a 3D printer console, driving a virtual printer. Made to show what a Hipe app can look like:
   a menu bar, toolbar and status bar, group boxes, buttons, sliders, checkboxes and radio buttons, a text field, a
   table, an SVG view of the toolpath that draws itself as the nozzle moves, a live SVG chart, a G-code console, a
   native dialog (Stop asks first), and optionally a video.
   The temperatures follow their targets, the jog pad moves the head (pause the print first), and the console takes
   G-code: G28, M104 S<t>, M140 S<t>, M106 S<0-255>, M107, M105, M114.
   Options: --camera <clip.mp4|.webm|.ogv> adds a Camera tab that plays the clip (looped, muted);
            --zoom <factor> scales the whole window (e.g. 1.5 for a sharp 1920x1080 screenshot of a 1280x720 layout). */

#include <hipe.h>
#include <math.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

static hipe_session s;

/* ---- the elements the program talks to; each control's events arrive with its own number as the requestor ---- */

#define NPATHS 14 /* toolpath moves in one layer: 2 outer perimeters, 2 bore perimeters, 10 infill rings */
enum { APP,
       B_HOME, B_PREHEAT, B_COOL, B_PAUSE, B_STOP,
       TAB_LAYER, TAB_CAMERA, VIEW_LAYER, VIEW_CAMERA, VIDEO,
       LAYER_TITLE, PATH0, NOZZLE = PATH0 + NPATHS,
       JOB_BAR, JOB_PCT, JOB_ELAPSED, JOB_LEFT, JOB_LAYER, JOB_STATE,
       T_HOT, T_BED, S_HOT, S_BED, V_HOT, V_BED, LINE_HOT, LINE_BED, TGT_HOT, TGT_BED,
       FAN, FAN_NUM, LIGHT, AUTOFAN,
       XP, XM, YP, YM, ZP, ZM, XYHOME, STEP0, STEP1, STEP2, POS_X, POS_Y, POS_Z,
       LOG, CMD, SEND,
       ST_STATE, ST_TEMPS, ST_ETA, NLOC };
static hipe_loc loc[NLOC];
#define L(x) (unsigned long long) loc[x]

/* ---- the virtual printer ---- */

#define LAYERS 142
#define LAYER_SECS 30.0
enum { IDLE, PRINTING, PAUSED };
static int state = PRINTING, layer = 37;
static double layer_t;                            /* seconds into the current layer */
static double hot = 209.6, bed = 59.9, hot_target = 210, bed_target = 60;
static int fan = 100, autofan = 1, step = 1;
static double x = 112.4, y = 96.0, z = 37 * 0.2;
static double hist_hot[120], hist_bed[120];
static int nhist;

/* the toolpath of one layer, in the layer view's coordinates (a 14-tooth gear) */
#define MAXPTS 160
static double px[NPATHS][MAXPTS], py[NPATHS][MAXPTS], plen[NPATHS];
static int npts[NPATHS];
static double total_len;

static void make_toolpath(void) {
    for (int p = 0; p < NPATHS; p++) {
        int n = 0;
        if (p < 2) { /* gear outline: teeth as a trapezoid wave */
            double inset = p * 4;
            for (int i = 0; i <= 14 * 10; i++) {
                double a = i * 2 * M_PI / 140, ph = fmod(i, 10) / 10.0, r;
                if (ph < .2) r = 134; else if (ph < .35) r = 134 + (ph - .2) / .15 * 22;
                else if (ph < .65) r = 156; else if (ph < .8) r = 156 - (ph - .65) / .15 * 22; else r = 134;
                px[p][n] = 200 + (r - inset) * cos(a); py[p][n] = 200 + (r - inset) * sin(a); n++;
            }
        } else { /* circles: the bore's two perimeters, then the infill rings outwards */
            double r = p < 4 ? 40 + (p - 2) * 4 : 52 + (p - 4) * 8.2;
            for (int i = 0; i <= 64; i++) {
                double a = i * 2 * M_PI / 64 + p;
                px[p][n] = 200 + r * cos(a); py[p][n] = 200 + r * sin(a); n++;
            }
        }
        npts[p] = n; plen[p] = 0;
        for (int i = 1; i < n; i++) plen[p] += hypot(px[p][i] - px[p][i - 1], py[p][i] - py[p][i - 1]);
        total_len += plen[p];
    }
}

/* ---- talking to the server ---- */

static int arg_int(hipe_instruction* e, int i) { /* an argument as a number (arguments aren't NUL-terminated) */
    char b[24]; snprintf(b, sizeof b, "%.*s", (int) e->arg_length[i], e->arg[i]); return atoi(b);
}

static void get_value(hipe_loc l, char* out, size_t size) { /* reads a control's live value */
    hipe_send(s, HIPE_OP_GET_ATTRIBUTE, 0, l, 1, "value");
    hipe_instruction r; hipe_instruction_init(&r);
    hipe_await_instruction(s, &r, HIPE_OP_ATTRIBUTE_RETURN);
    snprintf(out, size, "%.*s", (int) r.arg_length[1], r.arg[1]);
    hipe_instruction_clear(&r);
}

static int is_checked(hipe_loc l) {
    hipe_send(s, HIPE_OP_GET_ATTRIBUTE, 0, l, 1, "checked");
    hipe_instruction r; hipe_instruction_init(&r);
    hipe_await_instruction(s, &r, HIPE_OP_ATTRIBUTE_RETURN);
    int on = r.arg_length[1] > 0;
    hipe_instruction_clear(&r);
    return on;
}

static void text(int l, const char* fmt, ...) { /* sets an element's text, printf-style */
    char buf[512]; va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
    hipe_send(s, HIPE_OP_SET_TEXT, 0, loc[l], 1, buf);
}

static void attr(int l, const char* name, const char* fmt, ...) {
    char buf[4096]; va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
    hipe_send(s, HIPE_OP_SET_ATTRIBUTE, 0, loc[l], 2, name, buf);
}

static void set_class(int l, const char* cls, int on, int* state) { /* TOGGLE_CLASS, only on a change */
    if (on != *state) hipe_send(s, HIPE_OP_TOGGLE_CLASS, 0, loc[l], 1, cls);
    *state = on;
}

/* ---- the console ---- */

#define LOGLINES 10
static char logbuf[LOGLINES][96];
static int nlog;

static void say(const char* fmt, ...) {
    if (nlog == LOGLINES) { memmove(logbuf[0], logbuf[1], sizeof logbuf[0] * (LOGLINES - 1)); nlog--; }
    va_list ap; va_start(ap, fmt); vsnprintf(logbuf[nlog++], sizeof logbuf[0], fmt, ap); va_end(ap);
    char all[LOGLINES * 97] = "";
    for (int i = 0; i < nlog; i++) { strcat(all, logbuf[i]); if (i < nlog - 1) strcat(all, "\n"); }
    hipe_send(s, HIPE_OP_SET_TEXT, 0, loc[LOG], 1, all); /* text mode 0: shown exactly as written */
}

/* ---- updating the display ---- */

static void show_state(void) {
    static const char* names[] = { "Idle", "Printing", "Paused" };
    static int paused_cls, idle_cls;
    text(JOB_STATE, "%s", names[state]);
    text(ST_STATE, "%s", names[state]);
    text(B_PAUSE, state == PAUSED ? "\xe2\x96\xb6  Resume" : "\xe2\x9d\x9a\xe2\x9d\x9a  Pause");
    set_class(APP, "paused", state == PAUSED, &paused_cls);
    set_class(APP, "idle", state == IDLE, &idle_cls);
}

static void show_position(void) {
    text(POS_X, "%7.2f", x); text(POS_Y, "%7.2f", y); text(POS_Z, "%7.2f", z);
}

static double y_hot(double t) { return fmin(fmax(58 - (t - 195) * 52 / 30, 4), 58); } /* hotend band: 195-225 */
static double y_bed(double t) { return fmin(fmax(116 - (t - 45) * 52 / 30, 64), 116); } /* bed band: 45-75 */

static void show_targets(void) {
    text(V_HOT, "%.0f \xc2\xb0""C", hot_target); text(V_BED, "%.0f \xc2\xb0""C", bed_target);
    attr(TGT_HOT, "y1", "%.1f", y_hot(hot_target)); attr(TGT_HOT, "y2", "%.1f", y_hot(hot_target));
    attr(TGT_BED, "y1", "%.1f", y_bed(bed_target)); attr(TGT_BED, "y2", "%.1f", y_bed(bed_target));
}

static void show_layer(void) { /* where the nozzle has got to in the current layer */
    double done = layer_t / LAYER_SECS * total_len;
    for (int p = 0; p < NPATHS; p++) {
        double d = done > plen[p] ? plen[p] : done > 0 ? done : 0;
        attr(PATH0 + p, "stroke-dasharray", "%.1f %.1f", d, plen[p] + 1);
        if (done > 0 && done <= plen[p]) { /* the nozzle is on this path */
            double run = 0;
            for (int i = 1; i < npts[p]; i++) {
                double seg = hypot(px[p][i] - px[p][i - 1], py[p][i] - py[p][i - 1]);
                if (run + seg >= done) {
                    double f = (done - run) / seg;
                    double nx = px[p][i - 1] + f * (px[p][i] - px[p][i - 1]), ny = py[p][i - 1] + f * (py[p][i] - py[p][i - 1]);
                    attr(NOZZLE, "cx", "%.1f", nx); attr(NOZZLE, "cy", "%.1f", ny);
                    x = 110 + (nx - 200) * 0.25; y = 110 - (ny - 200) * 0.25;
                    break;
                }
                run += seg;
            }
        }
        done -= plen[p];
    }
    double pct = (layer - 1 + layer_t / LAYER_SECS) / LAYERS * 100;
    char w[16]; snprintf(w, sizeof w, "%.1f%%", pct);
    hipe_send(s, HIPE_OP_SET_STYLE, 0, loc[JOB_BAR], 2, "width", w);
    text(JOB_PCT, "%.1f%%", pct);
}

static void show_job(void) {
    int el = (int) ((layer - 1) * LAYER_SECS + layer_t), left = (int) ((LAYERS - layer + 1) * LAYER_SECS - layer_t);
    text(LAYER_TITLE, "Layer %d of %d", layer, LAYERS);
    text(JOB_LAYER, "%d / %d", layer, LAYERS);
    text(JOB_ELAPSED, "%d:%02d:%02d", el / 3600, el / 60 % 60, el % 60);
    text(JOB_LEFT, state == IDLE ? "\xe2\x80\x94" : "%d:%02d:%02d", left / 3600, left / 60 % 60, left % 60);
    text(ST_ETA, state == PRINTING ? "Finishes in %d min" : "", (left + 59) / 60);
}

static void show_temps(void) {
    char a[2400], b[2400]; int na = 0, nb = 0;
    for (int i = 0; i < nhist; i++) {
        double cx = 300.0 * i / 119;
        na += snprintf(a + na, sizeof a - na, "%.1f,%.1f ", cx, y_hot(hist_hot[i]));
        nb += snprintf(b + nb, sizeof b - nb, "%.1f,%.1f ", cx, y_bed(hist_bed[i]));
    }
    attr(LINE_HOT, "points", "%s", a); attr(LINE_BED, "points", "%s", b);
    text(T_HOT, "%.1f", hot); text(T_BED, "%.1f", bed);
    text(ST_TEMPS, "Hotend %.1f \xc2\xb0""C    Bed %.1f \xc2\xb0""C", hot, bed);
}

/* ---- simulation steps ---- */

static void every_second(void) {
    hot += (hot_target - hot) * 0.08 + (rand() % 100 - 50) / 160.0 * (hot_target > 0);
    bed += (bed_target - bed) * 0.03 + (rand() % 100 - 50) / 400.0 * (bed_target > 0);
    if (hot_target == 0) hot += (22 - hot) * 0.02;
    if (bed_target == 0) bed += (22 - bed) * 0.01;
    if (nhist == 120) { memmove(hist_hot, hist_hot + 1, sizeof hist_hot - sizeof *hist_hot);
                        memmove(hist_bed, hist_bed + 1, sizeof hist_bed - sizeof *hist_bed); nhist--; }
    hist_hot[nhist] = hot; hist_bed[nhist] = bed; nhist++;
    show_temps();
    static int n;
    if (++n % 4 == 0) say("ok T:%.1f /%.1f B:%.1f /%.1f @:%d", hot, hot_target, bed, bed_target, (int) (hot_target - hot) * 8 + 64);
    show_job();
}

static void advance(double dt) {
    if (state != PRINTING) return;
    layer_t += dt;
    if (layer_t >= LAYER_SECS) {
        layer_t = 0;
        if (layer == LAYERS) { state = IDLE; say("; print finished"); show_state(); return; }
        layer++; z = layer * 0.2;
        say(";LAYER:%d", layer - 1);
        say("G1 Z%.2f F600", z);
        if (autofan) {
            fan = layer < 3 ? 0 : 100;
            attr(FAN, "value", "%d", fan);
        }
    }
    show_layer();
    text(POS_Z, "%7.2f", z); text(POS_X, "%7.2f", x); text(POS_Y, "%7.2f", y);
}

/* ---- G-code typed into the console ---- */

static void gcode(const char* line) {
    char c[96]; int n = 0;
    for (const char* p = line; *p && n < 95; p++) c[n++] = (*p >= 'a' && *p <= 'z') ? *p - 32 : *p;
    c[n] = 0;
    say("> %s", c);
    const char* sv = strchr(c, 'S');
    double v = sv ? atof(sv + 1) : 0;
    if (!strncmp(c, "G28", 3)) {
        if (state == PRINTING) { say("// busy: pause the print first"); return; }
        x = y = 0; z = 0; show_position();
    } else if (!strncmp(c, "M104", 4) && sv) { hot_target = fmin(v, 260); attr(S_HOT, "value", "%.0f", hot_target); }
    else if (!strncmp(c, "M140", 4) && sv) { bed_target = fmin(v, 110); attr(S_BED, "value", "%.0f", bed_target); }
    else if (!strncmp(c, "M106", 4)) { fan = sv ? (int) (fmin(v, 255) / 2.55 + .5) : 100; attr(FAN, "value", "%d", fan); }
    else if (!strncmp(c, "M107", 4)) { fan = 0; attr(FAN, "value", "0"); }
    else if (c[0] == 0) return;
    else if (strncmp(c, "M105", 4) && strncmp(c, "M114", 4)) { say("echo:Unknown command: \"%s\"", c); return; }
    if (!strncmp(c, "M114", 4)) say("X:%.2f Y:%.2f Z:%.2f E:0.00", x, y, z);
    show_targets();
    say("ok");
}

/* ---- building the window ---- */

static char* markup; static size_t used, cap;
static void add(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    used += vsnprintf(markup + used, cap - used, fmt, ap);
    va_end(ap);
}

static void build(int camera) {
    cap = 65536; markup = malloc(cap); used = 0;
    add("<div id='app' hipe-loc='%llu'>", L(APP));
    add("<div class='menubar'><span>File</span><span>Edit</span><span>Printer</span><span>View</span><span>Help</span></div>");

    add("<div class='toolbar'><div class='conn'><span class='lamp'></span>Virtual printer<span class='dim'>  \xc2\xb7  "
        "250 000 baud</span></div>");
    add("<button hipe-loc='%llu'>\xe2\x8c\x82  Home</button>", L(B_HOME));
    add("<button hipe-loc='%llu'>Preheat PLA</button>", L(B_PREHEAT));
    add("<button hipe-loc='%llu'>Cool down</button><span class='gap'></span>", L(B_COOL));
    add("<button class='pause' hipe-loc='%llu'></button><button class='stop' hipe-loc='%llu'>\xe2\x96\xa0  Stop</button></div>",
        L(B_PAUSE), L(B_STOP));

    add("<div class='main'><div class='left'>");
    /* the layer view, and the camera if there is one */
    add("<div class='box view'><div class='title' hipe-loc='%llu'></div>", L(LAYER_TITLE));
    if (camera)
        add("<div class='tabs'><span class='on' hipe-loc='%llu'>Layer</span><span hipe-loc='%llu'>Camera</span></div>",
            L(TAB_LAYER), L(TAB_CAMERA));
    add("<div class='plate show' hipe-loc='%llu'><svg viewBox='0 0 400 400'>"
        "<defs><pattern id='grid' width='20' height='20' patternUnits='userSpaceOnUse'>"
        "<path d='M20 0H0V20' fill='none' stroke='#2b3138' stroke-width='1'/></pattern></defs>"
        "<rect x='-1000' y='-1000' width='2400' height='2400' fill='url(#grid)'/>", L(VIEW_LAYER));
    add("<path class='ghost' d='");
    for (int p = 0; p < NPATHS; p++)
        for (int i = 0; i < npts[p]; i++) add("%c%.1f %.1f", i ? 'L' : 'M', px[p][i], py[p][i]);
    add("'/>");
    for (int p = 0; p < NPATHS; p++) {
        add("<polyline class='%s' hipe-loc='%llu' stroke-dasharray='0 9999' points='", p < 4 ? "wall" : "fill", L(PATH0 + p));
        for (int i = 0; i < npts[p]; i++) add("%.1f,%.1f ", px[p][i], py[p][i]);
        add("'/>");
    }
    add("<circle class='nozzle' hipe-loc='%llu' cx='200' cy='200' r='5'/></svg></div>", L(NOZZLE));
    if (camera)
        add("<div class='plate cam' hipe-loc='%llu'><video loop muted hipe-loc='%llu'></video></div>", L(VIEW_CAMERA), L(VIDEO));
    add("</div>");

    /* the job */
    add("<div class='box job'><div class='title'>Job</div><div class='jobrow'><div class='file'>gear_14t.gcode"
        "<div class='dim'>PLA \xc2\xb7 0.2 mm layers \xc2\xb7 0.4 mm nozzle</div></div>"
        "<div class='state' hipe-loc='%llu'></div></div>", L(JOB_STATE));
    add("<div class='bar'><div hipe-loc='%llu'></div></div>", L(JOB_BAR));
    add("<table><tr><td>Progress</td><td>Layer</td><td>Elapsed</td><td>Remaining</td></tr>"
        "<tr class='big'><td hipe-loc='%llu'></td><td hipe-loc='%llu'></td><td hipe-loc='%llu'></td><td hipe-loc='%llu'></td></tr>"
        "</table></div>", L(JOB_PCT), L(JOB_LAYER), L(JOB_ELAPSED), L(JOB_LEFT));
    add("</div><div class='right'>");

    /* temperatures */
    add("<div class='box'><div class='title'>Temperatures</div><table class='temps'>"
        "<tr><td><span class='key hot'></span>Hotend</td><td class='now' hipe-loc='%llu'></td>"
        "<td><input type='range' min='0' max='260' value='210' class='hotslider' hipe-loc='%llu'></td><td class='tgt' hipe-loc='%llu'></td></tr>"
        "<tr><td><span class='key bed'></span>Bed</td><td class='now' hipe-loc='%llu'></td>"
        "<td><input type='range' min='0' max='110' value='60' class='bedslider' hipe-loc='%llu'></td><td class='tgt' hipe-loc='%llu'></td></tr>"
        "</table>", L(T_HOT), L(S_HOT), L(V_HOT), L(T_BED), L(S_BED), L(V_BED));
    add("<div class='chartwrap'><span class='ax' style='top:2%%'>225\xc2\xb0</span><span class='ax' style='top:38%%'>195\xc2\xb0</span><span class='ax' style='top:51%%'>75\xc2\xb0</span><span class='ax' style='top:87%%'>45\xc2\xb0</span>");
    add("<svg class='chart' viewBox='0 0 300 120' preserveAspectRatio='none'>"
        "<line x1='0' x2='300' y1='31' y2='31' class='grid'/><line x1='0' x2='300' y1='61' y2='61' class='sep'/><line x1='0' x2='300' y1='90' y2='90' class='grid'/>"
        "<line x1='0' x2='300' class='tgt hot' hipe-loc='%llu'/><line x1='0' x2='300' class='tgt bed' hipe-loc='%llu'/>"
        "<polyline class='hot' hipe-loc='%llu'/><polyline class='bed' hipe-loc='%llu'/></svg></div></div>",
        L(TGT_HOT), L(TGT_BED), L(LINE_HOT), L(LINE_BED));

    /* move, fan and light */
    add("<div class='pair'><div class='box move'><div class='title'>Move</div><div class='jog'>"
        "<div class='pad'><span></span><button hipe-loc='%llu'>Y+</button><span></span>"
        "<button hipe-loc='%llu'>X\xe2\x88\x92</button><button hipe-loc='%llu'>\xe2\x8c\x82</button><button hipe-loc='%llu'>X+</button>"
        "<span></span><button hipe-loc='%llu'>Y\xe2\x88\x92</button><span></span></div>"
        "<div class='zcol'><button hipe-loc='%llu'>Z+</button><button hipe-loc='%llu'>Z\xe2\x88\x92</button></div></div>",
        L(YP), L(XM), L(XYHOME), L(XP), L(YM), L(ZP), L(ZM));
    add("<div class='steps'>Step <label><input type='radio' name='step' hipe-loc='%llu'> 0.1</label>"
        "<label><input type='radio' name='step' checked hipe-loc='%llu'> 1</label>"
        "<label><input type='radio' name='step' hipe-loc='%llu'> 10 mm</label></div>", L(STEP0), L(STEP1), L(STEP2));
    add("<div class='lcd'><span>X</span><b hipe-loc='%llu'></b><span>Y</span><b hipe-loc='%llu'></b>"
        "<span>Z</span><b hipe-loc='%llu'></b></div></div>", L(POS_X), L(POS_Y), L(POS_Z));
    add("<div class='box fan'><div class='title'>Cooling &amp; light</div>"
        "<div class='fanrow'><span>Part fan</span><b hipe-loc='%llu'>100%%</b></div><input type='range' class='fanslider' min='0' max='100' value='100' hipe-loc='%llu'>"
        "<label><input type='checkbox' checked hipe-loc='%llu'> Fan off for first layers</label>"
        "<label><input type='checkbox' hipe-loc='%llu'> Enclosure light</label></div></div>",
        L(FAN_NUM), L(FAN), L(AUTOFAN), L(LIGHT));

    /* console */
    add("<div class='box console'><div class='title'>Console</div><pre hipe-loc='%llu'></pre>"
        "<div class='cmdrow'><input type='text' placeholder='G-code, e.g. M104 S215' hipe-loc='%llu'>"
        "<button hipe-loc='%llu'>Send</button></div></div>", L(LOG), L(CMD), L(SEND));
    add("</div></div>");

    add("<div class='status'><span hipe-loc='%llu'></span><span class='sep'></span><span hipe-loc='%llu'></span>"
        "<span class='grow'></span><span hipe-loc='%llu'></span></div></div>", L(ST_STATE), L(ST_TEMPS), L(ST_ETA));

    hipe_loc listed[NLOC]; int n = 0; /* the locations the markup uses: without a camera, not the camera's */
    for (int i = 0; i < NLOC; i++) if (camera || i < TAB_LAYER || i > VIDEO) listed[n++] = loc[i];
    hipe_send_markup(s, 0, markup, 1, listed, n);
    free(markup);
}

static const char* css =
"html, body { height:100%; margin:0; overflow:hidden; }\n"
"#app { display:flex; flex-direction:column; height:100%; background:#1e2125; color:#d7dce2; cursor:default;\n"
"       font:13px Cantarell,'DejaVu Sans',sans-serif; }\n"
".menubar { display:flex; background:#2a2e33; border-bottom:1px solid #15171a; padding:0 4px; flex:none; }\n"
".menubar span { padding:5px 10px; }\n"
".menubar span:hover { background:#3b4148; }\n"
".toolbar { display:flex; align-items:center; padding:6px 8px; background:#25292e; border-bottom:1px solid #15171a; flex:none; }\n"
".toolbar button { margin-right:6px; }\n"
".toolbar .gap { flex:1; }\n"
".conn { margin:0 18px 0 4px; }\n"
".lamp { display:inline-block; width:9px; height:9px; border-radius:50%; background:#4cd964; margin-right:8px;\n"
"        box-shadow:0 0 6px #4cd964; }\n"
".dim { color:#8a939d; }\n"
"button { font:inherit; color:inherit; padding:5px 12px; border-radius:4px; border:1px solid #121417;\n"
"        background:linear-gradient(#3d434a,#30353b); box-shadow:inset 0 1px 0 rgba(255,255,255,.08); }\n"
"button:active { background:#2a2e33; }\n"
"button.pause { min-width:92px; }\n"
"button.stop { background:linear-gradient(#b8432f,#9a3423); color:#fff; }\n"
"#app.idle button.pause, #app.idle button.stop { opacity:.4; }\n"
".main { flex:1; display:flex; min-height:0; padding:4px; }\n"
".left { flex:1; display:flex; flex-direction:column; min-width:0; }\n"
".right { width:470px; flex:none; display:flex; flex-direction:column; }\n"
".box { position:relative; border:1px solid #3a4047; border-radius:5px; margin:12px 6px 6px; padding:14px 12px 10px; }\n"
".box > .title { position:absolute; top:-9px; left:10px; padding:0 6px; background:#1e2125; color:#e8a33d; font-weight:bold; }\n"
".view { flex:1; display:flex; flex-direction:column; min-height:0; }\n"
".tabs { position:absolute; top:-11px; right:10px; background:#1e2125; padding:0 4px; }\n"
".tabs span { display:inline-block; padding:2px 10px; border:1px solid #3a4047; border-radius:4px; margin-left:4px; background:#25292e; }\n"
".tabs span.on { background:#3d434a; color:#fff; }\n"
".plate { flex:1; position:relative; min-height:0; display:none; background:#16191c; border-radius:3px; }\n"
".plate.show { display:block; }\n"
"#app.lit .plate { background:#2f3a42; }\n"
".plate svg, .plate video { position:absolute; left:0; top:0; width:100%; height:100%; }\n"
".ghost { fill:none; stroke:#2f363d; stroke-width:3; }\n"
".wall, .fill { fill:none; stroke:#e8a33d; stroke-width:4; stroke-linejoin:round; }\n"
".fill { stroke:#c7862c; stroke-width:3.4; }\n"
".nozzle { fill:#fff; stroke:#e8a33d; stroke-width:2; }\n"
"#app.paused .nozzle { fill:#e8a33d; }\n"
".job { flex:none; }\n"
".jobrow { display:flex; align-items:flex-start; }\n"
".file { flex:1; font-size:15px; }\n"
".state { padding:2px 10px; border-radius:10px; background:#2f5d3a; color:#bff0c8; }\n"
"#app.paused .state { background:#6b5320; color:#ffe2a6; }\n"
"#app.idle .state { background:#3a4047; color:#d7dce2; }\n"
".bar { height:10px; background:#15171a; border:1px solid #3a4047; border-radius:5px; margin:10px 0 6px; overflow:hidden; }\n"
".bar div { height:100%; background:repeating-linear-gradient(-45deg,#e8a33d,#e8a33d 8px,#d18f2b 8px,#d18f2b 16px); }\n"
"table { border-collapse:collapse; width:100%; color:inherit; font:inherit; }\n"
".job td { color:#8a939d; padding:0 4px; width:25%; }\n"
".job tr.big td { color:#d7dce2; font-size:20px; font-family:'DejaVu Sans Mono',monospace; }\n"
".temps td { padding:3px 4px; white-space:nowrap; }\n"
".temps .now { font:20px 'DejaVu Sans Mono',monospace; width:84px; text-align:right; padding-right:12px; }\n"
".temps .tgt { color:#b6bec7; width:56px; text-align:right; }\n"
".temps input { width:100%; }\n"
".key { display:inline-block; width:10px; height:10px; border-radius:2px; margin-right:8px; }\n"
".key.hot { background:#ff6a3d; }\n"
".key.bed { background:#3da5ff; }\n"
".chartwrap { position:relative; margin-top:8px; }\n"
".chart { display:block; width:100%; height:120px; background:#16191c; border-radius:3px; }\n"
".ax { position:absolute; right:6px; font-size:10px; color:#56606a; }\n"
".chart polyline { fill:none; stroke-width:2; vector-effect:non-scaling-stroke; }\n"
".chart .hot { stroke:#ff6a3d; }\n"
".chart .bed { stroke:#3da5ff; }\n"
".chart line { stroke-width:1; vector-effect:non-scaling-stroke; }\n"
".chart .grid { stroke:#232830; }\n"
".chart .sep { stroke:#3a4047; }\n"
".chart line.tgt { stroke-dasharray:4 4; opacity:.6; }\n"
".chart line.tgt.hot { stroke:#ff6a3d; }\n"
".chart line.tgt.bed { stroke:#3da5ff; }\n"
"input[type=range] { -webkit-appearance:none; appearance:none; height:6px; border-radius:3px; background:#15171a;\n"
"        border:1px solid #3a4047; }\n"
"input[type=range]::-webkit-slider-thumb { -webkit-appearance:none; appearance:none; width:14px; height:14px;\n"
"        border-radius:3px; border:1px solid #121417; background:linear-gradient(#e6e9ec,#b9c0c7); }\n"
"input[type=checkbox], input[type=radio] { color:#e8a33d; }\n"
".pair { display:flex; flex:none; }\n"
".pair .box { flex:1; }\n"
".move { flex:none !important; }\n"
".jog { display:flex; align-items:center; }\n"
".pad { width:132px; line-height:0; }\n"
".pad > * { display:inline-block; vertical-align:top; width:40px; height:30px; margin:0 4px 4px 0; padding:0;\n"
"        line-height:28px; text-align:center; box-sizing:border-box; }\n"
".zcol button { display:block; width:40px; height:30px; padding:0; margin:0 0 4px 10px; }\n"
".steps { margin:4px 0 8px; }\n"
".steps label { margin-left:8px; }\n"
".lcd { font:13px 'DejaVu Sans Mono',monospace; background:#0f1a12; color:#7dff9a; border:1px solid #2a3a2e;\n"
"       border-radius:3px; padding:5px 8px; white-space:nowrap; text-shadow:0 0 4px rgba(125,255,154,.5); }\n"
".lcd span { color:#4a8a5a; margin-left:6px; }\n"
".lcd span:first-child { margin-left:0; }\n"
".fan label { display:block; margin-top:8px; }\n"
".fanrow { display:flex; }\n"
".fanrow span { flex:1; }\n"
".fanrow b { font:13px 'DejaVu Sans Mono',monospace; }\n"
".fanslider { display:block; width:100%; box-sizing:border-box; margin:8px 0 4px; }\n"
".console { flex:1; display:flex; flex-direction:column; min-height:0; }\n"
".console pre { flex:1; margin:0; overflow:hidden; display:flex; flex-direction:column; justify-content:flex-end; font:12px 'DejaVu Sans Mono',monospace; color:#b6c2cf;\n"
"       background:#121417; border:1px solid #2a2f35; border-radius:3px; padding:6px 8px; }\n"
".cmdrow { display:flex; margin-top:6px; }\n"
".cmdrow input { flex:1; font:12px 'DejaVu Sans Mono',monospace; color:inherit; background:#121417; border:1px solid #3a4047;\n"
"       border-radius:3px; padding:5px 8px; margin-right:6px; outline:none; }\n"
".cmdrow input:focus { border-color:#e8a33d; }\n"
".status { display:flex; align-items:center; flex:none; height:24px; padding:0 10px; background:#2a2e33;\n"
"       border-top:1px solid #15171a; color:#a9b2bc; font-size:12px; }\n"
".status .sep { width:1px; height:12px; background:#454c54; margin:0 12px; }\n"
".status .grow { flex:1; }\n";

/* ---- events ---- */

static int light_cls;

static void jog(double dx, double dy, double dz) {
    if (state == PRINTING) { say("// busy: pause the print to jog"); return; }
    double st = step == 0 ? 0.1 : step == 1 ? 1 : 10;
    x = fmin(fmax(x + dx * st, 0), 220); y = fmin(fmax(y + dy * st, 0), 220); z = fmin(fmax(z + dz * st, 0), 250);
    say("G91");
    say("G1 %s%+.1f F3000", dx ? "X" : dy ? "Y" : "Z", (dx + dy + dz) * st);
    say("G90");
    show_position();
}

static void handle(hipe_instruction* e) {
    char v[128];
    switch (e->requestor) {
    case B_HOME: gcode("G28"); break;
    case B_PREHEAT: gcode("M104 S210"); gcode("M140 S60"); break;
    case B_COOL: gcode("M104 S0"); gcode("M140 S0"); break;
    case B_PAUSE:
        if (state == IDLE) break;
        state = state == PAUSED ? PRINTING : PAUSED;
        say(state == PAUSED ? "M25 ; pause" : "M24 ; resume");
        show_state();
        break;
    case B_STOP: /* a native dialog asks first; its answer arrives as HIPE_OP_DIALOG_RETURN */
        if (state != IDLE)
            hipe_send(s, HIPE_OP_DIALOG, B_STOP, 0, 4, "Stop the print?",
                      "The print will be cancelled and the hotend and bed turned off.", "Stop print", "\xe2\x96\xa0\n\xe2\x9a\xa0");
        break;
    case TAB_LAYER: case TAB_CAMERA: {
        static int cam;
        if ((e->requestor == TAB_CAMERA) == cam) break;
        cam = !cam;
        int l[] = { TAB_LAYER, TAB_CAMERA, VIEW_LAYER, VIEW_CAMERA };
        for (int i = 0; i < 4; i++) hipe_send(s, HIPE_OP_TOGGLE_CLASS, 0, loc[l[i]], 1, i < 2 ? "on" : "show");
        break; }
    case VIDEO: /* "canplay": start the camera clip */
        hipe_send(s, HIPE_OP_AUDIOVIDEO_STATE, 0, loc[VIDEO], 4, "", "", "1", "");
        break;
    case S_HOT: get_value(loc[S_HOT], v, sizeof v); hot_target = atof(v); show_targets(); break;
    case S_BED: get_value(loc[S_BED], v, sizeof v); bed_target = atof(v); show_targets(); break;
    case FAN: get_value(loc[FAN], v, sizeof v); fan = atoi(v); text(FAN_NUM, "%d%%", fan); break;
    case AUTOFAN: autofan = is_checked(loc[AUTOFAN]); break;
    case LIGHT: set_class(APP, "lit", is_checked(loc[LIGHT]), &light_cls); break;
    case XP: jog(1, 0, 0); break;
    case XM: jog(-1, 0, 0); break;
    case YP: jog(0, 1, 0); break;
    case YM: jog(0, -1, 0); break;
    case ZP: jog(0, 0, 1); break;
    case ZM: jog(0, 0, -1); break;
    case XYHOME: gcode("G28"); break;
    case STEP0: case STEP1: case STEP2: step = e->requestor - STEP0; break;
    case CMD: /* keydown: only Enter sends */
        if (arg_int(e, 1) != 13) break;
        /* fall through */
    case SEND:
        get_value(loc[CMD], v, sizeof v);
        gcode(v);
        attr(CMD, "value", "");
        break;
    }
}

static double now_s(void) { struct timeval tv; gettimeofday(&tv, 0); return tv.tv_sec + tv.tv_usec / 1e6; }

int main(int argc, char** argv) {
    const char* camera = 0; const char* key = 0; const char* zoom = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--camera") && i + 1 < argc) camera = argv[++i];
        else if (!strcmp(argv[i], "--zoom") && i + 1 < argc) zoom = argv[++i];
        else key = argv[i];
    }
    s = hipe_open_session(key, 0, 0, "Layerline");
    if (!s) exit(1);
    hipe_set_buffered(s, 1);
    make_toolpath();

    hipe_send(s, HIPE_OP_APPEND_TAG, 0, 0, 1, "style");
    hipe_send(s, HIPE_OP_SET_TEXT, 0, hipe_newest_location(), 1, css); /* text mode 0: the stylesheet as written */
    if (zoom) {
        char z[32]; snprintf(z, sizeof z, "zoom:%s;", zoom);
        hipe_send(s, HIPE_OP_ADD_STYLE_RULE, 0, 0, 2, "html", z);
    }

    hipe_loc first = hipe_reserve_locations(s, NLOC);
    for (int i = 0; i < NLOC; i++) loc[i] = first + i;
    build(camera != 0);
    if (!camera) { /* the camera's elements weren't made: give their locations back */
        char list[64]; snprintf(list, sizeof list, "%llu-%llu", L(TAB_LAYER), L(VIDEO));
        hipe_send(s, HIPE_OP_FREE_LOCATION, 0, 0, 1, list);
    }

    /* two minutes of history, so the window opens mid-print: the hotend was raised from 205 to 210 a minute ago */
    double h = 205, rate = 0;
    for (int i = 0; i < 120; i++) {
        double target = i < 58 ? 205 : 210;
        rate = rate * .82 + (target - h) * .045; /* a heater with a little overshoot */
        h += rate;
        hist_hot[i] = h + (rand() % 100 - 50) / 220.0;
        hist_bed[i] = 60 + sin(i * .3) * .15 + (rand() % 100 - 50) / 500.0;
    }
    nhist = 120; hot = hist_hot[119];
    layer_t = 15.5;
    show_state(); show_targets(); show_temps(); show_layer(); show_job(); show_position();
    say("ok T:205.1 /205.0 B:60.0 /60.0 @:58");
    say(";LAYER:35");
    say("G1 Z7.20 F600");
    say("ok T:204.9 /205.0 B:59.9 /60.0 @:61");
    say("> M104 S210");
    say("ok");
    say("ok T:207.6 /210.0 B:60.0 /60.0 @:96");
    say("ok T:210.6 /210.0 B:59.9 /60.0 @:55");
    say(";LAYER:36");
    say("G1 Z7.40 F600");
    say("ok T:210.1 /210.0 B:60.1 /60.0 @:63");

    if (camera) { /* load the clip; "canplay" says when it can start */
        FILE* f = fopen(camera, "rb");
        if (f) {
            fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
            char* data = malloc(n);
            if (fread(data, 1, n, f) == (size_t) n) {
                const char* ext = strrchr(camera, '.');
                const char* mime = ext && !strcmp(ext, ".webm") ? "video/webm" : ext && !strcmp(ext, ".ogv") ? "video/ogg" : "video/mp4";
                hipe_send(s, HIPE_OP_EVENT_REQUEST, VIDEO, loc[VIDEO], 1, "canplay");
                hipe_instruction si; hipe_instruction_init(&si);
                si.opcode = HIPE_OP_SET_SRC; si.location = loc[VIDEO];
                si.arg[0] = data; si.arg_length[0] = n;
                si.arg[1] = (char*) mime; si.arg_length[1] = strlen(mime);
                hipe_send_instruction(s, si);
            }
            free(data); fclose(f);
        }
        hipe_send(s, HIPE_OP_EVENT_REQUEST, TAB_LAYER, loc[TAB_LAYER], 1, "click");
        hipe_send(s, HIPE_OP_EVENT_REQUEST, TAB_CAMERA, loc[TAB_CAMERA], 1, "click");
    }

    int clicks[] = { B_HOME, B_PREHEAT, B_COOL, B_PAUSE, B_STOP, XP, XM, YP, YM, ZP, ZM, XYHOME, SEND };
    for (int i = 0; i < (int) (sizeof clicks / sizeof *clicks); i++)
        hipe_send(s, HIPE_OP_EVENT_REQUEST, clicks[i], loc[clicks[i]], 1, "click");
    int inputs[] = { S_HOT, S_BED, FAN };
    for (int i = 0; i < 3; i++) hipe_send(s, HIPE_OP_EVENT_REQUEST, inputs[i], loc[inputs[i]], 1, "input");
    int changes[] = { AUTOFAN, LIGHT, STEP0, STEP1, STEP2 };
    for (int i = 0; i < 5; i++) hipe_send(s, HIPE_OP_EVENT_REQUEST, changes[i], loc[changes[i]], 1, "change");
    hipe_send(s, HIPE_OP_EVENT_REQUEST, CMD, loc[CMD], 1, "keydown");
    hipe_flush(s);

    double last = now_s(), second = last;
    hipe_instruction e; hipe_instruction_init(&e);
    for (;;) {
        short r;
        while ((r = hipe_next_instruction(s, &e, 0)) > 0) {
            if (e.opcode == HIPE_OP_FRAME_CLOSE || e.opcode == HIPE_OP_SERVER_DENIED) return 0;
            if (e.opcode == HIPE_OP_EVENT) handle(&e);
            if (e.opcode == HIPE_OP_DIALOG_RETURN && e.requestor == B_STOP && arg_int(&e, 1) == 1) {
                state = IDLE; layer_t = 0;
                say("M524 ; abort print");
                gcode("M104 S0"); gcode("M140 S0");
                show_state(); show_layer(); show_job();
            }
        }
        if (r < 0) return 0;
        double t = now_s();
        advance(t - last); last = t;
        if (t - second >= 1) { every_second(); second += 1; }
        hipe_flush(s);
        struct pollfd p = { hipe_session_fd(s), POLLIN, 0 };
        poll(&p, 1, 120);
    }
}
