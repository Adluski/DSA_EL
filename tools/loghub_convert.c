/* loghub_convert.c - turn a Loghub *_structured.csv into "time type" records.
 *   time = the Timestamp column (Unix seconds)
 *   type = EventTemplate mapped to 0..31: the 31 most frequent templates get
 *          0..30 by frequency (ties: template text order), all others are 31.
 * Usage: loghub_convert in.csv records.txt types.txt */
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OTHER 31
#define MAXF 64

typedef struct { char *text; size_t count; int type; } Tpl;
typedef struct { uint64_t t; size_t line; size_t tpl; } Rec;

static Tpl *tpls;   /* global so the comparators can see it */

static int by_count(const void *a, const void *b)
{
    const Tpl *x = &tpls[*(const size_t *)a], *y = &tpls[*(const size_t *)b];
    if (x->count != y->count)
        return x->count < y->count ? 1 : -1;
    return strcmp(x->text, y->text);
}

/* by time; equal times keep file order */
static int by_time(const void *a, const void *b)
{
    const Rec *x = a, *y = b;
    if (x->t != y->t)
        return x->t < y->t ? -1 : 1;
    return (x->line > y->line) - (x->line < y->line);
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "usage: %s in.csv records.txt types.txt\n", argv[0]);
        return 1;
    }
    FILE *in = fopen(argv[1], "r");
    if (!in) {
        perror(argv[1]);
        return 1;
    }
    static char line[1 << 16];
    char *f[MAXF];
    int col_t = -1, col_e = -1;
    size_t ntpl = 0, nrec = 0, cap = 0, lineno = 0;
    Rec *recs = NULL;

    while (fgets(line, sizeof line, in)) {
        lineno++;
        size_t len = strcspn(line, "\r\n");
        if (line[len] == '\0' && !feof(in)) {
            fprintf(stderr, "line %zu too long\n", lineno);
            return 1;
        }
        line[len] = '\0';
        /* split CSV in place: "quoted, fields" and "" inside quotes */
        int nf = 0;
        for (char *p = line; nf < MAXF;) {
            char *w = p, c;
            f[nf++] = w;
            if (*p == '"') {
                for (p++; *p && !(*p == '"' && p[1] != '"'); p++)
                    *w++ = *p == '"' ? *p++ : *p;
                if (*p == '"')
                    p++;
            } else {
                while (*p && *p != ',')
                    w++, p++;
            }
            c = *p;
            *w = '\0';
            if (c != ',')
                break;
            p++;
        }
        if (col_t < 0) {   /* header: find the two columns we need */
            for (int i = 0; i < nf; i++) {
                if (!strcmp(f[i], "Timestamp"))
                    col_t = i;
                if (!strcmp(f[i], "EventTemplate"))
                    col_e = i;
            }
            if (col_t < 0 || col_e < 0) {
                fprintf(stderr, "header has no Timestamp/EventTemplate column\n");
                return 1;
            }
            continue;
        }
        char *end = NULL;
        uint64_t t = 0;
        errno = 0;
        if (nf > col_t && nf > col_e)
            t = strtoull(f[col_t], &end, 10);
        if (!end || errno || end == f[col_t] || *end) {
            fprintf(stderr, "line %zu: bad row, skipped\n", lineno);
            continue;
        }
        size_t k = 0;
        while (k < ntpl && strcmp(tpls[k].text, f[col_e]))
            k++;
        if (k == ntpl) {
            size_t n = strlen(f[col_e]) + 1;   /* strdup is not C11 */
            tpls = realloc(tpls, (ntpl + 1) * sizeof *tpls);
            tpls[ntpl++] = (Tpl){memcpy(malloc(n), f[col_e], n), 0, OTHER};
        }
        tpls[k].count++;
        if (nrec == cap)
            recs = realloc(recs, (cap = cap ? 2 * cap : 1024) * sizeof *recs);
        recs[nrec++] = (Rec){t, lineno, k};
    }
    fclose(in);

    /* rank templates by frequency; the top 31 get types 0..30 */
    size_t *order = malloc(ntpl * sizeof *order), other_n = 0, other_t = 0;
    for (size_t i = 0; i < ntpl; i++)
        order[i] = i;
    qsort(order, ntpl, sizeof *order, by_count);
    for (size_t i = 0; i < ntpl && i < OTHER; i++)
        tpls[order[i]].type = (int)i;

    FILE *out = fopen(argv[2], "w"), *types = fopen(argv[3], "w");
    if (!out || !types) {
        perror("output");
        return 1;
    }
    qsort(recs, nrec, sizeof *recs, by_time);
    for (size_t i = 0; i < nrec; i++)
        fprintf(out, "%" PRIu64 " %d\n", recs[i].t, tpls[recs[i].tpl].type);
    fprintf(types, "# type\tcount\ttemplate\n");
    for (size_t i = 0; i < ntpl; i++) {
        Tpl *p = &tpls[order[i]];
        if (p->type < OTHER)
            fprintf(types, "%d\t%zu\t%s\n", p->type, p->count, p->text);
        else
            other_n += p->count, other_t++;
    }
    fprintf(types, "%d\t%zu\tother (%zu less frequent templates)\n", OTHER, other_n, other_t);
    fclose(out);
    fclose(types);
    printf("%zu records, %zu distinct templates (%zu merged into type %d), time %" PRIu64
           " .. %" PRIu64 "\n", nrec, ntpl, other_t, OTHER, nrec ? recs[0].t : 0,
           nrec ? recs[nrec - 1].t : 0);
    for (size_t i = 0; i < ntpl; i++)
        free(tpls[i].text);
    free(tpls);
    free(recs);
    free(order);
    return 0;
}
