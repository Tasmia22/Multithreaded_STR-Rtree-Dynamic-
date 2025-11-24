#include "rtree.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <math.h>

int countRectsInFile(const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (!file)
    {
        perror("Unable to open file for counting");
        return 0;
    }

    int count = 0;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), file))
    {
        count++;
    }

    fclose(file);
    return count;
}

// Function to initialize a bounding box
Rect *readRectsFromFile(const char *filename, int *num_rects)
{
    int line_count = countRectsInFile(filename); // Each line = one rectangle
    *num_rects = line_count;

    if (*num_rects <= 0)
    {
        return NULL;
    }

    Rect *rects = (Rect *)malloc(*num_rects * sizeof(Rect));
    if (!rects)
    {
        perror("Unable to allocate memory for rectangles");
        return NULL;
    }

    FILE *file = fopen(filename, "r");
    if (!file)
    {
        perror("Unable to open file");
        free(rects);
        return NULL;
    }

    int x1, y1, x2, y2;
    for (int i = 0; i < *num_rects; i++)
    {
        if (fscanf(file, "%d,%d,%d,%d", &x1, &y1, &x2, &y2) != 4)
        {
            fprintf(stderr, "Error reading rectangle at line %d\n", i + 1);
            free(rects);
            fclose(file);
            return NULL;
        }

        // Normalize rectangle coordinates
        rects[i].xmin = (x1 < x2) ? x1 : x2;
        rects[i].ymin = (y1 < y2) ? y1 : y2;
        rects[i].xmax = (x1 > x2) ? x1 : x2;
        rects[i].ymax = (y1 > y2) ? y1 : y2;
    }

    fclose(file);
    return rects;
}



void initMBR(MBR *mbr)
{
   mbr->xmin = INT_MAX;
   mbr->ymin = INT_MAX;
   mbr->xmax = INT_MIN;
   mbr->ymax = INT_MIN;
}


// Function to update the MBR with a point
void updateMBRWithRect(MBR *mbr, Rect r)
{
   if (r.xmin < mbr->xmin)
       mbr->xmin = r.xmin;
   if (r.ymin < mbr->ymin)
       mbr->ymin = r.ymin;
   if (r.xmax > mbr->xmax)
       mbr->xmax = r.xmax;
   if (r.ymax > mbr->ymax)
       mbr->ymax = r.ymax;
}


// Function to compute the union of two MBRs
MBR unionJoin(MBR *mbr1, MBR *mbr2)
{
   MBR result;
   result.xmin = (mbr1->xmin < mbr2->xmin) ? mbr1->xmin : mbr2->xmin;
   result.ymin = (mbr1->ymin < mbr2->ymin) ? mbr1->ymin : mbr2->ymin;
   result.xmax = (mbr1->xmax > mbr2->xmax) ? mbr1->xmax : mbr2->xmax;
   result.ymax = (mbr1->ymax > mbr2->ymax) ? mbr1->ymax : mbr2->ymax;
   return result;
}


// Function to create a leaf node
Node *createLeaf(Rect *rectArr, int low, int high)
{
   Node *leaf = (Node *)malloc(sizeof(Node));
   leaf->isLeaf = 1;
   leaf->count = high - low + 1;
   leaf->rects = (Rect *)malloc(leaf->count * sizeof(Rect));


   // Initialize MBR and update with each rectangle
   initMBR(&leaf->mbr);
   for (int i = low; i <= high; i++)
   {
       leaf->rects[i - low] = rectArr[i];
       updateMBRWithRect(&leaf->mbr, rectArr[i]);
   }
   return leaf;
}
// Safer leaf: also clear children pointer
Node *createLeaf_STR(Rect *rectArr, int low, int high)
{
    Node *leaf = (Node *)malloc(sizeof(Node));
    leaf->isLeaf = 1;
    leaf->children = NULL;                   // <-- important
    leaf->count = high - low + 1;
    leaf->rects = (Rect *)malloc((size_t)leaf->count * sizeof(Rect));

    initMBR(&leaf->mbr);
    for (int i = low; i <= high; i++) {
        leaf->rects[i - low] = rectArr[i];
        updateMBRWithRect(&leaf->mbr, rectArr[i]);
    }
    return leaf;
}



