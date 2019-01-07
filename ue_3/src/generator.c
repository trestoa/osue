#include <stdio.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <time.h>
#include <fcntl.h>
#include <sys/mman.h> 
#include <semaphore.h>

#include "common.h"

/**
 * @brief Program name.
 * @details Name of the executable used for usage and error messages.
 */
static char *progname;

static int vertex_count;
static int edge_count;
static edge_t *edges;

static sem_t *used_sem, *free_sem, *write_sem;

static solution_ringbuffer_t *solution_buf;

/**
 * Print usage. 
 * @brief Prints synopsis of the mydiff program.
 * 
 * @details Prints the usage message of the programm mydiff on sterr and terminates 
 * the program with EXIT_FAILURE.
 * Global variables: progname.
 */
static void usage(void);

/**
 * Cleanup and terminate.
 * @brief Close open files and terminate program with the given status code.
 * 
 * @param status Returns status of the program.
 * 
 * @details Closes open files (file pointers != NULL) and exits the program with 
 * exit(), returning the given status.
 * Global variables: outfile, file1, file2.
 */
void cleanup_exit(int status);

static void read_edges(int argc, char **argv);

static int find_solution(edge_t *solution);

static void open_ringbuffer();

static void write_solution(edge_t *solution, int len);

int main(int argc, char **argv) {
    srand(time(NULL));
    progname = argv[0];

    if(argc < 2) {
        usage();
    }

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

        for(int i = 0; i < solution_len; i++) {
            printf(" %u-%u", solution[i].node_1, solution[i].node_2);
        }
        puts("\n");

        write_solution(solution, solution_len);
    }
    printf("solution size: %u", solution_len);
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
        fprintf(stderr, "[%s] shm_open failed: %s\n", progname, strerror(errno));
        close(shmfd);
        exit(EXIT_FAILURE);
    }

    solution_buf = mmap(NULL, sizeof(*solution_buf), PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    if(solution_buf == MAP_FAILED) {
        fprintf(stderr, "[%s] mmap failed: %s\n", progname, strerror(errno));
        close(shmfd);
        exit(EXIT_FAILURE);
    }

    close(shmfd);

    // Open semaphores
    used_sem = sem_open(RES_PREFIX "_used", 0);
    free_sem = sem_open(RES_PREFIX "_free", 0);
    write_sem = sem_open(RES_PREFIX "_write", O_CREAT, 0600, 1);
}

static void write_solution(edge_t *solution, int len) {
    printf("writing to pos: %u\n", solution_buf->write_pos);
    sem_wait(free_sem);
    sem_wait(write_sem);
    
    memcpy(&(solution_buf->buf) + solution_buf->write_pos, solution, MAX_SOLUTION_SIZE);
    solution_buf->buf_elem_counts[solution_buf->write_pos] = len;
    
    solution_buf->write_pos = (solution_buf->write_pos + 1) % RINGBUFFER_ELEM_COUNT;
    sem_post(write_sem);
    sem_post(used_sem);
}

static void usage(void) {
    fprintf(stderr, "Usage: %s EDGE1...\n", progname);
    exit(EXIT_FAILURE);
}

void cleanup_exit(int status) {
    free(edges);
    exit(status);
}