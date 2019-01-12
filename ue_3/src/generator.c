/**
 * @file generator.c
 * @author Markus Klein (e11707252@student.tuwien.ac.at)
 * @brief Implementation of the solution generator for exercise 3.
 * @version 1.0
 * @date 2019-01-10
 * @details The generator program implements a simple randomized algorithm 
 * which calculates valid three colorings of a given graph by removing some edges.
 * The results are written to a circular buffer on a shared memory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h> 
#include <semaphore.h>
#include <signal.h>

#include "common.h"

/**
 * @brief Program name.
 * @details Name of the executable used for usage and messages.
 */
static char *progname;

/**
 * @brief Termination flag.
 * @details Indicates that the program should be terminated.
 */
static int term = 0;

/**
 * @brief Number of vertices in the given graph.
 * @details The vertex coount is implicitly passed to the program as the maximum 
 * node value of an edge.
 */
static int vertex_count;

/**
 * @brief Number of edges in the graph.
 * @details Equals to argc - 1.
 */
static int edge_count;

/**
 * @brief Parsed edges of the graph.
 * @details Points to a dynamically allocated array of edge_t objects which contain
 * the parsed graph definition.
 */
static edge_t *edges;

/**
 * @brief Semaphore of the number of solutions currently in the solution buffer.
 * @details Points to a named semaphore which indicates the number of solutions currently 
 * stored in the solution circular buffer. It is used by the generators to notify the 
 * supervisor that a new solution was added and can be read from the solution buffer.
 */
static sem_t *used_sem = NULL;

/**
 * @brief Semaphore of the number of free spaces currently in the solution buffer.
 * @details Points to a named semaphore which indicates the number of free solution spaces
 * available in the solution circular buffer. It is used by the supervisor to notify the
 * generators that a new solution can be written to the solution buffer.
 */
static sem_t *free_sem = NULL;

/**
 * @brief Semaphore for synchronization of concurrent writes to the solution buffer.
 * @details This semaphore ensures mutual exclusion for concurrent writes to the solution
 * buffer by multiple generators. 
 */
static sem_t *write_sem = NULL;

/**
 * @brief Shared memory used for storing solutions.
 * @details This shared memory object serves as communication interface between the 
 * generators and the supervisors. As demanded in the exercise description, the solution
 * storage is implemented as a circular buffer. 
 */
static solution_ringbuffer_t *solution_buf = NULL;

/**
 * Print usage. 
 * @brief Prints synopsis of the generator program.
 * 
 * @details Prints the usage message of the programm generator on sterr and terminates 
 * the program with EXIT_FAILURE.
 * Global variables: progname.
 */
static void usage(void);

/**
 * Cleanup and terminate.
 * @brief Free allocated memory, unmap shared memory, close semaphores and terminate 
 * the program with the given status code.
 * 
 * @param status Returns status of the program.
 * 
 * @details Frees allocated memory, unmaps the shared memory, closes open semaphores 
 * and exits the program using exit(), returning the given status.
 * Global variables: edges, used_sem, free_sem, write_sem, solution_buf.
 */
static void cleanup_exit(int status);

/**
 * @brief Parse the command line arguments.
 * 
 * @param argc Argument counter.
 * @param argv Argument vector.
 * 
 * @details Parses each positional command line argument into a edge_t object
 * and stores the list of edges in the edges variable. Also, the vertex_count 
 * and edge_cound variables are set accordingly. 
 * Global variables: vertex_count, edges.
 */
static void read_edges(int argc, char **argv);

/**
 * @brief Calculate a random solution.
 * 
 * @param solution Pointer to an solution array which can hold MAX_SOLUTION_SIZE 
 * edge objects. The solution will be written to this array. 
 * @return int Solution size / number of edges to be remove from the graph in 
 * order to be 3-colorable. 
 * 
 * @details Implements a randomized algorithm which finds a valid solution for 
 * the 3-coloring problem by assigning a random color to each vertex and removing
 * all edges (u, v) with color(u) == color(v). The function will discard all 
 * solutions where more than MAX_SOLUTION_SIZE edges need to be remove. In this case,
 * the function will return MAX_SOLUTION_SIZE and the solution buffer will contain
 * the first MAX_SOLUTION_SIZE edges to be remove. 
 * Global variables: vertex_count, edge_count, edges.
 */
static int find_solution(edge_t *solution);

