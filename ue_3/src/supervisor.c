/**
 * @file supervisor.c
 * @author Markus Klein (e11707252@student.tuwien.ac.at)
 * @brief Implementaion of the supervisor program for exercise 3.
 * @version 1.0
 * @date 2019-01-10
 * @details The supervisor collects the solutions from the generator processes
 * and keeps track of the best solution. Whenever a new, better solution was found
 * it will be written to stdout. 
 */

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
#include <signal.h>

#include "common.h"

/**
 * @brief Program name.
 * @details Name of the executable used for usage and messages.
 */
static char *progname;

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
 * @brief Shared memory where solutions are written to by the generators.
 * @details This shared memory object serves as communication interface between the 
 * generators and the supervisors. As demanded in the exercise description, the solution
 * storage is implemented as a circular buffer. 
 */
static solution_ringbuffer_t *solution_buf;

/**
 * Print usage. 
 * @brief Prints synopsis of the supervisor program.
 * 
 * @details Prints the usage message of the programm supervisor on sterr and terminates 
 * the program with EXIT_FAILURE.
 * Global variables: progname.
 */
static void usage(void);

/**
 * Cleanup and terminate.
 * @brief Free allocated memory, unmap and unlink shared memory, close and unlink semaphores,
 * notify the generators of the termination and terminate the program with the given status code.
 * 
 * @param status Returns status of the program.
 * 
 * @details Frees allocated memory, notifies the generators of the termination be setting the 
 * term flag, unmaps and unlinks the shared memory, closes and unlinks open semaphores and 
 * exits the program using exit(), returning the given status.
 * Global variables: edges, used_sem, free_sem, write_sem, solution_buf.
 */
static void cleanup_exit(int status);

/**
 * @brief Signal handler for SIGINT and SIGTERM.
 * 
 * @param signal Caught signal.
 * 
 * @details Signal handler. Initiates the termination of the program by setting
 * the term flag in the shared memory. The supervisor will finish processing the current
 * solution and terminate afterwards.
 * Global variables: solution_buf.
 */
static void handle_signal(int signal);

/**
 * @brief Creates and opens the solution ringbuffer.
 * @details Creates the shared memory used for communication with the supervisor
 * and maps to solution_buf. The function also creates all semaphores used for 
 * synchronization between the different components of the program. 
 * Global variables: solution_buf, used_sem, free_sem, write_sem.
 */
static void setup_ringbuffer();

/**
 * @brief Read a solution from the solution ringbuffer.
 * 
 * @param solution Pointer where the address of the solution array should be stored.
 * @return int Size of the solution.
 * 
 * @details Reads a solution from the solution buffer and stores the address to the 
 * solution in the given pointer. Blocks and waits for the next solution if non is
 * available.
 */
static int read_solution(edge_t **solution);

/**
 * @brief Main method of the supervisor program. 
 * 
 * @param argc Argument counter.
 * @param argv Argument vector.
 * @return int Program exit code (EXIT_SUCCESS)
 * 
 * @details Parses the command line arguments, creates the solution ringbuffer and
 * reading solutions until either a SIGINT or SIGTERM is caught or one of the
 * generators finds a solution where 0 edges need to be removed.
 */
int main(int argc, char **argv) {
    progname = argv[0];

    if(argc > 1) {
        usage();
    }

    // Setup shutdown handler
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    setup_ringbuffer();

    edge_t *cur_solution;
    int cur_solution_len, best_solution_len = INT_MAX;
    do {
        cur_solution_len = read_solution(&cur_solution);
        if(cur_solution_len == -1) {
            break;
        }

        if(cur_solution_len < best_solution_len) {
            if(cur_solution_len > 0) {
                printf("[%s] Solution with %u edges:", progname, cur_solution_len);
                for(int i = 0; i < cur_solution_len; i++) {
                    printf(" %u-%u", cur_solution[i].node_1, cur_solution[i].node_2);
                }
                printf("\n");
            } else {
                solution_buf->term = 1;
            }
            
            best_solution_len = cur_solution_len;
        }
    } while(solution_buf->term == 0);

    if(best_solution_len == 0) {
        printf("[%s] The graph is 3-colorable!\n", progname);
    }

    cleanup_exit(EXIT_SUCCESS);
}

