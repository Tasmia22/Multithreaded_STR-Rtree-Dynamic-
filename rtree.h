#ifndef RTREE_H
#define RTREE_H

#include <limits.h>
#include <stdbool.h>

#define BUNDLEFACTOR 1024   // max rectangles per leaf
#define FANOUT 128         // max children per internal node

typedef struct {
    int xmin, ymin, xmax, ymax;
} Rect, MBR;

typedef struct Node {
    int isLeaf;
    int count;
    union {
        struct Node **children; // internal node
        Rect *rects;            // leaf node
    };
    MBR mbr;
} Node;
typedef struct RTreeStats
{
    int totalNodes;
    int leafNodes;
    int internalNodes;
    int maxDepth;
} RTreeStats;
typedef struct {
    int z_value;
    int index;
} ZRect;

// typedef struct {
//     int thread_id;
//     int start_idx;
//     int end_idx;
//     Rect *queries;
//     int *results;
//     Node *root;
// } ThreadArgs;


// Function declarations
Rect *readRectsFromFile(const char *filename, int *num_rects);
void initMBR(MBR *mbr);
void updateMBRWithRect(MBR *mbr, Rect r);
MBR unionJoin(MBR *mbr1, MBR *mbr2);
Node *createLeaf(Rect *rectArr, int low, int high);
int compareByXCenter(const void *a, const void *b);
int compareByYCenter(const void *a, const void *b);
Node *createRTree(Rect *rectArr, int low, int high);
Node *createRTree_STR(Rect *rectArr, int low, int high);
Node *createRTree_STR_2(Rect *rectArr, int low, int high);
bool isOverlap(const MBR *mbr, Rect r);
int searchRTree(Node *node, Rect queryRect, int q);
void printRTreeStats(Node *root);
void Zsorting(Rect rects[], int num_rects);
void writeTimingLog(int numRects, int numQuery, int numThreads, double seq_time_ms, double par_time_ms);
int searchRTree_iter(Node *root, Rect queryRect, int q);

Rect *selectDataDataset(int *numRects, int option);
Rect *selectQueryDataset(int *numQuery, int dataset_option);
#endif