int compareByXCenter(const void *a, const void *b)
{
   const Rect *r1 = (const Rect *)a;
   const Rect *r2 = (const Rect *)b;
   int cx1 = (r1->xmin + r1->xmax) / 2;
   int cx2 = (r2->xmin + r2->xmax) / 2;
   return cx1 - cx2;
}


int compareByYCenter(const void *a, const void *b)
{
   const Rect *r1 = (const Rect *)a;
   const Rect *r2 = (const Rect *)b;
   int cy1 = (r1->ymin + r1->ymax) / 2;
   int cy2 = (r2->ymin + r2->ymax) / 2;
   return cy1 - cy2;
}


Node *createRTree(Rect *rectArr, int low, int high)
{
   int total = high - low + 1;
   if (total <= 0)
       return NULL;


   // Step 1: Sort rectangles by X, then Y (space-filling order)
   qsort(&rectArr[low], total, sizeof(Rect), compareByXCenter);


   // Step 2: Create leaf nodes
   int numLeaves = (total + BUNDLEFACTOR - 1) / BUNDLEFACTOR;
   Node **current_level = (Node **)malloc(numLeaves * sizeof(Node *));


   for (int i = 0; i < numLeaves; i++)
   {
       int start = low + i * BUNDLEFACTOR;
       int end = (start + BUNDLEFACTOR - 1 < high) ? (start + BUNDLEFACTOR - 1) : high;
       current_level[i] = createLeaf(rectArr, start, end);
   }


   // Step 3: Recursively group nodes into higher internal nodes
   while (numLeaves > 1)
   {
       int numParents = (numLeaves + FANOUT - 1) / FANOUT;
       Node **next_level = (Node **)malloc(numParents * sizeof(Node *));


       for (int i = 0; i < numParents; i++)
       {
           int start = i * FANOUT;
           int end = (start + FANOUT < numLeaves) ? (start + FANOUT) : numLeaves;


           Node *parent = (Node *)malloc(sizeof(Node));
           parent->isLeaf = 0;
           parent->count = end - start;
           parent->children = (Node **)malloc(parent->count * sizeof(Node *));
           initMBR(&parent->mbr);


           for (int j = start; j < end; j++)
           {
               parent->children[j - start] = current_level[j];
               parent->mbr = unionJoin(&parent->mbr, &current_level[j]->mbr);
           }


           next_level[i] = parent;
       }


       free(current_level);
       current_level = next_level;
       numLeaves = numParents;
   }


   // Final root node
   Node *root = current_level[0];
   free(current_level);
   return root;
}
Node *createRTree_STR(Rect *rectArr, int low, int high)
{
    int total = high - low + 1;
    if (total <= 0) return NULL;

    // 1) Global X sort
    qsort(&rectArr[low], (size_t)total, sizeof(Rect), compareByXCenter);

    // 2) Determine tiling
    int targetLeaves = (total + BUNDLEFACTOR - 1) / BUNDLEFACTOR;
    int numSlices    = (int)ceil(sqrt((double)targetLeaves));
    int sliceSize    = (total + numSlices - 1) / numSlices;   // ceil

    // -------- Pass A: count leaves we'll actually create --------
    int countedLeaves = 0;
    for (int s = 0; s < numSlices; s++) {
        int sliceLow  = low + s * sliceSize;
        int sliceHigh = sliceLow + sliceSize - 1;
        if (sliceLow > high) break;
        if (sliceHigh > high) sliceHigh = high;

        int sliceCount = sliceHigh - sliceLow + 1;
        if (sliceCount <= 0) continue;

        // ceil(sliceCount / BUNDLEFACTOR)
        countedLeaves += (sliceCount + BUNDLEFACTOR - 1) / BUNDLEFACTOR;
    }

    // Allocate exactly what we need
    Node **current_level = (Node **)malloc((size_t)countedLeaves * sizeof(Node *));
    int leafCount = 0;

    // -------- Pass B: build leaves slice-by-slice --------
    for (int s = 0; s < numSlices; s++) {
        int sliceLow  = low + s * sliceSize;
        int sliceHigh = sliceLow + sliceSize - 1;
        if (sliceLow > high) break;
        if (sliceHigh > high) sliceHigh = high;

        int sliceCount = sliceHigh - sliceLow + 1;
        if (sliceCount <= 0) continue;

        // Sort this slice by Y
        qsort(&rectArr[sliceLow], (size_t)sliceCount, sizeof(Rect), compareByYCenter);

        // Pack into leaves of size BUNDLEFACTOR
        for (int i = sliceLow; i <= sliceHigh; i += BUNDLEFACTOR) {
            int end = i + BUNDLEFACTOR - 1;
            if (end > sliceHigh) end = sliceHigh;
            current_level[leafCount++] = createLeaf_STR(rectArr, i, end);
        }
    }
    int currCount = leafCount;   // == countedLeaves

    // -------- Build upper levels (like your original) --------
    while (currCount > 1) {
        int numParents = (currCount + FANOUT - 1) / FANOUT;
        Node **next_level = (Node **)malloc((size_t)numParents * sizeof(Node *));

        for (int i = 0; i < numParents; i++) {
            int start = i * FANOUT;
            int end   = (start + FANOUT < currCount) ? (start + FANOUT) : currCount;

            Node *parent = (Node *)malloc(sizeof(Node));
            parent->isLeaf = 0;
            parent->rects = NULL;                                // <-- important
            parent->count = end - start;
            parent->children = (Node **)malloc((size_t)parent->count * sizeof(Node *));
            initMBR(&parent->mbr);

            for (int j = start; j < end; j++) {
                parent->children[j - start] = current_level[j];
                parent->mbr = unionJoin(&parent->mbr, &current_level[j]->mbr);
            }
            next_level[i] = parent;
        }

        free(current_level);
        current_level = next_level;
        currCount = numParents;
    }

    Node *root = current_level[0];
    free(current_level);
    return root;
}



