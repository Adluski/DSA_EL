/* gui.c - raylib + raygui window over the Phase 3 event index.
 * Same features as menu.c: type records, load a file, generate, show, count,
 * first-after. Times are seconds; values from 1973 on are shown as UTC dates. */
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui-4.0.h"
#include "../phase3/query.h"
#include "../common/gen.h"

#define SIGMA 32
#define EPS 16
#define MAXN 10000000u
#define LINES 33
#define FONTS "/usr/share/fonts/truetype/dejavu/"

/* colour theme (dark slate) */
#define BG      GetColor(0x0F172AFF)
#define CARD    GetColor(0x1E293BFF)
#define EDGE    GetColor(0x334155FF)
#define INK     GetColor(0xE2E8F0FF)
#define MUTED   GetColor(0x94A3B8FF)
#define CYAN    GetColor(0x22D3EEFF)
#define COBALT    GetColor(0x3B82F6FF)
#define IRIS  GetColor(0x8B5CF6FF)
#define TEAL    GetColor(0x14B8A6FF)
#define AMBER   GetColor(0xF59E0BFF)
#define MINT   GetColor(0x22C55EFF)
#define ROSE    GetColor(0xF43F5EFF)

typedef struct { uint64_t t; uint8_t ty; } Rec;
/* ts sorted, ty[i] = type of record i, n = 0 when nothing is loaded */
typedef struct { uint64_t *ts; uint8_t *ty; size_t n; EventIndex ix; } Data;
typedef struct { Rectangle r; const char *label; char text[512]; } Box;
enum { REC, N, SEED, PATH, X, Y, A, B, NBOX };

static struct { char s[100]; Color c; } out[LINES];   /* the output panel */
static int nout;

