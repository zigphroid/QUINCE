// QUINCE - Quake, ported to PectinOS.
//
// A full-screen program, cut down from LARDER ($PECTIN_ROOT/apps/examples/
// larder.c), the model one: the gingham title bar on row 0, a status
// line with the keys on the last row, Esc to leave, and one wait for the
// keys. When it grows a list, a text field, a question or a find bar,
// take them from LARDER and sdk/ui.h (add $(SDK)/lib/ui.c to SDKLIB in
// the Makefile) rather than writing new ones, so the keys behave as they
// do in every other program ($PECTIN_ROOT/docs/HUMAN-INTERFACE.md §4, §6).
//
// The part worth testing is in lib/ (no kernel calls, so it builds on the
// host too). The screen is drawn whole after each key, from the program's
// own state: simpler than drawing only what changed, and quick enough at
// 80x30.
#include <pectin.h>

#include "quince_core.h"

#define COLS_MAX    160
#define A_TITLE     PECTIN_ATTR(PECTIN_BLACK, PECTIN_WHITE)
#define A_TEXT      PECTIN_ATTR_DEFAULT
#define A_STATUS    PECTIN_ATTR(PECTIN_BLACK, PECTIN_AMBER)

static int cols = 80, rows = 30;
static uint16_t line[COLS_MAX];         // one row's cells: attr << 8 | character
static const char *who;                 // the NAME on the line, or NULL

static int put_text(int col, const char *s, uint8_t attr)
{
    for (; *s && col < cols; s++)
        line[col++] = (uint16_t)(attr << 8 | (uint8_t)*s);
    return col;
}

static void blank(uint8_t attr)
{
    for (int c = 0; c < cols; c++)
        line[c] = (uint16_t)(attr << 8 | ' ');
}

static void draw(void)
{
    char text[COLS_MAX];
    int len;

    // Row 0: a 0 cell there is gingham, with the title in the middle.
    snprintf(text, sizeof text, " QUINCE ");
    len = (int)strlen(text);
    for (int c = 0; c < cols; c++)
        line[c] = 0;
    put_text((cols - len) / 2, text, A_TITLE);
    con_cells(0, 0, line, (uint32_t)cols);

    for (int r = 1; r < rows - 1; r++) {
        blank(A_TEXT);
        if (r == rows / 2) {
            quince_greeting(text, sizeof text, who);
            len = (int)strlen(text);
            put_text(len < cols ? (cols - len) / 2 : 0, text, A_TEXT);
        }
        con_cells(0, r, line, (uint32_t)cols);
    }

    blank(A_STATUS);
    put_text(1, "Esc leave", A_STATUS);
    con_cells(0, rows - 1, line, (uint32_t)cols);
    con_cursor(false);
}

// False to leave.
static bool do_key(int k)
{
    // Another burner in front, or this one back: nothing to save yet. A
    // program with unsaved work saves it on BACKGROUND, since it may be
    // ended for room while behind ($PECTIN_ROOT/sdk/README.md, "Saved
    // behind another burner").
    if (k == PECTIN_KEY_BACKGROUND || k == PECTIN_KEY_FOREGROUND)
        return true;
    return k != PECTIN_KEY_ESC;
}

static void screen(void)
{
    int w, h;
    con_size(&w, &h, NULL);
    if (w > 0 && w <= COLS_MAX)
        cols = w;
    if (h > 8)
        rows = h;
    set_break(false);                   // Ctrl+C is Copy in a full-screen program; Ctrl+Esc still stops it
    clear_screen();                     // the first full-screen call: the screen is ours
    for (;;) {
        draw();
        // getkey sleeps until a key: nothing else changes the screen, and
        // the other burners get the time. For the mouse as well, wait in
        // pectin_wait with PECTIN_WAIT_ON_MOUSE, as LARDER does.
        if (!do_key(getkey()))
            break;
    }
    con_cursor(true);
    clear_screen();
}

static void fail(const char *what, const char *got, const char *want)
{
    eprintf("FAIL: %s: got \"%s\", want \"%s\"\n", what, got, want);
}

// The host test's checks, on CANNER's code: what make sim-smoke runs.
static int selftest(void)
{
    int ran;
    int failed = quince_selftest(fail, &ran);

    if (failed) {
        eprintf("selftest: %d of %d checks failed\n", failed, ran);
        return 1;
    }
    printf("selftest: %d checks OK\n", ran);
    return 0;
}

// M1's timings (docs/ROADMAP.md): Quake over the null GL, in
// quince/sys_quince.c and lib/qbench.c. Quake's files are in QUINCE's
// LARDER jar, as the store's programs keep theirs: J:\LARDER\QUINCE\ID1\PAK0.PAK.
#define QUAKE_BASEDIR "J:\\LARDER\\QUINCE"

int quince_bench(int argc, char **argv);

static int bench(const struct pectin_args *a)
{
    static char frames[12], mem[12];
    char *words[8];
    int n = 0;
    words[n++] = QUAKE_BASEDIR;
    if (pectin_arg_set(a, "TIMEDEMO")) {
        words[n++] = "timedemo";
        words[n++] = (char *)pectin_arg_str(a, "DEMO");
    } else {
        words[n++] = "server";
        words[n++] = (char *)pectin_arg_str(a, "MAP");
        snprintf(frames, sizeof frames, "%d", (int)pectin_arg_num(a, "FRAMES", 0));
        words[n++] = frames;
    }
    if (pectin_arg_num(a, "MEM", 0) > 0) {
        snprintf(mem, sizeof mem, "%d", (int)pectin_arg_num(a, "MEM", 0));
        words[n++] = "-mem";
        words[n++] = mem;
    }
    if (pectin_arg_set(a, "V"))
        words[n++] = "-v";
    return quince_bench(n, words);
}

// The line, parsed against QUINCE.VRB by the shell before main runs.
// Static, not on the stack: every big buffer belongs out of it.
static uint32_t argbuf[64];

int main(void)
{
    const struct pectin_args *a = pectin_args(argbuf, sizeof argbuf);
    if (!a)
        return 2;                       // built without its template
    if (pectin_arg_set(a, "SELFTEST"))
        return selftest();
    if (pectin_arg_set(a, "TIMEDEMO") || pectin_arg_set(a, "SERVER"))
        return bench(a);
    who = pectin_arg_str(a, "NAME");
    screen();
    return 0;
}