/*
Node *createRTree(Rect *rectArr, int low, int high)
{
    int total = high - low + 1;
    if (total <= 0)
        return NULL;

    // ---------- STR leaf packing ---------- 
    // 1) Sort by X once on the whole range 
    qsort(&rectArr[low], total, sizeof(Rect), compareByXCenter);

    // Target leaf capacity and expected leaf count 
    const int leaf_cap = BUNDLEFACTOR;
    int numLeaves = (total + leaf_cap - 1) / leaf_cap;
    if (numLeaves < 1) numLeaves = 1;

    // Choose number of X-slabs ~ sqrt(numLeaves) (classic STR heuristic) 
    int S = (int)ceil(sqrt((double)numLeaves));
    if (S < 1) S = 1;

    // Allocate leaf node array up-front 
    Node **current_level = (Node **)malloc(numLeaves * sizeof(Node *));
    int leaf_idx = 0;

    // 2) Split into S X-slabs; within each slab, sort by Y and pack leaves 
    int slab_size = (total + S - 1) / S; // ceil(total / S) 
    for (int s = 0; s < S; s++) {
        int slab_low  = low + s * slab_size;
        int slab_high = (s == S - 1) ? high : (slab_low + slab_size - 1);
        if (slab_low > high) break;
        if (slab_high > high) slab_high = high;
        if (slab_low > slab_high) continue;

        // Sort this slab by Y 
        int slab_count = slab_high - slab_low + 1;
        qsort(&rectArr[slab_low], slab_count, sizeof(Rect), compareByYCenter);

        // Pack leaves inside this slab in chunks of BUNDLEFACTOR 
        for (int start = slab_low; start <= slab_high; start += leaf_cap) {
            int end = start + leaf_cap - 1;
            if (end > slab_high) end = slab_high;

            // Safety: if we somehow produced more leaves than planned, grow array 
            if (leaf_idx >= numLeaves) {
                // grow by 50% //
                int new_cap = (int)(numLeaves * 1.5) + 1;
                current_level = (Node **)realloc(current_level, new_cap * sizeof(Node *));
                numLeaves = new_cap;
            }
            current_level[leaf_idx++] = createLeaf(rectArr, start, end);
        }
    }

    // Fix the actual number of leaves in case we over-allocated 
    int n_nodes = leaf_idx;

    // Edge case: if everything fit in one leaf, return it 
    if (n_nodes == 1) {
        Node *root = current_level[0];
        free(current_level);
        return root;
    }

    // ---------- Build internal levels bottom-up with FANOUT ---------- 
    while (n_nodes > 1) {
        int numParents = (n_nodes + FANOUT - 1) / FANOUT;
        Node **next_level = (Node **)malloc(numParents * sizeof(Node *));

        for (int i = 0; i < numParents; i++) {
            int start = i * FANOUT;
            int end   = (start + FANOUT < n_nodes) ? (start + FANOUT) : n_nodes;

            Node *parent = (Node *)malloc(sizeof(Node));
            parent->isLeaf = 0;
            parent->count  = end - start;
            parent->children = (Node **)malloc(parent->count * sizeof(Node *));
            initMBR(&parent->mbr);

            for (int j = start; j < end; j++) {
                parent->children[j - start] = current_level[j];
                parent->mbr = unionJoin(&parent->mbr, &current_level[j]->mbr);
            }
            next_level[i] = parent;
        }

        free(current_level);
        current_level = next_level;
        n_nodes = numParents;
    }

    Node *root = current_level[0];
    free(current_level);
    return root;
}
*/


