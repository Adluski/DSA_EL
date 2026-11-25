/* gui.c - raylib + raygui window over the Phase 3 event index.
 * Same features as menu.c: type records, generate, show, count, first-after. */
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui-4.0.h"
#include "../phase3/query.h"
#include "../common/gen.h"

#define SIGMA 32
#define EPS 16
#define MAXN 10000000u
#define LINES 33

typedef struct { uint64_t t; uint8_t ty; } Rec;
/* ts sorted, ty[i] = type of record i, n = 0 when nothing is loaded */
typedef struct { uint64_t *ts; uint8_t *ty; size_t n; EventIndex ix; } Data;
typedef struct { Rectangle r; const char *label; char text[512]; } Box;
enum { REC, N, SEED, X, Y, A, B, NBOX };

static char out[LINES][100];   /* the output panel */
static int nout;

static void say(const char *fmt, ...)
{
    if (nout == LINES)
        return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(out[nout++], sizeof out[0], fmt, ap);
    va_end(ap);
}

static int by_time(const void *a, const void *b)
{
    uint64_t x = ((const Rec *)a)->t, y = ((const Rec *)b)->t;
    return (x > y) - (x < y);
}

/* Parse one unsigned number at *s and move *s past it. No sign allowed. */
static int parse_u64(const char **s, uint64_t *v)
{
    char *end;
    while (isspace((unsigned char)**s))
        (*s)++;
    if (!isdigit((unsigned char)**s))
        return 0;
    errno = 0;
    *v = strtoull(*s, &end, 10);
    *s = end;
    return errno == 0;
}

/* The whole string is one number in lo..hi. */
static int num(const char *str, uint64_t lo, uint64_t hi, uint64_t *v)
{
    const char *s = str;
    return parse_u64(&s, v) && strspn(s, " \t") == strlen(s) && *v >= lo && *v <= hi;
}

/* Drop the old records and index; make room for n new ones (n >= 1).
 * Returns 0 if out of memory (then nothing is loaded). */
static int reset(Data *d, size_t n)
{
    if (d->n)
        ev_free(&d->ix);
    free(d->ts);
    free(d->ty);
    *d = (Data){.ts = malloc(n * sizeof *d->ts), .ty = malloc(n)};
    if (d->ts && d->ty)
        return (int)(d->n = n);
    say("Out of memory.");
    return 0;
}

