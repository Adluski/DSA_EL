/* wavelet.h - bit vector with rank/select, and a pointer-based wavelet tree.
 * Positions are 0-based. select's k is 1-based (k = 1 means first occurrence). */
#ifndef WAVELET_H
#define WAVELET_H

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

/* ---------------- bit vector ---------------- */

typedef struct {
    uint64_t *bits;   /* bit i lives in word i/64, at position i%64 */
    uint32_t *cnt;    /* cnt[w] = number of 1s in words 0..w-1 (so n must be < 2^32) */
    size_t n;         /* number of bits */
} BitVec;

/* Number of positions j < i whose bit equals b. O(1): one table read
 * plus one popcount on the part of word i/64 that lies below i. */
static inline size_t bv_rank(const BitVec *bv, int b, size_t i)
{
    size_t w = i >> 6;
    uint64_t below = (1ULL << (i & 63)) - 1;   /* mask of bits 0..(i%64 - 1) */
    size_t ones = bv->cnt[w] + (size_t)__builtin_popcountll(bv->bits[w] & below);
    return b ? ones : i - ones;
}

/* Position of the k-th bit equal to b (k >= 1). The caller makes sure it exists.
 * rank(j) never decreases as j grows, so we binary search for the smallest j
 * with rank(j) == k; the bit we want is then at j - 1. O(log n). */
static inline size_t bv_select(const BitVec *bv, int b, size_t k)
{
    size_t lo = 1, hi = bv->n;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (bv_rank(bv, b, mid) < k)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo - 1;
}

/* ---------------- wavelet tree ---------------- */

/* A node covers symbols lo..hi. Its bit vector has one bit per symbol that
 * reached it: 0 if the symbol is in the lower half (goes left), 1 if upper half.
 * A leaf (lo == hi) stores no bits, because every symbol in it equals lo;
 * its bv.n still records how many symbols reached it. */
typedef struct WtNode {
    int lo, hi;
    BitVec bv;
    struct WtNode *left, *right;
} WtNode;

/* Build the tree for a[0..n) with symbols in lo..hi.
 * For the whole tree call wt_build(a, n, 0, sigma - 1). */
static inline WtNode *wt_build(const uint8_t *a, size_t n, int lo, int hi)
{
    WtNode *node = calloc(1, sizeof *node);
    node->lo = lo;
    node->hi = hi;
    node->bv.n = n;
    if (lo == hi)
        return node;

    int mid = (lo + hi) / 2;
    /* n/64 + 1 words: one spare word so rank(n) is safe even when n % 64 == 0 */
    size_t nw = n / 64 + 1;
    node->bv.bits = calloc(nw, sizeof(uint64_t));
    node->bv.cnt = calloc(nw, sizeof(uint32_t));

    /* split the symbols into the two halves, keeping their order */
    uint8_t *la = malloc(n + 1), *ra = malloc(n + 1);   /* +1 avoids malloc(0) */
    size_t nl = 0, nr = 0;
    for (size_t i = 0; i < n; i++) {
        if (a[i] <= mid) {
            la[nl++] = a[i];
        } else {
            ra[nr++] = a[i];
            node->bv.bits[i >> 6] |= 1ULL << (i & 63);
        }
    }
    for (size_t w = 1; w < nw; w++)
        node->bv.cnt[w] = node->bv.cnt[w - 1] + (uint32_t)__builtin_popcountll(node->bv.bits[w - 1]);

    node->left = wt_build(la, nl, lo, mid);
    node->right = wt_build(ra, nr, mid + 1, hi);
    free(la);
    free(ra);
    return node;
}

static inline void wt_free(WtNode *node)
{
    if (!node)
        return;
    wt_free(node->left);
    wt_free(node->right);
    free(node->bv.bits);
    free(node->bv.cnt);
    free(node);
}

/* Symbol at position i. Follow i down: at each node the bit tells us which
 * child the symbol went to, and rank tells us its new position in that child. */
static inline int wt_access(const WtNode *node, size_t i)
{
    while (node->lo < node->hi) {
        int b = (int)((node->bv.bits[i >> 6] >> (i & 63)) & 1);
        i = bv_rank(&node->bv, b, i);
        node = b ? node->right : node->left;
    }
    return node->lo;
}

/* Occurrences of c in positions [0, i). Same walk as access, but the path is
 * chosen by c. When we reach c's leaf, i is the count of c's before the
 * original position i. */
static inline size_t wt_rank(const WtNode *node, int c, size_t i)
{
    if (c < node->lo || c > node->hi)
        return 0;
    while (node->lo < node->hi) {
        int b = c > (node->lo + node->hi) / 2;
        i = bv_rank(&node->bv, b, i);
        node = b ? node->right : node->left;
    }
    return i;
}

/* Position of the k-th occurrence of c, or -1 if there is none.
 * Go down to c's leaf, where the k-th c is simply at position k-1. On the way
 * back up, the position p in a child is the (p+1)-th 0 (left child) or 1
 * (right child) in the parent's bit vector, so select on that bit maps it up. */
static inline int64_t wt_select(const WtNode *node, int c, size_t k)
{
    if (c < node->lo || c > node->hi)
        return -1;
    if (node->lo == node->hi)
        return (k >= 1 && k <= node->bv.n) ? (int64_t)(k - 1) : -1;

    int b = c > (node->lo + node->hi) / 2;
    int64_t p = wt_select(b ? node->right : node->left, c, k);
    if (p < 0)
        return -1;
    return (int64_t)bv_select(&node->bv, b, (size_t)p + 1);
}

/* Occurrences of c in positions [l, r). */
static inline size_t wt_count(const WtNode *root, int c, size_t l, size_t r)
{
    return wt_rank(root, c, r) - wt_rank(root, c, l);
}

/* Total memory of the tree in bytes: node structs + bit words + count arrays. */
static inline size_t wt_bytes(const WtNode *node)
{
    if (!node)
        return 0;
    size_t b = sizeof *node;
    if (node->lo < node->hi)
        b += (node->bv.n / 64 + 1) * (sizeof(uint64_t) + sizeof(uint32_t));
    return b + wt_bytes(node->left) + wt_bytes(node->right);
}

#endif