//----------------STR_Version----------------

    // --- helpers: compare nodes by MBR center ---
static int cmpNodeX(const void *A, const void *B) {
    const Node *a = *(Node *const *)A;
    const Node *b = *(Node *const *)B;
    long cxa = (long)a->mbr.xmin + a->mbr.xmax;
    long cxb = (long)b->mbr.xmin + b->mbr.xmax;
    return (cxa > cxb) - (cxa < cxb);
}
static int cmpNodeY(const void *A, const void *B) {
    const Node *a = *(Node *const *)A;
    const Node *b = *(Node *const *)B;
    long cya = (long)a->mbr.ymin + a->mbr.ymax;
    long cyb = (long)b->mbr.ymin + b->mbr.ymax;
    return (cya > cyb) - (cya < cyb);
}

// Safer leaf (ensure children=NULL)
static Node *createLeaf_safe(Rect *rectArr, int low, int high) {
    Node *leaf = (Node *)malloc(sizeof(Node));
    leaf->isLeaf = 1;
    leaf->children = NULL;
    leaf->count = high - low + 1;
    leaf->rects = (Rect *)malloc((size_t)leaf->count * sizeof(Rect));
    initMBR(&leaf->mbr);
    for (int i = low; i <= high; ++i) {
        leaf->rects[i - low] = rectArr[i];
        updateMBRWithRect(&leaf->mbr, rectArr[i]);
    }
    return leaf;
}

// Group an array of Node* into parents using STR at THIS level (recursive).
// cap = max children per internal node (FANOUT).
static Node *group_nodes_STR(Node **nodes, int n, int cap) {
    if (n <= 0) return NULL;
    if (n == 1) return nodes[0];                            // nothing to group
    if (n <= cap) {                                         // single parent root
        Node *p = (Node *)malloc(sizeof(Node));
        p->isLeaf = 0;
        p->rects = NULL;
        p->count = n;
        p->children = (Node **)malloc((size_t)n * sizeof(Node *));
        initMBR(&p->mbr);
        for (int i = 0; i < n; ++i) {
            p->children[i] = nodes[i];
            p->mbr = unionJoin(&p->mbr, &nodes[i]->mbr);
        }
        return p;
    }

    // STR tiling on nodes: sort by X, slice, within slice sort by Y, then pack groups of size 'cap'.
    qsort(nodes, (size_t)n, sizeof(Node *), cmpNodeX);

    int S = (int)ceil(sqrt((double)n / cap));              // number of X-slices at this level
    if (S < 1) S = 1;
    int sliceSize = (n + S - 1) / S;

    // Pass A: count parents
    int parentCount = 0;
    for (int s = 0; s < S; ++s) {
        int sLo = s * sliceSize;
        int sHi = sLo + sliceSize; if (sHi > n) sHi = n;
        int sc  = sHi - sLo;
        if (sc <= 0) continue;
        parentCount += (sc + cap - 1) / cap;               // ceil(sc / cap)
    }

    Node **parents = (Node **)malloc((size_t)parentCount * sizeof(Node *));
    int pc = 0;

    // Pass B: build parents
    for (int s = 0; s < S; ++s) {
        int sLo = s * sliceSize;
        int sHi = sLo + sliceSize; if (sHi > n) sHi = n;
        int sc  = sHi - sLo;
        if (sc <= 0) continue;

        qsort(nodes + sLo, (size_t)sc, sizeof(Node *), cmpNodeY);

        for (int i = sLo; i < sHi; i += cap) {
            int jEnd = i + cap; if (jEnd > sHi) jEnd = sHi;
            int cnt = jEnd - i;

            Node *p = (Node *)malloc(sizeof(Node));
            p->isLeaf = 0;
            p->rects = NULL;
            p->count = cnt;
            p->children = (Node **)malloc((size_t)cnt * sizeof(Node *));
            initMBR(&p->mbr);

            for (int j = 0; j < cnt; ++j) {
                p->children[j] = nodes[i + j];
                p->mbr = unionJoin(&p->mbr, &nodes[i + j]->mbr);
            }
            parents[pc++] = p;
        }
    }

    // Recurse upward
    Node *root = group_nodes_STR(parents, pc, cap);
    free(parents);
    return root;
}