static void say(Color c, const char *fmt, ...)
{
    if (nout == LINES)
        return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(out[nout].s, sizeof out[0].s, fmt, ap);
    va_end(ap);
    out[nout++].c = c;
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

/* A time at *s: plain seconds, hh:mm[:ss] (hours may go past 24), or a UTC
 * date yyyy-mm-dd, optionally followed by hh:mm[:ss]. */
static int parse_time(const char **s, uint64_t *v)
{
    uint64_t h, m, sec = 0, mo, dd, days = 0;
    if (!parse_u64(s, &h))
        return 0;
    if (**s == '-') {
        if (!isdigit((unsigned char)*++*s) || !parse_u64(s, &mo) || **s != '-' ||
            !isdigit((unsigned char)*++*s) || !parse_u64(s, &dd) ||
            h < 1970 || h > 9999 || mo < 1 || mo > 12 || dd < 1 || dd > 31)
            return 0;
        /* days since 1970-01-01 (H. Hinnant's days_from_civil) */
        uint64_t y = h - (mo <= 2), era = y / 400, yoe = y % 400;
        days = era * 146097 + yoe * 365 + yoe / 4 - yoe / 100 +
               (153 * (mo > 2 ? mo - 3 : mo + 9) + 2) / 5 + dd - 1 - 719468;
        const char *p = *s + strspn(*s, " \t");
        if (!isdigit((unsigned char)*p) || p[strspn(p, "0123456789")] != ':') {
            *v = days * 86400;   /* date only: midnight */
            return 1;
        }
        *s = p;
        parse_u64(s, &h);
        if (h > 23)
            return 0;
    } else if (**s != ':') {
        *v = h;   /* plain seconds */
        return 1;
    }
    (*s)++;
    if (!isdigit((unsigned char)**s) || !parse_u64(s, &m) || m > 59)
        return 0;
    if (**s == ':') {
        (*s)++;
        if (!isdigit((unsigned char)**s) || !parse_u64(s, &sec) || sec > 59)
            return 0;
    }
    if (h > 1000000000)
        return 0;
    *v = days * 86400 + h * 3600 + m * 60 + sec;
    return 1;
}

/* The whole string is one time >= lo. */
static int time_box(const char *str, uint64_t lo, uint64_t *v)
{
    const char *s = str;
    return parse_time(&s, v) && strspn(s, " \t") == strlen(s) && *v >= lo;
}

/* Seconds shown as a clock: a UTC date-time for real timestamps (from 1973
 * on), else hh:mm:ss with "day N" in front after the first day.
 * Four rotating buffers, so one say() can show several times. */
static const char *clock_str(uint64_t t)
{
    static char buf[4][48];
    static int k;
    char *b = buf[k++ & 3];
    uint64_t day = t / 86400, r = t % 86400;
    time_t tt = (time_t)t;
    struct tm *g = t >= 100000000 ? gmtime(&tt) : NULL;
    if (g)
        strftime(b, sizeof buf[0], "%Y-%m-%d %H:%M:%S", g);
    else if (day)
        snprintf(b, sizeof buf[0], "day %" PRIu64 " %02" PRIu64 ":%02" PRIu64 ":%02" PRIu64,
                 day + 1, r / 3600, r / 60 % 60, r % 60);
    else
        snprintf(b, sizeof buf[0], "%02" PRIu64 ":%02" PRIu64 ":%02" PRIu64, r / 3600, r / 60 % 60, r % 60);
    return b;
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
    say(ROSE, "Out of memory.");
    return 0;
}

/* Load n parsed records (sorted here by time); bad = entries that were skipped. */
static void load_recs(Data *d, Rec *r, size_t n, size_t bad)
{
    if (bad)
        say(AMBER, "Skipped %zu bad entries (need: time type, type 0..31).", bad);
    if (!n) {
        say(ROSE, "No valid events found; old data kept.");
    } else if (reset(d, n)) {
        qsort(r, n, sizeof *r, by_time);
        for (size_t i = 0; i < n; i++)
            d->ts[i] = r[i].t, d->ty[i] = r[i].ty;
    }
}

/* A smooth TrueType font, or raylib's built-in one if the file is missing. */
static Font font(const char *file, int size)
{
    Font f = LoadFontEx(TextFormat(FONTS "%s", file), size, NULL, 0);
    if (!IsFontValid(f))
        return GetFontDefault();
    SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
    return f;
}

/* A button filled with one accent colour. */
static int button(Rectangle r, const char *text, Color c)
{
    GuiSetStyle(BUTTON, BASE_COLOR_NORMAL, ColorToInt(c));
    GuiSetStyle(BUTTON, BASE_COLOR_FOCUSED, ColorToInt(ColorBrightness(c, 0.2f)));
    GuiSetStyle(BUTTON, BASE_COLOR_PRESSED, ColorToInt(ColorBrightness(c, -0.25f)));
    GuiSetStyle(BUTTON, BORDER_COLOR_NORMAL, ColorToInt(c));
    GuiSetStyle(BUTTON, BORDER_COLOR_FOCUSED, ColorToInt(WHITE));
    GuiSetStyle(BUTTON, BORDER_COLOR_PRESSED, ColorToInt(WHITE));
    return GuiButton(r, text);
}

/* A rounded card: numbered title in an accent colour, then help lines
 * (separated by \n) saying what its buttons do. */
static void card(Font bold, Font ui, Rectangle r, const char *title, const char *help, Color c)
{
    char tmp[512];
    DrawRectangleRounded(r, 0.03f, 8, CARD);
    DrawRectangleRoundedLinesEx(r, 0.03f, 8, 1.0f, EDGE);
    DrawRectangle((int)r.x, (int)r.y + 14, 4, 22, c);
    DrawTextEx(bold, title, (Vector2){r.x + 16, r.y + 12}, 20, 1, c);
    snprintf(tmp, sizeof tmp, "%s", help);
    float y = r.y + 42;
    for (char *line = strtok(tmp, "\n"); line; line = strtok(NULL, "\n"), y += 20)
        DrawTextEx(ui, line, (Vector2){r.x + 16, y}, 15, 0, INK);
}

int main(void)
{
    Box bx[NBOX] = {
        [REC] = {{40, 232, 528, 36}, "Your events as  hh:mm type,  separated by commas", "09:30 3, 08:15 1, 09:00 3, 08:45 2, 10:10 5"},
        [N] = {{40, 354, 170, 36}, "How many events", "100000"},
        [SEED] = {{220, 354, 150, 36}, "Random seed", "42"},
        [PATH] = {{40, 430, 380, 36}, "Or a file with one  time type  per line", "../data/loghub/bgl_records.txt"},
        [X] = {{40, 778, 255, 36}, "From type (0-31)", "1"},
        [Y] = {{313, 778, 255, 36}, "To type (0-31)", "3"},
        [A] = {{40, 854, 255, 36}, "Start time", "08:30"},
        [B] = {{313, 854, 255, 36}, "End time", "10:00"},
    };
    Data d = {0};
    int edit = -1;
    uint64_t x, y, a, b, v;

    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(1280, 960, "Event log queries: PGM-index + wavelet tree");
    SetExitKey(KEY_NULL);   /* Esc must not close the window mid-demo */
    SetTargetFPS(60);
    Font ui = font("DejaVuSans.ttf", 18), bold = font("DejaVuSans-Bold.ttf", 32),
         mono = font("DejaVuSansMono.ttf", 17),
         small = font("DejaVuSans.ttf", 15);
    GuiSetFont(ui);
    GuiSetStyle(DEFAULT, TEXT_SIZE, 18);
    GuiSetStyle(DEFAULT, TEXT_SPACING, 0);
    GuiSetStyle(DEFAULT, BORDER_WIDTH, 1);
    GuiSetStyle(DEFAULT, BACKGROUND_COLOR, ColorToInt(BG));
    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL, ColorToInt(BG));
    GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED, ColorToInt(BG));
    GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED, ColorToInt(GetColor(0x0B1222FF)));
    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL, ColorToInt(EDGE));
    GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED, ColorToInt(CYAN));
    GuiSetStyle(DEFAULT, BORDER_COLOR_PRESSED, ColorToInt(CYAN));
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, ColorToInt(INK));
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED, ColorToInt(WHITE));
    GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED, ColorToInt(WHITE));
    GuiSetStyle(LABEL, TEXT_COLOR_NORMAL, ColorToInt(MUTED));
    GuiSetStyle(TEXTBOX, TEXT_PADDING, 10);
    say(MUTED, "Start with step 1: load your events or generate some.");

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BG);

        /* header: gradient bar, title, records-loaded chip */
        DrawRectangleGradientH(0, 0, GetScreenWidth(), 84, GetColor(0x1D4ED8FF), GetColor(0x0E7490FF));
        DrawTextEx(bold, "Event Log Queries", (Vector2){28, 12}, 32, 1, WHITE);
        DrawTextEx(ui, "PGM-index finds the time range, wavelet tree counts the types",
                   (Vector2){30, 52}, 18, 0, GetColor(0xBFDBFEFF));
        const char *chip = TextFormat("%zu events loaded", d.n);
        float cw = MeasureTextEx(ui, chip, 18, 0).x + 32;
        Rectangle cr = {GetScreenWidth() - cw - 24, 24, cw, 36};
        DrawRectangleRounded(cr, 1.0f, 12, Fade(BLACK, 0.3f));
        DrawCircle((int)cr.x + 16, (int)cr.y + 18, 5, d.n ? MINT : AMBER);
        DrawTextEx(ui, chip, (Vector2){cr.x + 26, cr.y + 8}, 18, 0, WHITE);

        card(bold, small, (Rectangle){24, 100, 560, 384}, "1  Get some events",
             "Load my events: uses the events you typed in the box below.\n"
             "Generate: makes a random event log with bursts of activity.\n"
             "Load file: reads a file of real events, e.g. the Loghub BGL log.", COBALT);
        card(bold, small, (Rectangle){24, 500, 560, 116}, "2  Look at them",
             "Show events: lists the first 30 events, oldest first.", TEAL);
        card(bold, small, (Rectangle){24, 632, 560, 316}, "3  Ask a question",
             "Count events: how many events of these types are in the time window.\n"
             "Find first event: earliest event of the From type at or after Start time.\n"
             "Times: hh:mm, hh:mm:ss, or a UTC date like 2005-07-01 08:00.", IRIS);
        card(bold, small, (Rectangle){604, 100, 652, 848}, "Answer", "", CYAN);
        DrawTextEx(small, "Tip: click a box to type in it; click again or press Enter when done.",
                   (Vector2){624, 918}, 15, 0, MUTED);

        for (int i = 0; i < NBOX; i++) {
            GuiLabel((Rectangle){bx[i].r.x, bx[i].r.y - 24, bx[i].r.width + 40, 22}, bx[i].label);
            if (GuiTextBox(bx[i].r, bx[i].text, sizeof bx[i].text, edit == i))
                edit = edit == i ? -1 : i;
        }
        int load = button((Rectangle){40, 276, 528, 38}, "Load my events", COBALT);
        int gen = button((Rectangle){380, 354, 188, 36}, "Generate", GetColor(0x6366F1FF));
        int loadf = button((Rectangle){430, 430, 138, 36}, "Load file", GetColor(0x0EA5E9FF));
        int show = button((Rectangle){40, 566, 528, 38}, "Show events", TEAL);
        int cnt = button((Rectangle){40, 904, 255, 38}, "Count events", IRIS);
        int first = button((Rectangle){313, 904, 255, 38}, "Find first event", GetColor(0xD946EFFF));

        for (int i = 0; i < nout; i++) {
            Vector2 p = {624, 144 + 19.0f * i};
            int ok = !strcmp(out[i].s, "match"), bad = !strcmp(out[i].s, "MISMATCH");
            if (ok || bad) {   /* result badge */
                const char *t = ok ? "MATCH" : "MISMATCH";
                float tw = MeasureTextEx(bold, t, 20, 1).x;
                DrawRectangleRounded((Rectangle){p.x, p.y + 2, tw + 40, 32}, 1.0f, 12, ok ? MINT : ROSE);
                DrawTextEx(bold, t, (Vector2){p.x + 20, p.y + 7}, 20, 1, BG);
            } else {
                DrawTextEx(mono, out[i].s, p, 17, 0, out[i].c);
            }
        }
        EndDrawing();

        if (load || loadf || gen || show || cnt || first)
            nout = 0;
        if ((show || cnt || first) && !d.n) {
            say(AMBER, "No events yet: use step 1 first.");
        } else if (load) {
            char tmp[sizeof bx[REC].text];
            size_t n = 0, bad = 0;
            strcpy(tmp, bx[REC].text);
            Rec *r = malloc((strlen(tmp) + 1) * sizeof *r);   /* more than enough */
            for (char *p = strtok(tmp, ",;"); r && p; p = strtok(NULL, ",;")) {
                const char *s = p;
                uint64_t t, ty;
                if (parse_time(&s, &t) && parse_u64(&s, &ty) && ty < SIGMA && strspn(s, " \t") == strlen(s))
                    r[n++] = (Rec){t, (uint8_t)ty};
                else if (p[strspn(p, " \t")])
                    bad++;
            }
            load_recs(&d, r, n, bad);
            free(r);
        } else if (loadf) {
            FILE *f = fopen(bx[PATH].text, "r");
            char line[256];
            size_t n = 0, cap = 0, bad = 0;
            Rec *r = NULL;
            if (!f)
                say(ROSE, "Cannot open file: %s", bx[PATH].text);
            while (f && n < MAXN && fgets(line, sizeof line, f)) {
                const char *s = line + strspn(line, " \t\r\n");
                uint64_t t, ty;
                if (!*s || *s == '#')   /* blank line or comment */
                    continue;
                if (!parse_time(&s, &t) || !parse_u64(&s, &ty) || ty >= SIGMA || s[strspn(s, " \t\r\n")]) {
                    bad++;
                    continue;
                }
                if (n == cap) {
                    Rec *g = realloc(r, (cap = cap ? 2 * cap : 1024) * sizeof *r);
                    if (!g)
                        break;
                    r = g;
                }
                r[n++] = (Rec){t, (uint8_t)ty};
            }
            if (f) {
                fclose(f);
                load_recs(&d, r, n, bad);
            }
            free(r);
        } else if (gen) {
            if (!num(bx[N].text, 1, MAXN, &v) || !num(bx[SEED].text, 0, UINT64_MAX, &a))
                say(ROSE, "How many events: 1 to %u.  Random seed: a whole number.", MAXN);
            else if (reset(&d, v))
                gen_events(v, a, d.ty, d.ts);
        } else if (show) {
            say(CYAN, "%5s  %19s  %5s", "#", "time", "type");
            for (size_t i = 0; i < d.n && i < 30; i++)
                say(i % 2 ? MUTED : INK, "%5zu  %19s  %5d", i, clock_str(d.ts[i]), d.ty[i]);
            if (d.n > 30)
                say(MUTED, "   ... %zu more", d.n - 30);
        } else if (cnt) {
            if (!num(bx[X].text, 0, SIGMA - 1, &x) || !num(bx[Y].text, x, SIGMA - 1, &y) ||
                !time_box(bx[A].text, 0, &a) || !time_box(bx[B].text, a, &b)) {
                say(ROSE, "Please check the numbers:");
                say(ROSE, "  From type and To type: 0 to 31, From <= To");
                say(ROSE, "  Start time <= End time (hh:mm or yyyy-mm-dd hh:mm)");
            } else {
                size_t l, r, brute = 0;
                ev_range(&d.ix, a, b, &l, &r);
                size_t wt = wt_range_count(d.ix.wt, (int)x, (int)y, l, r);
                for (size_t i = 0; i < d.n; i++)
                    brute += d.ts[i] >= a && d.ts[i] <= b && d.ty[i] >= x && d.ty[i] <= y;
                say(CYAN, "Count events with type %" PRIu64 " to %" PRIu64, x, y);
                say(CYAN, "  from  %s", clock_str(a));
                say(CYAN, "  to    %s   (both included)", clock_str(b));
                say(MUTED, " ");
                say(INK, "PGM-index: the events in this time window are at");
                if (l < r)
                    say(INK, "           positions %zu to %zu", l, r - 1);
                else
                    say(AMBER, "           no positions: the window is empty");
                say(MUTED, " ");
                if (brute)
                    say(CYAN, "Matching events:");
                else if (l < r)
                    say(AMBER, "No event of these types in this time window.");
                if (brute)
                    say(CYAN, "%5s  %19s  %5s", "#", "time", "type");
                size_t shown = 0;   /* list only the events that were counted */
                for (size_t i = l; i < r && shown < 14; i++)
                    if (d.ty[i] >= x && d.ty[i] <= y) {
                        say(MINT, "%5zu  %19s  %5d", i, clock_str(d.ts[i]), d.ty[i]);
                        shown++;
                    }
                if (brute > shown)
                    say(MUTED, "   ... %zu more matching events", brute - shown);
                say(MUTED, " ");
                say(IRIS, "wavelet tree  answer = %zu", wt);
                say(AMBER, "brute force   answer = %zu", brute);
                say(MUTED, " ");
                say(INK, "%s", wt == brute ? "match" : "MISMATCH");
            }
        } else if (first) {
            if (!num(bx[X].text, 0, SIGMA - 1, &x) || !time_box(bx[A].text, 0, &a))
                say(ROSE, "Please check: From type 0 to 31, Start time as hh:mm or a date.");
            else if (first_after(&d.ix, (int)x, a, &v))
                say(MINT, "Earliest type %" PRIu64 " event at or after %s", x, clock_str(a)),
                say(MINT, "  is at  %s", clock_str(v));
            else
                say(AMBER, "No type %" PRIu64 " event at or after %s.", x, clock_str(a));
        }
        if (d.n && !d.ix.wt) {   /* reset() cleared the index: build it for the new records */
            d.ix = ev_build(d.ts, d.ty, d.n, SIGMA, EPS);
            say(MINT, "Loaded %zu records, index built (PGM eps=%d + wavelet tree).", d.n, EPS);
        }
    }
    UnloadFont(ui);
    UnloadFont(bold);
    UnloadFont(mono);
    UnloadFont(small);
    CloseWindow();
    if (d.n)
        ev_free(&d.ix);
    free(d.ts);
    free(d.ty);
    return 0;
}
