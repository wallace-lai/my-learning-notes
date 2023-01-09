#include <iostream>
#include "avl.h"

using namespace std;

#define ARRAY_LEN(a) (sizeof(a) / sizeof(a[0]))

typedef struct tagDataNode {
    int data;
    avl_node_t node;
} DataNode;

static DataNode src0[] = {
    {.data = 51, .node = { 0 }},
    {.data = 98, .node = { 0 }},
    {.data = 44, .node = { 0 }},
    {.data = 83, .node = { 0 }},
    {.data = 20, .node = { 0 }}
};

static DataNode src1[] = {
    {.data = 89, .node = { 0 }},
    {.data = 84, .node = { 0 }},
    {.data = 99, .node = { 0 }},
    {.data = 23, .node = { 0 }},
    {.data = 28, .node = { 0 }},
    {.data = 34, .node = { 0 }},
    // {.data = 71, .node = { 0 }},
    // {.data = 49, .node = { 0 }},
    // {.data = 40, .node = { 0 }},
    // {.data = 78, .node = { 0 }}
};

static DataNode src2[] = {
    {.data = 96, .node = { 0 }},
    {.data = 37, .node = { 0 }},
    {.data = 74, .node = { 0 }},
    {.data = 10, .node = { 0 }},
    {.data = 32, .node = { 0 }},
    {.data = 53, .node = { 0 }},
    {.data = 59, .node = { 0 }},
    {.data = 63, .node = { 0 }},
    {.data = 77, .node = { 0 }},
    {.data = 19, .node = { 0 }},
    {.data = 42, .node = { 0 }},
    {.data = 28, .node = { 0 }},
    {.data = 21, .node = { 0 }},
    {.data = 90, .node = { 0 }},
    {.data = 43, .node = { 0 }},
    {.data = 17, .node = { 0 }},
    {.data = 23, .node = { 0 }},
    {.data = 10, .node = { 0 }},
    {.data = 75, .node = { 0 }},
    {.data = 68, .node = { 0 }}
};

static DataNode src3[] = {
    {.data = 5, .node = {0}},   // 0
    {.data = 3, .node = {0}},   // 1
    {.data = 7, .node = {0}},   // 2
    {.data = 2, .node = {0}},   // 3
    {.data = 4, .node = {0}},   // 4
    {.data = 6, .node = {0}},   // 5
    {.data = 8, .node = {0}},   // 6
    {.data = 1, .node = {0}},   // 7
    {.data = 9, .node = {0}},   // 8
};

static DataNode src4[] = {
    {.data = 1, .node = {0}},
    {.data = 2, .node = {0}},
    {.data = 3, .node = {0}},
};

int cmp(const void *lhs, const void *rhs)
{
    int diff = ((DataNode *)lhs)->data - ((DataNode *)rhs)->data;
    if (diff == 0) {
        return 0;
    }

    return (diff > 0 ? 1 : -1);
}

void avl_dump(avl_tree_t *tree)
{
    DataNode *dataNode = (DataNode *)avl_first(tree);

    while (dataNode != NULL) {
        printf(" %d", dataNode->data);
        dataNode = (DataNode *)AVL_NEXT(tree, dataNode);
    }
    printf("\n");
}

int main()
{
    avl_tree_t tree;

    avl_create(&tree, cmp, sizeof(DataNode), offsetof(DataNode, node));
    for (int i = 0; i < ARRAY_LEN(src3); i++) {
        avl_add(&tree, &src3[i]);
    }

    // case 1
    // avl_dump(&tree);
    avl_remove(&tree, &src3[2]);
    avl_dump(&tree);

    // // 9
    // avl_remove(&tree, &src3[8]);
    // avl_dump(&tree);

    // // 4
    // avl_remove(&tree, &src3[4]);
    // avl_dump(&tree);

    // // 6
    // avl_remove(&tree, &src3[5]);
    // avl_dump(&tree);

    return 0;
}