// Fully recursive STR bulk loader (leaves + all upper levels use STR tiling).
Node *createRTree_STR_2(Rect *rectArr, int low, int high)
{
    int total = high - low + 1;
    if (total <= 0) return NULL;

    // Leaf-level STR: sort by X, slice, within slice sort by Y, pack leaves of size BUNDLEFACTOR.
    qsort(&rectArr[low], (size_t)total, sizeof(Rect), compareByXCenter);

    int S = (int)ceil(sqrt((double)total / BUNDLEFACTOR)); // recommended STR formula
    if (S < 1) S = 1;
    int sliceSize = (total + S - 1) / S;

    // Count leaves (two-pass to avoid overflow)
    int leafCount = 0;
    for (int s = 0; s < S; ++s) {
        int sliceLow  = low + s * sliceSize;
        int sliceHigh = sliceLow + sliceSize - 1;
        if (sliceLow > high) break;
        if (sliceHigh > high) sliceHigh = high;
        int sc = sliceHigh - sliceLow + 1;
        if (sc <= 0) continue;
        leafCount += (sc + BUNDLEFACTOR - 1) / BUNDLEFACTOR;
    }

    Node **leaves = (Node **)malloc((size_t)leafCount * sizeof(Node *));
    int L = 0;

    for (int s = 0; s < S; ++s) {
        int sliceLow  = low + s * sliceSize;
        int sliceHigh = sliceLow + sliceSize - 1;
        if (sliceLow > high) break;
        if (sliceHigh > high) sliceHigh = high;
        int sc = sliceHigh - sliceLow + 1;
        if (sc <= 0) continue;

        qsort(&rectArr[sliceLow], (size_t)sc, sizeof(Rect), compareByYCenter);

        for (int i = sliceLow; i <= sliceHigh; i += BUNDLEFACTOR) {
            int end = i + BUNDLEFACTOR - 1;
            if (end > sliceHigh) end = sliceHigh;
            leaves[L++] = createLeaf_safe(rectArr, i, end);
        }
    }

    // Upper levels: recursively group leaves with STR using FANOUT as capacity.
    Node *root = group_nodes_STR(leaves, L, FANOUT);
    free(leaves);
    return root;
}


bool isOverlap(const MBR *mbr, Rect r)
{
   return !(r.xmax < mbr->xmin || r.xmin > mbr->xmax ||
            r.ymax < mbr->ymin || r.ymin > mbr->ymax);
}



// int searchRTree(Node *node, Rect queryRect, int q)
// {
//    int count = 0;


//    // If query does not overlap node's MBR, skip this subtree
//    if (!isOverlap(&node->mbr, queryRect))
//        return 0;


//    if (node->isLeaf)
//    {
//        // Check each rectangle in the leaf node
//        for (int i = 0; i < node->count; i++)
//        {
//            if (isOverlap((MBR *)&node->rects[i], queryRect))
//            {
//                // if (q == 6)
//                //     printf("\nQuery[%d]=[%d,%d-%d,%d] overlapped with Rect[%d,%d-%d,%d]", q, queryRect.xmin, queryRect.ymin, queryRect.xmax, queryRect.ymax, node->rects[i].xmin, node->rects[i].ymin, node->rects[i].xmax, node->rects[i].ymax);
//                 count++;
//            }
//        }
//    }
//    else
//    {
//        // Recurse into all overlapping children
//        for (int i = 0; i < node->count; i++)
//        {
//            count += searchRTree(node->children[i], queryRect, q);
//        }
//    }


