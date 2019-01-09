#ifndef COMMON_H
#define COMMON_H

#define MAX(x, y) (((x) > (y)) ? (x) : (y))

#define MAX_SOLUTION_SIZE 8

#define RES_PREFIX "11707252"
#define RINGBUFFER_ELEM_COUNT 100

typedef struct edge {
    int node_1, node_2;
} edge_t;

typedef struct solution_ringbuffer {
    int term;
    int buf_elem_counts[RINGBUFFER_ELEM_COUNT];
    edge_t buf[RINGBUFFER_ELEM_COUNT][MAX_SOLUTION_SIZE];
    int write_pos;
} solution_ringbuffer_t;

#endif