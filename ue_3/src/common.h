/**
 * @file common.h
 * @author Markus Klein (e11707252@student.tuwien.ac.at)
 * @brief This module contains macros and struct definitions used by both
 * supervisor and generator.
 * @version 1.0
 * @date 2019-01-10
 * @details Contains common configurations of the program, along with the 
 * helper macro MAX and the struct definitions for objects stored in the 
 * shared memory.
 */

#ifndef COMMON_H
#define COMMON_H

/**
 * @brief Maximum function
 * @details Returns the larger value of two values compareable with the '>' operator.
 */
#define MAX(x, y) (((x) > (y)) ? (x) : (y))

/**
 * @brief Maximum solution size.
 * @details Sets the maximum number of edges which can be removed from the graph
 * in order to be considered as solution.
 */
#define MAX_SOLUTION_SIZE 8

/**
 * @brief Resource prefix.
 * @details Prefix used for all named resources (semaphores and shared memory).
 */
#define RES_PREFIX "11707252"

/**
 * @brief Ringbuffer size.
 * @details Number of element which can be stored in the solution ringbuffer.
 */
#define RINGBUFFER_ELEM_COUNT 56

/**
 * @brief Edge struct
 * @details Struct used to represent edges.
 */
typedef struct edge {
    int node_1, node_2;
} edge_t;

/**
 * @brief Structure of the shared memory.
 * @details Contains the structure for the solution ringsbuffer as well as
 * as the term variable which is used to notify the generators on supervisor 
 * shutdown. Each solution consists of the edge set in buf and the solution 
 * length in buf_elem_counts.
 */
typedef struct solution_ringbuffer {
    int term;
    int buf_elem_counts[RINGBUFFER_ELEM_COUNT];
    edge_t buf[RINGBUFFER_ELEM_COUNT][MAX_SOLUTION_SIZE];
    int write_pos;
} solution_ringbuffer_t;

#endif