//    return count;
// }

int searchRTree(Node *node, Rect queryRect, int q)
{
    if (node == NULL) return 0;

    int count = 0;

    // Prune if query doesn't overlap node MBR
    if (!isOverlap(&node->mbr, queryRect))
        return 0;

    if (node->isLeaf) {
        for (int i = 0; i < node->count; i++) {
            const Rect *r = &node->rects[i];
            if (isOverlap((const MBR *)r, queryRect)) {
                // Optional debug logging for a specific query ID
                // if (q == 6)
                //     printf("\nQuery[%d]=[%d,%d-%d,%d] overlapped with Rect[%d,%d-%d,%d]", q,
                //         queryRect.xmin, queryRect.ymin, queryRect.xmax, queryRect.ymax,
                //         r->xmin, r->ymin, r->xmax, r->ymax);
                count++;
            }
        }
    } else {
        for (int i = 0; i < node->count; i++) {
            count += searchRTree(node->children[i], queryRect, q);
        }
    }

    return count;
}
// Iterative search: avoids recursion overhead and improves I-cache usage
int searchRTree_iter(Node *root, Rect queryRect, int q)
{
    if (!root) return 0;


    Node *stack[256];
    int top = 0;
    stack[top++] = root;

    int count = 0;

    while (top) {
        Node *node = stack[--top];

        // Prune
        if (!isOverlap(&node->mbr, queryRect))
            continue;

        if (node->isLeaf) {
            // Scan leaf
            for (int i = 0; i < node->count; i++) {
                const Rect *r = &node->rects[i];
                if (isOverlap((const MBR *)r, queryRect))
                    count++;
            }
        } else {
            // Push (lightly prefetched) children
            // Optional prefetch of the first child MBR to hide latency:
            // if (node->count > 0) __builtin_prefetch(&node->children[0]->mbr, 0, 1);
            for (int i = 0; i < node->count; i++) {
                stack[top++] = node->children[i];
                // Defensive: avoid overflow if FANOUT grows
                if (top >= (int)(sizeof(stack)/sizeof(stack[0]))) {
                    // Fallback to recursive descent on overflow (very unlikely with your tree)
                    count += searchRTree(node->children[i], queryRect, q);
                    top--; // undo push
                }
            }
        }
    }
    return count;
}


// Helper function to collect statistics recursively
void collectRTreeStats(Node *node, int depth, RTreeStats *stats)
{
    if (!node)
        return;

    stats->totalNodes++;

    if (node->isLeaf)
    {
        stats->leafNodes++;
    }
    else
    {
        stats->internalNodes++;
    }

    if (depth > stats->maxDepth)
        stats->maxDepth = depth;

    if (!node->isLeaf)
    {
        for (int i = 0; i < node->count; i++)
        {
            collectRTreeStats(node->children[i], depth + 1, stats);
        }
    }
}
// Function to compute and print R-tree statistics
void printRTreeStats(Node *root)
{
    RTreeStats stats = {0, 0, 0, 0};
    collectRTreeStats(root, 1, &stats);

    printf("\n=== R-Tree Stats ===\n");
    printf("Total Nodes       : %d\n", stats.totalNodes);
    printf("Subtree Nodes     : %d\n", stats.internalNodes);
    printf("Leaf Nodes        : %d\n", stats.leafNodes);
    printf("Number of Levels  : %d\n", stats.maxDepth);
    printf("====================\n");
}