/**
 * @brief Opens the solution ringbuffer.
 * @details Opens the shared memory used for communication with the supervisor
 * and maps to solution_buf. The function also opens all semaphores used for 
 * synchronization between the different components of the program. 
 * Global variables: solution_buf, used_sem, free_sem, write_sem.
 */
static void open_ringbuffer();

/**
 * @brief Signal handler for SIGINT and SIGTERM.
 * 
 * @param signal Caught signal.
 * 
 * @details Signal handler. Initiates the termination of the generator by setting
 * the term flag. 
 * Global variables: term.
 */
static void handle_signal(int signal);

/**
 * @brief Write a solution to the solution ringbuffer.
 * 
 * @param solution Solution to be written.
 * @param len Number of edges which need to be remove for this solution.
 * 
 * @details Writes the given solution array to the next free space into the 
 * solution ringbuffer. Blocks and waits for a free space if none is available.
 */
static void write_solution(edge_t *solution, int len);

/**
 * @brief Main method of the generator program. 
 * 
 * @param argc Argument counter.
 * @param argv Argument vector.
 * @return int Program exit code (EXIT_SUCCESS)
 * 
 * @details Sets the seed for rand(), parses the command line arguments,
 * opens the solution ringbuffer and starts continously generating random 
 * solutions which are written to the solution buffer.
 */
int main(int argc, char **argv) {
    srand(getpid());
    progname = argv[0];

    if(argc < 2) {
        usage();
    }

    // Setup shutdown handler
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    argc--;
    argv++;

    edges = malloc(argc * sizeof(edge_t));
    if(edges == NULL) {
        fprintf(stderr, "[%s] malloc failed: %s\n", progname, strerror(errno));
        cleanup_exit(EXIT_FAILURE);
    }

    memset(edges, 0, argc * sizeof(edge_t));
    read_edges(argc, argv);

    open_ringbuffer();

    edge_t solution[MAX_SOLUTION_SIZE];
    int solution_len = 0;

    while(1) {
        solution_len = find_solution(solution);
        if(solution_len == MAX_SOLUTION_SIZE) {
            continue;
        }

        if(solution_buf->term == 1 || term == 1) {
            break;
        }

        write_solution(solution, solution_len);
    }
    cleanup_exit(EXIT_SUCCESS);
}

static void handle_signal(int signal) { 
    term = 1;
}

