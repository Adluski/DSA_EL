/* demo.c - small hand-checkable wavelet tree: 12 symbols, alphabet 0..7. */
#include <stdio.h>
#include "wavelet.h"

static const uint8_t A[12] = {3, 1, 4, 1, 5, 2, 6, 5, 3, 5, 0, 7};
#define N 12

/* Print nodes in preorder, indented by depth. The symbols that reach a node
 * are exactly the input symbols in lo..hi, in their original order, so we
 * list them next to the bits to make each level easy to check on paper. */
static void print_node(const WtNode *node, int depth)
{
    printf("%*s[%d,%d] symbols:", depth * 4, "", node->lo, node->hi);
    for (int i = 0; i < N; i++)
        if (A[i] >= node->lo && A[i] <= node->hi)
            printf(" %d", A[i]);
    if (node->lo == node->hi) {
        printf("   (leaf)\n");
        return;
    }
    printf("\n%*s       bits:   ", depth * 4, "");
    for (size_t i = 0; i < node->bv.n; i++)
        printf("%d ", (int)((node->bv.bits[i >> 6] >> (i & 63)) & 1));
    printf("  (0 = %d..%d go left, 1 = %d..%d go right)\n",
           node->lo, (node->lo + node->hi) / 2, (node->lo + node->hi) / 2 + 1, node->hi);
    print_node(node->left, depth + 1);
    print_node(node->right, depth + 1);
}

int main(void)
{
    printf("position: ");
    for (int i = 0; i < N; i++)
        printf("%2d ", i);
    printf("\nsymbol:   ");
    for (int i = 0; i < N; i++)
        printf("%2d ", A[i]);
    printf("\n\n");

    WtNode *root = wt_build(A, N, 0, 7);
    print_node(root, 0);

    printf("\nqueries (positions start at 0, ranges are [l, r)):\n");
    printf("access(4)       = %d\n", wt_access(root, 4));
    printf("access(10)      = %d\n", wt_access(root, 10));
    printf("rank(5, 10)     = %zu   (5s in positions 0..9)\n", wt_rank(root, 5, 10));
    printf("rank(1, 3)      = %zu   (1s in positions 0..2)\n", wt_rank(root, 1, 3));
    printf("rank(7, 12)     = %zu\n", wt_rank(root, 7, 12));
    printf("select(5, 2)    = %lld   (2nd 5)\n", (long long)wt_select(root, 5, 2));
    printf("select(1, 2)    = %lld   (2nd 1)\n", (long long)wt_select(root, 1, 2));
    printf("select(6, 2)    = %lld  (there is only one 6)\n", (long long)wt_select(root, 6, 2));
    printf("count(5, 5, 10) = %zu   (5s in positions 5..9)\n", wt_count(root, 5, 5, 10));
    printf("count(3, 0, 12) = %zu\n", wt_count(root, 3, 0, 12));

    wt_free(root);
    return 0;
}