void writeTimingLog(int numRects, int numQuery, int numThreads, double seq_time_ms, double par_time_ms)
{
    // Compute speedup
    double speedup = seq_time_ms / par_time_ms;

    // Ensure Log directory exists
    mkdir("Log", 0777);

    // Format log filename (same file for each day)
    time_t rawtime;
    struct tm *timeinfo;
    char filename[100];

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(filename, sizeof(filename), "Log/%Y-%m-%d.txt", timeinfo);

    // Open log file in append mode
    FILE *log = fopen(filename, "a");
    if (log)
    {
        // Add timestamped section header
        char timestamp[64];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
        fprintf(log, "\n=== Run @ %s ===\n", timestamp);

        fprintf(log, "Dataset: %d rects, %d queries\n", numRects, numQuery);
        fprintf(log, "Threads used: %d\n", numThreads);
        fprintf(log, "Sequential Time: %.2f ms\n", seq_time_ms);
        fprintf(log, "Parallel Time:   %.2f ms\n", par_time_ms);
        fprintf(log, "Speedup: %.2fx\n", speedup);
        fprintf(log, "-------------------------\n");

        fclose(log);
        printf("📁 Timing log saved to: %s\n", filename);
    }
    else
    {
        perror("❌ Failed to open log file");
    }
}

Rect *selectDataDataset(int *numRects, int option)
{
    const char *paths[] = {
        "Data/Uniform_Box_6M_int.csv",
        "Data/mbrs_sports_999k.csv",
        "Data/sports_mbr_1.7M.csv",
        "Data/mbrs_parks_300k.csv",
        "Data/mbrs_cemetery_168k.csv",
        "Data/lakes_mbr_int.csv"};

    if (option < 1 || option > 6)
    {
        printf("Invalid option. Exiting.\n");
        exit(1);
    }

    return readRectsFromFile(paths[option - 1], numRects);
}

