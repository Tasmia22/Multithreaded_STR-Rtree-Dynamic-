#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include "rtree.h"
#include <stdatomic.h>
#include <unistd.h>
// Global shared index and mutex
int shared_index = 0;

// --- timing helpers ---
static inline double sec_since(struct timespec a, struct timespec b)
{
    return (b.tv_sec - a.tv_sec) + (b.tv_nsec - a.tv_nsec) / 1e9;
}

typedef struct
{
    int thread_id;
    Rect *queries;
    int *results;
    Node *root;
    int numQuery;
    int chunk_size;
    char pad[20]; // prevent false sharing
} ThreadArgs __attribute__((aligned(64)));

// Worker function with dynamic scheduling
void *thread_worker_dynamic(void *arg)
{
    ThreadArgs *args = (ThreadArgs *)arg;

    while (1)
    {
        int start = __sync_fetch_and_add(&shared_index, args->chunk_size);
        if (start >= args->numQuery)
            break;

        int end = start + args->chunk_size;
        if (end > args->numQuery)
            end = args->numQuery;

        // int local_count = 0;
        for (int i = start; i < end; i++)
        {
            args->results[i] = searchRTree(args->root, args->queries[i], i);
            // local_count += args->results[i];
        }
        //  printf("Thread %d processed [%d-%d), overlaps = %d\n", args->thread_id, start, end, local_count);
    }

    return NULL;
}

 void run_thread_pool_query_dynamic(Rect *query_rects, int *results, Node *root, int numQuery, int numThreads, int chunk_size)
{
    pthread_t threads[numThreads];
    ThreadArgs *args = aligned_alloc(64, numThreads * sizeof(ThreadArgs));
    if (!args)
    {
        perror("aligned_alloc failed");
        exit(EXIT_FAILURE);
    }

    // Initialize shared index and mutex
    shared_index = 0;
    //pthread_mutex_init(&index_mutex, NULL);

    for (int t = 0; t < numThreads; t++)
    {
        args[t] = (ThreadArgs){
            .thread_id = t,
            .queries = query_rects,
            .results = results,
            .root = root,
            .numQuery = numQuery,
            .chunk_size = chunk_size};
        pthread_create(&threads[t], NULL, thread_worker_dynamic, &args[t]);
    }

    for (int t = 0; t < numThreads; t++)
    {
        pthread_join(threads[t], NULL);
    }

  //  pthread_mutex_destroy(&index_mutex);
    free(args); // Free dynamically allocated thread arguments here
}

int main()
{
    struct timespec t0, t1, t2, t3, t4,t5;
    double rtree_construction_time;
    int numRects, numQuery, dataset_option = 0;
    printf("\nHow many data you want to work with? Choose option: \n\t1. 6M\n\t2. Sports(999k)\n\t3. Sports(1.7M) \n\t4. parks(300k)\n\t5. cemetery(168k)\n\t6. Lakes(8M)\n");
    printf("\nEnter your option: ");

    if (scanf(" %d", &dataset_option) != 1)
    {
        fprintf(stderr, "Invalid input. Exiting.\n");
        return EXIT_FAILURE;
    }
    Rect *rects = selectDataDataset(&numRects, dataset_option);
    if (!rects)
    {
        printf("Failed to read points.\n");
        return -1;
    }

    printf("Read %d rects successfully.\n", numRects);
    printf("Total dataset size: %.2f MB\n", (numRects * sizeof(Rect)) / (1024.0 * 1024.0));
    // R-tree construction (sequential)
    clock_gettime(CLOCK_MONOTONIC, &t0);
    //Node *root = createRTree(rects, 0, numRects - 1);
   //Node *root = createRTree_STR(rects, 0, numRects - 1);
   Node *root = createRTree_STR_2(rects, 0, numRects - 1);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    rtree_construction_time = sec_since(t0,t1);
    printf("\nR-tree construction time = %.2f s\n", rtree_construction_time);
    printRTreeStats(root);
    // Load queries
    Rect *query_rects = selectQueryDataset(&numQuery, dataset_option);

    if (!query_rects)
    {
        printf("Failed to read Query Rectangles.\n");
        return -1;
    }
    Zsorting(query_rects, numQuery);
    printf("Read %d query rects. Query data size: %.2f MB\n", numQuery, (numQuery * sizeof(Rect)) / (1024.0 * 1024.0));

    // Allocate result array
    int *cpu_overlap_count = calloc(numQuery, sizeof(int));

    // === Sequential Query Search ===
    long long found_seq = 0;
    clock_gettime(CLOCK_MONOTONIC, &t2);
    for (int i = 0; i < numQuery; i++)
    {
        // int result = searchRTree(root, query_rects[i], i);
        int result = searchRTree(root, query_rects[i], i);

        cpu_overlap_count[i] = (long long)result;
        found_seq += (long long)result;
    }

    clock_gettime(CLOCK_MONOTONIC, &t3);
    double seq_time = sec_since(t2,t3);
    printf("\n[Sequential] Overlaps = %lld, Time = %.2f s\n", found_seq, seq_time);

    // === Parallel Query Search (Thread Pool) ===
    int numThreads = sysconf(_SC_NPROCESSORS_ONLN); // returns 12
    //int numThreads = 8;
    memset(cpu_overlap_count, 0, numQuery * sizeof(int));
    clock_gettime(CLOCK_MONOTONIC, &t4);

    run_thread_pool_query_dynamic(query_rects, cpu_overlap_count, root, numQuery, numThreads, 10000);

    long long found_par = 0;
    for (int i = 0; i < numQuery; i++)
    {
        found_par += (long long)cpu_overlap_count[i];
    }
    clock_gettime(CLOCK_MONOTONIC, &t5);
    double par_time = sec_since(t4,t5);
    double speedup = seq_time / par_time;

    printf("[Parallel]   Overlaps = %lld, Time = %.2f s (Threads: %d)\n", found_par, par_time, numThreads);
    printf("⚡ Speedup = %.2fx\n", speedup);

    //  Result check
    if (found_seq != found_par)
    {
        printf("❌ Mismatch between sequential and parallel results!\n");
    }
    else
    {
        printf("✅ Results match between sequential and parallel runs.\n");
    }
    // === Write timing results to file ===
    writeTimingLog(numRects, numQuery, numThreads, seq_time, par_time);

    // Cleanup
    free(cpu_overlap_count);
    free(rects);
    free(query_rects);

    return 0;
}
