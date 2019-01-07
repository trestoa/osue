#include <stdio.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <fcntl.h>
#include <sys/mman.h> 
#include <sys/types.h> 
#include <unistd.h>
#include <semaphore.h>
#include <limits.h>
#include <string.h>

#include "common.h"

/**
 * @brief Program name.
 * @details Name of the executable used for usage and error messages.
 */
static char *progname;

static sem_t *used_sem, *free_sem;

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

static void setup_ringbuffer();

static int read_solution(edge_t **solution);

int main(int argc, char **argv) {
    progname = argv[0];

    if(argc > 1) {
        usage();
    }

    setup_ringbuffer();

    edge_t *cur_solution;
    int cur_solution_len, best_solution_len = INT_MAX;
    do {
        cur_solution_len = read_solution(&cur_solution);
        if(cur_solution_len < best_solution_len) {
            printf("[%s] Solution with %u edges:", progname, cur_solution_len);
            for(int i = 0; i < cur_solution_len; i++) {
                printf(" %u-%u", cur_solution[i].node_1, cur_solution[i].node_1);
            }
            printf("\n");

            best_solution_len = cur_solution_len;
        }
    } while(cur_solution_len > 0);

    if(best_solution_len == 0) {
        printf("The graph is 3-colorable!");
    }
}

static void setup_ringbuffer() {
    // Open shared memory
    int shmfd;
    if((shmfd = shm_open(RES_PREFIX, O_RDWR | O_CREAT, 0600)) == -1) {
        fprintf(stderr, "[%s] shm_open failed: %s\n", progname, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if(ftruncate(shmfd, sizeof(*solution_buf)) < 0) {
        fprintf(stderr, "[%s] ftruncate failed: %s\n", progname, strerror(errno));
        exit(EXIT_FAILURE);
    }

    solution_buf = mmap(NULL, sizeof(*solution_buf), PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    memset(solution_buf, 0, sizeof(*solution_buf));
    if(solution_buf == MAP_FAILED) {
        fprintf(stderr, "[%s] mmap failed: %s\n", progname, strerror(errno));
        exit(EXIT_FAILURE);
    }

    close(shmfd);

    // Open semaphores
    used_sem = sem_open(RES_PREFIX "_used", O_CREAT, 0600, 0);
    free_sem = sem_open(RES_PREFIX "_free", O_CREAT, 0600, RINGBUFFER_ELEM_COUNT-1);
}

static int read_solution(edge_t **solution) {
    static int read_pos = 0;
    sem_wait(used_sem);
    *solution = solution_buf->buf[read_pos];
    sem_post(free_sem);
    read_pos = (read_pos + 1) % RINGBUFFER_ELEM_COUNT;
    return solution_buf->buf_elem_counts[read_pos];
}

static void usage(void) {
    fprintf(stderr, "Usage: %s\n", progname);
    exit(EXIT_FAILURE);
}