static void setup_ringbuffer() {
    // Open shared memory
    int shmfd;
    if((shmfd = shm_open(RES_PREFIX, O_RDWR | O_CREAT, 0600)) == -1) {
        fprintf(stderr, "[%s] shm_open failed: %s\n", progname, strerror(errno));
        cleanup_exit(EXIT_FAILURE);
    }

    if(ftruncate(shmfd, sizeof(*solution_buf)) < 0) {
        fprintf(stderr, "[%s] ftruncate failed: %s\n", progname, strerror(errno));
        close(shmfd);
        cleanup_exit(EXIT_FAILURE);
    }

    solution_buf = mmap(NULL, sizeof(*solution_buf), PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    memset(solution_buf, 0, sizeof(*solution_buf));
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
    if((used_sem = sem_open(RES_PREFIX "_used", O_CREAT, 0600, 0)) == SEM_FAILED) {
        fprintf(stderr, "[%s] sem_open failed: %s\n", progname, strerror(errno));
        cleanup_exit(EXIT_FAILURE);
    }
    if((free_sem = sem_open(RES_PREFIX "_free", O_CREAT, 0600, RINGBUFFER_ELEM_COUNT-1)) == SEM_FAILED) {
        fprintf(stderr, "[%s] sem_open failed: %s\n", progname, strerror(errno));
        cleanup_exit(EXIT_FAILURE);
    }
    if((write_sem = sem_open(RES_PREFIX "_write", O_CREAT, 0600, 1)) == SEM_FAILED) {
        fprintf(stderr, "[%s] sem_open failed: %s\n", progname, strerror(errno));
        cleanup_exit(EXIT_FAILURE);
    }
}

static int read_solution(edge_t **solution) {
    static int read_pos = 0;
    if(sem_wait(used_sem) < 0) {
        if(errno == EINTR) {
            return -1;
        }

        fprintf(stderr, "[%s] sem_wait failed: %s\n", progname, strerror(errno)); 
        cleanup_exit(EXIT_FAILURE);
    }
    *solution = solution_buf->buf[read_pos];
    if(sem_post(free_sem) < 0) {
        fprintf(stderr, "[%s] sem_post failed: %s\n", progname, strerror(errno)); 
        cleanup_exit(EXIT_FAILURE);
    }

    int len = solution_buf->buf_elem_counts[read_pos];
    read_pos = (read_pos + 1) % RINGBUFFER_ELEM_COUNT;
    return len;
}

static void usage(void) {
    fprintf(stderr, "Usage: %s\n", progname);
    exit(EXIT_FAILURE);
}

static void handle_signal(int signal) {
    solution_buf->term = 1;
}

static void cleanup_exit(int status) {
    if(solution_buf != NULL) {
        munmap(solution_buf, sizeof(*solution_buf));
    }
    if(shm_unlink(RES_PREFIX) < 0) {
        fprintf(stderr, "[%s] shm_unlink failed: %s\n", progname, strerror(errno));
    }

    // post free and write semaphores in order to unblock generators
    if(sem_post(free_sem)) {
        fprintf(stderr, "[%s] sem_post failed: %s\n", progname, strerror(errno));
    }
    if(sem_post(write_sem)) {
        fprintf(stderr, "[%s] sem_post failed: %s\n", progname, strerror(errno));
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
    if(sem_unlink(RES_PREFIX "_used") < 0) {
        fprintf(stderr, "[%s] sem_unlink failed: %s\n", progname, strerror(errno));
    }
    if(sem_unlink(RES_PREFIX "_free") < 0) {
        fprintf(stderr, "[%s] sem_unlink failed: %s\n", progname, strerror(errno));
    }
    if(sem_unlink(RES_PREFIX "_write") < 0) {
        fprintf(stderr, "[%s] sem_unlink failed: %s\n", progname, strerror(errno));
    }

    exit(status);
}