int main(void)
{
    Box bx[NBOX] = {
        [REC] = {{20, 110, 440, 32}, "Records: time type, comma separated", "50 3, 10 1, 30 3, 20 2, 70 5"},
        [N] = {{20, 215, 130, 32}, "N", "100000"},
        [SEED] = {{160, 215, 130, 32}, "Seed", "42"},
        [X] = {{20, 345, 60, 32}, "X", "1"},
        [Y] = {{90, 345, 60, 32}, "Y", "3"},
        [A] = {{160, 345, 140, 32}, "A (time)", "20"},
        [B] = {{310, 345, 150, 32}, "B (time)", "70"},
    };
    Data d = {0};
    int edit = -1;
    uint64_t x, y, a, b, v;

    SetTraceLogLevel(LOG_WARNING);
    InitWindow(1100, 740, "Event log queries: PGM-index + wavelet tree");
    SetExitKey(KEY_NULL);   /* Esc must not close the window mid-demo */
    SetTargetFPS(60);
    GuiSetStyle(DEFAULT, TEXT_SIZE, 18);
    say("Load typed records or generate some to start.");

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText("Event log queries", 20, 20, 30, DARKBLUE);
        DrawText(TextFormat("PGM-index + wavelet tree    records loaded: %zu", d.n), 20, 56, 18, GRAY);
        for (int i = 0; i < NBOX; i++) {
            GuiLabel((Rectangle){bx[i].r.x, bx[i].r.y - 24, bx[i].r.width, 22}, bx[i].label);
            if (GuiTextBox(bx[i].r, bx[i].text, sizeof bx[i].text, edit == i))
                edit = edit == i ? -1 : i;
        }
        int load = GuiButton((Rectangle){20, 150, 440, 32}, "Load typed records (sorted by time)");
        int gen = GuiButton((Rectangle){300, 215, 160, 32}, "Generate bursty");
        int show = GuiButton((Rectangle){20, 265, 440, 32}, "Show records (first 30)");
        int cnt = GuiButton((Rectangle){20, 390, 215, 32}, "Count X..Y in [A, B]");
        int first = GuiButton((Rectangle){245, 390, 215, 32}, "First X at/after A");
        DrawRectangleLines(480, 20, 600, 700, LIGHTGRAY);
        for (int i = 0; i < nout; i++)
            DrawText(out[i], 492, 30 + 21 * i, 18,
                     strstr(out[i], "MISMATCH") ? RED : strstr(out[i], "match") ? DARKGREEN : BLACK);
        EndDrawing();

        if (load || gen || show || cnt || first)
            nout = 0;
        if ((show || cnt || first) && !d.n) {
            say("No records yet: load or generate first.");
        } else if (load) {
            char tmp[sizeof bx[REC].text];
            size_t n = 0, bad = 0;
            strcpy(tmp, bx[REC].text);
            Rec *r = malloc((strlen(tmp) + 1) * sizeof *r);   /* more than enough */
            for (char *p = strtok(tmp, ",;"); r && p; p = strtok(NULL, ",;")) {
                const char *s = p;
                uint64_t t, ty;
                if (parse_u64(&s, &t) && parse_u64(&s, &ty) && ty < SIGMA && strspn(s, " \t") == strlen(s))
                    r[n++] = (Rec){t, (uint8_t)ty};
                else if (p[strspn(p, " \t")])
                    bad++;
            }
            if (bad)
                say("Skipped %zu bad entries (need: time type, type 0..31).", bad);
            if (!n) {
                say("No valid records in the box; old data kept.");
            } else if (reset(&d, n)) {
                qsort(r, n, sizeof *r, by_time);
                for (size_t i = 0; i < n; i++)
                    d.ts[i] = r[i].t, d.ty[i] = r[i].ty;
            }
            free(r);
        } else if (gen) {
            if (!num(bx[N].text, 1, MAXN, &v) || !num(bx[SEED].text, 0, UINT64_MAX, &a))
                say("N must be 1..%u and the seed a whole number.", MAXN);
            else if (reset(&d, v))
                gen_events(v, a, d.ty, d.ts);
        } else if (show) {
            say("%6s %20s %5s", "#", "time", "type");
            for (size_t i = 0; i < d.n && i < 30; i++)
                say("%6zu %20" PRIu64 " %5d", i, d.ts[i], d.ty[i]);
            if (d.n > 30)
                say("... %zu more", d.n - 30);
        } else if (cnt) {
            if (!num(bx[X].text, 0, SIGMA - 1, &x) || !num(bx[Y].text, x, SIGMA - 1, &y) ||
                !num(bx[A].text, 0, UINT64_MAX, &a) || !num(bx[B].text, a, UINT64_MAX, &b)) {
                say("Need whole numbers with 0 <= X <= Y <= 31 and A <= B.");
            } else {
                size_t l, r, brute = 0;
                ev_range(&d.ix, a, b, &l, &r);
                size_t wt = wt_range_count(d.ix.wt, (int)x, (int)y, l, r);
                for (size_t i = 0; i < d.n; i++)
                    brute += d.ts[i] >= a && d.ts[i] <= b && d.ty[i] >= x && d.ty[i] <= y;
                say("Types %" PRIu64 "..%" PRIu64 ", time [%" PRIu64 ", %" PRIu64 "]", x, y, a, b);
                say("PGM: positions l=%zu .. r=%zu (r excluded)", l, r);
                say("wavelet tree answer: %zu", wt);
                say("brute-force answer:  %zu", brute);
                say("%s", wt == brute ? "match" : "MISMATCH");
            }
        } else if (first) {
            if (!num(bx[X].text, 0, SIGMA - 1, &x) || !num(bx[A].text, 0, UINT64_MAX, &a))
                say("Need whole numbers with 0 <= X <= 31 and A >= 0.");
            else if (first_after(&d.ix, (int)x, a, &v))
                say("First type %" PRIu64 " event at or after %" PRIu64 ": time %" PRIu64, x, a, v);
            else
                say("No type %" PRIu64 " event at or after %" PRIu64 ".", x, a);
        }
        if (d.n && !d.ix.wt) {   /* reset() cleared the index: build it for the new records */
            d.ix = ev_build(d.ts, d.ty, d.n, SIGMA, EPS);
            say("Loaded %zu records, index built (PGM eps=%d + wavelet tree).", d.n, EPS);
        }
    }
    CloseWindow();
    if (d.n)
        ev_free(&d.ix);
    free(d.ts);
    free(d.ty);
    return 0;
}