Rect *selectQueryDataset(int *numQuery, int dataset_option)
{
    int option = 0;

    /* Will point at the correct table for the chosen dataset */
    const char **paths = NULL;
    size_t npaths = 0;

    switch (dataset_option) {
    case 1: { /* Synthetic */
        static const char *synthetic_paths[] = {
            "Query/Synthetic_Data/Uniform_Box_1408.csv",
            "Query/Synthetic_Data/Uniform_Box_6M_int_1%.csv",
            "Query/Synthetic_Data/Uniform_Box_6M_int_5%.csv",
            "Query/Synthetic_Data/Uniform_Box_6M_int_10%.csv",
            "Query/Synthetic_Data/Uniform_Box_6M_int_25%.csv",
            "Query/Synthetic_Data/Uniform_Box_90k.csv",
            "Query/Synthetic_Data/Uniform_Box_180k.csv",
            "Query/Synthetic_Data/Uniform_Box_360k.csv",
            "Query/Synthetic_Data/Uniform_Box_720k.csv"
        };
        printf("\nChoose the Query Dataset for Synthetic Dataset:\n"
               "\t1. Uniform_Box_1408\n"
               "\t2. Uniform_Box_1%%\n"
               "\t3. Uniform_Box_5%%\n"
               "\t4. Uniform_Box_10%%\n"
               "\t5. Uniform_Box_25%%\n"
               "\t6. Uniform_Box_90k\n"
               "\t7. Uniform_Box_180k\n"
               "\t8. Uniform_Box_360k\n"
               "\t9. Uniform_Box_720k\n"
               "Enter your option (1-9): ");
        if (scanf(" %d", &option) != 1) { printf("Invalid input.\n"); exit(1); }
        paths  = synthetic_paths;
        npaths = sizeof(synthetic_paths) / sizeof(synthetic_paths[0]);
        break;
    }
    case 2: { /* Sports (999k) */
        static const char *sports_paths[] = {
            "Query/mbrs_sports_999k/mbrs_sports_999k_1%.csv",
            "Query/mbrs_sports_999k/mbrs_sports_999k_5%.csv",
            "Query/mbrs_sports_999k/mbrs_sports_999k_10%.csv",
            "Query/mbrs_sports_999k/mbrs_sports_999k_25%.csv",
            "Query/mbrs_sports_999k/mbrs_sports_999k_50%.csv",
        };
        printf("\nChoose the Query Dataset for Sports(999k):\n"
               "\t1. 1%%\n"
               "\t2. 5%%\n"
               "\t3. 10%%\n"
               "\t4. 25%%\n"
               "\t5. 50%%\n"
               "Enter your option (1-5): ");
        if (scanf(" %d", &option) != 1) { printf("Invalid input.\n"); exit(1); }
        paths  = sports_paths;
        npaths = sizeof(sports_paths) / sizeof(sports_paths[0]);
        break;
    }
    case 3: { /* Sports (1.7M) */
        static const char *sports_17_paths[] = {
            "Query/Sports_1.7M/sports_mbr_int_1%.csv",
            "Query/Sports_1.7M/sports_mbr_int_5%.csv",
            "Query/Sports_1.7M/sports_mbr_int_10%.csv",
            "Query/Sports_1.7M/sports_mbr_int_25%.csv",
        };
        printf("\nChoose the Query Dataset for Sports(1.7M):\n"
               "\t1. 1%%\n"
               "\t2. 5%%\n"
               "\t3. 10%%\n"
               "\t4. 25%%\n"
               "Enter your option (1-4): ");
        if (scanf(" %d", &option) != 1) { printf("Invalid input.\n"); exit(1); }
        paths  = sports_17_paths;
        npaths = sizeof(sports_17_paths) / sizeof(sports_17_paths[0]);
        break;
    }
    case 4: { /* Parks (300k) */
        static const char *parks_paths[] = {
            "Query/mbrs_parks_300k/mbrs_parks_300k_1%.csv",
            "Query/mbrs_parks_300k/mbrs_parks_300k_5%.csv",
            "Query/mbrs_parks_300k/mbrs_parks_300k_10%.csv",
            "Query/mbrs_parks_300k/mbrs_parks_300k_25%.csv",
            "Query/mbrs_parks_300k/mbrs_parks_300k_50%.csv"
        };
        printf("\nChoose the Query Dataset for Parks(300k):\n"
               "\t1. 1%%\n"
               "\t2. 5%%\n"
               "\t3. 10%%\n"
               "\t4. 25%%\n"
               "\t5. 50%%\n"
               "Enter your option (1-5): ");
        if (scanf(" %d", &option) != 1) { printf("Invalid input.\n"); exit(1); }
        paths  = parks_paths;
        npaths = sizeof(parks_paths) / sizeof(parks_paths[0]);
        break;
    }
    case 5: { /* Cemetery (168k) */
        static const char *cemetery_paths[] = {
            "Query/mbrs_cemetery_168k/mbrs_cemetery_168k_10%.csv",
            "Query/mbrs_cemetery_168k/mbrs_cemetery_168k_25%.csv",
            "Query/mbrs_cemetery_168k/mbrs_cemetery_168k_50%.csv",
            "Query/mbrs_cemetery_168k/mbrs_cemetery_168k_75%.csv"
        };
        printf("\nChoose the Query Dataset for Cemetery(168k):\n"
               "\t1. 10%%\n"
               "\t2. 25%%\n"
               "\t3. 50%%\n"
               "\t4. 75%%\n"
               "Enter your option (1-4): ");
        if (scanf(" %d", &option) != 1) { printf("Invalid input.\n"); exit(1); }
        paths  = cemetery_paths;
        npaths = sizeof(cemetery_paths) / sizeof(cemetery_paths[0]);
        break;
    }
    case 6: { /* Lakes (8M) */
        static const char *lakes_paths[] = {
            "Query/lakes_mbr_int/lakes_mbr_int_1%.csv",
            "Query/lakes_mbr_int/lakes_mbr_int_5%.csv",
            "Query/lakes_mbr_int/lakes_mbr_int_10%.csv",
            "Query/lakes_mbr_int/lakes_mbr_int_25%.csv",
            "Query/lakes_mbr_int/lakes_mbr_int_50%.csv"
        };
        printf("\nChoose the Query Dataset for Lakes(8M):\n"
               "\t1. 1%%\n"
               "\t2. 5%%\n"
               "\t3. 10%%\n"
               "\t4. 25%%\n"
               "\t5. 50%%\n"
               "Enter your option (1-5): ");
        if (scanf(" %d", &option) != 1) { printf("Invalid input.\n"); exit(1); }
        paths  = lakes_paths;
        npaths = sizeof(lakes_paths) / sizeof(lakes_paths[0]);
        break;
    }
    default:
        printf("Invalid dataset option. Exiting.\n");
        exit(1);
    }

    /* Validate selection */
    if (option < 1 || (size_t)option > npaths) {
        printf("Invalid option. Exiting.\n");
        exit(1);
    }

    /* Load and return the chosen query file */
    return readRectsFromFile(paths[option - 1], numQuery);
}