static void read_edges(int argc, char **argv) {
    char *endptr;
    for(int i = 0; i < argc; i++) {
        errno = 0;
        edges[i].node_1 = strtol(argv[i], &endptr, 10);
        vertex_count = MAX(vertex_count, edges[i].node_1);
        if ((errno == ERANGE && (edges[i].node_1 == LONG_MAX || edges[i].node_1 == LONG_MIN))
                || (errno != 0 && edges[i].node_1 == 0)) {
            fprintf(stderr, "[%s] strtol failed: %s\n", progname, strerror(errno));
            exit(EXIT_FAILURE);
        }

        // endptr should now point to the delimiter, so increment it by 1
        endptr++;
        errno = 0;
        edges[i].node_2 = strtol(endptr, &endptr, 10);
        vertex_count = MAX(vertex_count, edges[i].node_2);
        if ((errno == ERANGE && (edges[i].node_2 == LONG_MAX || edges[i].node_2 == LONG_MIN))
                || (errno != 0 && edges[i].node_2 == 0)) {
            fprintf(stderr, "[%s] strtol failed: %s\n", progname, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }

    // vertex indices start with zero
    vertex_count++;
    edge_count = argc;
}

static int find_solution(edge_t *solution) {
    memset(solution, 0, sizeof(*solution) * MAX_SOLUTION_SIZE);
    int solution_len = 0;
    uint8_t *colors = malloc(vertex_count);
    if(colors == NULL) {
        fprintf(stderr, "[%s] malloc failed: %s\n", progname, strerror(errno));
        cleanup_exit(EXIT_FAILURE);
    }

    // Generate random coloring
    for(int i = 0; i < vertex_count; i++) {
        colors[i] = rand() % 3;
    }

    // Collect list of edges to remove
    for(int i = 0; i < edge_count; i++) {
        edge_t *edge = edges + i;
        if(colors[edge->node_1] == colors[edge->node_2]) {
            memcpy(solution + solution_len, edge, sizeof(edge_t));
            solution_len++;
        }

        if(solution_len >= MAX_SOLUTION_SIZE) {
            break;
        }
    }

    free(colors);
    return solution_len;
}

static void open_ringbuffer() {
    int shmfd = shm_open(RES_PREFIX, O_RDWR, 0);
     if(shmfd == -1) {
        if(errno == ENOENT) {
            fprintf(stderr, "[%s] Shared memory not found. Maybe there is no supervisor running?\n", progname);    
        } else {
            fprintf(stderr, "[%s] shm_open failed: %s\n", progname, strerror(errno));
        }
        cleanup_exit(EXIT_FAILURE);
    }

    solution_buf = mmap(NULL, sizeof(*solution_buf), PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    if(solution_buf == MAP_FAILED) {
        fprintf(stderr, "[%s] mmap failed: %s\n", progname, strerror(errno));
        close(shmfd);
        cleanup_exit(EXIT_FAILURE);
    }

    if(close(shmfd) < 0) {
        fprintf(stderr, "[%s] close failed: %s\n", progname, strerror(errno));
        cleanup_exit(EXIT_FAILURE);
    }

    // Open semaphores
    if((used_sem = sem_open(RES_PREFIX "_used", 0)) == SEM_FAILED) {
        fprintf(stderr, "[%s] sem_open failed: %s\n", progname, strerror(errno));
        cleanup_exit(EXIT_FAILURE);
    }
    if((free_sem = sem_open(RES_PREFIX "_free", 0)) == SEM_FAILED) {
        fprintf(stderr, "[%s] sem_open failed: %s\n", progname, strerror(errno));
        cleanup_exit(EXIT_FAILURE);
    }
    if((write_sem = sem_open(RES_PREFIX "_write", 0)) == SEM_FAILED) {
        fprintf(stderr, "[%s] sem_open failed: %s\n", progname, strerror(errno));
        cleanup_exit(EXIT_FAILURE);
    }
}

static void write_solution(edge_t *solution, int len) {
    if(sem_wait(free_sem) < 0) {
        if(errno == EINTR) {
            return;
        }

        fprintf(stderr, "[%s] sem_wait failed: %s\n", progname, strerror(errno)); 
        cleanup_exit(EXIT_FAILURE);
    }
    if(sem_wait(write_sem) < 0) {
        if(errno == EINTR) {
            return;
        }

        fprintf(stderr, "[%s] sem_wait failed: %s\n", progname, strerror(errno)); 
        cleanup_exit(EXIT_FAILURE);
    }

    // Only write solution of the supervisor has not terminated jet. 
    // Otherwise, just post the semophores. 
    if(solution_buf->term == 1) {
        // unblock the next generator
        if(sem_post(write_sem) < 0) {
            fprintf(stderr, "[%s] sem_post failed: %s\n", progname, strerror(errno)); 
            cleanup_exit(EXIT_FAILURE);
        }
        if(sem_post(free_sem) < 0) {
            fprintf(stderr, "[%s] sem_post failed: %s\n", progname, strerror(errno)); 
            cleanup_exit(EXIT_FAILURE);
        }
        return;
    }

    memcpy(solution_buf->buf[solution_buf->write_pos], solution, sizeof(*solution) * MAX_SOLUTION_SIZE);
    solution_buf->buf_elem_counts[solution_buf->write_pos] = len;
    
    solution_buf->write_pos = (solution_buf->write_pos + 1) % RINGBUFFER_ELEM_COUNT;

    if(sem_post(write_sem) < 0) {
        fprintf(stderr, "[%s] sem_post failed: %s\n", progname, strerror(errno)); 
        cleanup_exit(EXIT_FAILURE);
    }
    if(sem_post(used_sem) < 0) {
        fprintf(stderr, "[%s] sem_post failed: %s\n", progname, strerror(errno)); 
        cleanup_exit(EXIT_FAILURE);
    }
}

static void usage(void) {
    fprintf(stderr, "Usage: %s EDGE1...\n", progname);
    exit(EXIT_FAILURE);
}

static void cleanup_exit(int status) {
    free(edges);

    if(solution_buf != NULL) {
        munmap(solution_buf, sizeof(*solution_buf));
    }
    if(used_sem != NULL) {
        sem_close(used_sem);
    }
    if(free_sem != NULL) {
        sem_close(free_sem);
    }
    if(write_sem != NULL) {
        sem_close(write_sem);
    }

    exit(status);
}