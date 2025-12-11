# Multithreaded STR R tree Query Engine

## Overview

This project implements a main memory R-tree for rectangle data and evaluates query performance in both sequential and multithreaded settings.

The code builds an R-tree using a Sort-Tile-Recursive style construction and then executes a set of range queries. Each query counts the number of rectangles that overlap the query rectangle. The same queries are executed sequentially and with a thread pool so that speedup can be measured.

`rtree.c` is the main driver of the program. The R tree data structures and helper routines are defined in `rtree.h` and implemented in `rtreefunction.c`. Query reordering by Z order is implemented in `zordering.c`.

The project is intended to run on a POSIX system with a C compiler and pthreads support.

## Repository contents

The GitHub repository contains only the source files to keep the repository small

* `rtree.c` main program  
* `rtree.h` shared data structures and function declarations  
* `rtreefunction.c` R tree construction search and statistics  
* `zordering.c` Z order sorting helpers  
* `makefile` build script  

The data query and log directories are not tracked in the repository. You must create them locally before running the program as described next. Link: https://drive.google.com/drive/folders/1-ZI3Ir65Uu5gj7Jk-a0oncUk11hB5HvP?usp=sharing

## Building

1. Open a terminal in the `Multithreaded_STRRtree_Dymamic` directory.
2. Run the command  

   `make`

   This should produce an executable named `rtree_multithreaded` in the same directory.

If the build fails check that you have a recent C compiler and the pthreads library installed.

## Running

Run the executable from the project directory

`./rtree_multithreaded`

The program first asks which dataset to use

1. Uniform boxes with about six million rectangles  
2. Sports dataset with about nine hundred ninety nine thousand rectangles  
3. Sports dataset with about one point seven million rectangles  
4. Parks dataset with about three hundred thousand rectangles  
5. Cemetery dataset with about one hundred sixty eight thousand rectangles  
6. Lakes dataset with about eight million rectangles

Based on the chosen option the program

1. Loads the corresponding rectangle dataset through `selectDataDataset`.
2. Builds an STR style R tree with `createRTree_STR_2`.
3. Prints basic tree statistics with `printRTreeStats`.
4. Loads the matching query set with `selectQueryDataset`.
5. Applies Z order sorting to the queries with `Zsorting`.
6. Executes all queries sequentially and reports total overlaps and total time.
7. Executes the same queries with a multithreaded thread pool and reports overlaps time and speedup.
8. Writes a timing line to a log file in the `Log` directory through `writeTimingLog`.

## Implementation details

### R tree construction

The tree is built in memory from an array of `Rect` structures.

`createRTree_STR_2` implements an STR style bulk load. Rectangles are partitioned recursively while honoring a fanout limit so that the final tree is reasonably balanced and spatially clustered.

`printRTreeStats` reports statistics such as the number of nodes number of leaves and the tree height.

### Query processing

Each query is a rectangle given in the same coordinate system as the data rectangles.

`searchRTree` performs a standard depth first traversal starting from the root node. It tests overlap with each node minimum bounding rectangle and descends only into subtrees whose MBR overlaps the query. At the leaves it counts the number of data rectangles that intersect the query rectangle and returns that count.

Before running the queries the code calls `Zsorting` on the query array. This reorders the query rectangles by a Z order key to improve cache locality.

### Sequential execution

The sequential loop in `main` simply calls `searchRTree` for every query and accumulates the overlap counts. Timing is measured with `clock_gettime` and converted to seconds by `sec_since`.

### Multithreaded execution

The file implements a simple thread pool with dynamic scheduling.

1. `ThreadArgs` holds per thread parameters including the shared array of queries the result array the R tree root the total number of queries and a chunk size.
2. `shared_index` is a global integer used as a work counter.
3. Each worker thread repeatedly calls `__sync_fetch_and_add` on `shared_index` to claim the next chunk of queries. This provides atomic fetch and add without an explicit mutex.
4. For each claimed chunk the thread iterates through its query range and calls `searchRTree`, writing the per query overlap count into the shared result array.
5. Threads exit when there are no queries left.

`run_thread_pool_query_dynamic` creates `numThreads` worker threads each with the same arguments except for the `thread_id` field. It then waits for all threads to finish and frees the aligned argument array.

By default `numThreads` is set to the number of online logical cores on the system as reported by `sysconf(_SC_NPROCESSORS_ONLN)`. The chunk size is currently set to ten thousand queries and can be tuned to trade off scheduling overhead against load balance.

After the parallel run the program verifies that the total overlap count matches the sequential run.

## Output and logs

During a run the program prints

1. Dataset choice and number of rectangles  
2. Size of the dataset in megabytes  
3. R tree construction time  
4. Basic tree statistics  
5. Number of queries and query memory footprint  
6. Sequential overlap count and query time  
7. Parallel overlap count query time thread count and speedup  
8. A check mark if sequential and parallel totals match or a warning if they differ



