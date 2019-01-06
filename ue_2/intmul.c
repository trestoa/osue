/**
 * @file intmul.c
 * @author Markus Klein (e11707252@student.tuwien.ac.at)
 * @brief Main module for exercise 2.
 * @version 1.0
 * @date 2018-12-14
 * 
 * @details This module contains the implementation of the program for 
 * multiplying two large integers of equal length.  It uses an recursive 
 * algorithm which splits the multiplication into four (smaller) subproducts 
 * which are afterwards added together. More details on the algorithm can 
 * be found in the exercise description. 
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

/**
 * @brief Array of structs for storing data of a child process. 
 * @details This array contains a struct object for each of the
 * four child processes. The object stores information such as the 
 * child's process id, the io pipes as well as its result. 
 */
static struct child_process {
    int pid;
    int pipe_in_fd[2];
    int pipe_out_fd[2];
    unsigned char *res;
    int res_len;
} children[4];

/**
 * @brief Program name.
 * @details Name of the executable used for usage and error messages.
 */
static char *progname;

/**
 * @brief First input number.
 * Hex string representation of the first factor of the multiplication.
 */
static char *a = NULL;

/**
 * @brief Second input number.
 * Hex string representation of the second factor of the multiplication.
 */
static char *b = NULL;

/**
 * Print usage. 
 * @brief Prints synopsis of the intmul program.
 * 
 * @details Prints the usage message of the programm intmul on sterr and terminates 
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
 * @details Closes open files (file descriptor == -1)  and exits the program with 
 * exit(), returning the given status.
 * Global variables: a, b, children.
 */
static void cleanup_exit(int status);

/**
 * @brief Reads the multiplication factors from stdin.
 * @details Reads two lines from stdin using getline and stores the resulting strings
 * to the variable a (for the first line) and b (for the second line).
 * Global variables: a, b.
 */
static void read_nums(void);

/**
 * @brief Contains the actual implementation of the multiplication algoritm.
 * @details Multiplies the integers represented by the hex strings stored in a and b using
 * the algorithm desribed in the exercise description. The numbers are recursively split into
 * halfs with a new process being forked for each combination until both numbers consist of 
 * only a single digit. In this case, the result is directly calculated and written to stdout. 
 * Otherwise, the results of the sub-processes are join digit by digit and also written to stdout.
 * Global variables: a, b, children.
 */
static void multiply(void);

/**
 * @brief Calculates the result of the multiplication from the four sub-results.
 * @details This function calculates the sum of the results (which is equal to the 
 * product of the input numbers) of the sub processes by stepping through the sub 
 * results digit by digit. For a correct calculation, three of the sub results need 
 * to left-shifted a few digits. In order to correctly approximate the result length,
 * the fact that a product of two numbers of n digit can only have 2*n digits is used.
 * 
 * @param len Length of the input numbers.
 */
static void calculate_res(int len);

/**
 * @brief Reads the results of a child process to a the given child process struct.
 * @details Reads the result of child process from the child's stdout pipe and stores 
 * the pointer to the result buffer in the 'res' field of the given child process struct.
 * The digit count of the result is written to the 'res_len' field. However, the result 
 * buffer will always have a length of 'len' regardless of the actual digit count.
 * 
 * @param child child process struct where the result should be written to.
 * @param len Length of the input numbers (use for determining the size of the result buffer).
 */
static void read_child_res(struct child_process *child, int len);

/**
 * @brief Extracts a digit from the hex string representation of a integer number.
 * @details Reads a single hex digit from the given hex string representation of an 
 * integer number and converts it to an integer using strtol. The resulting number 
 * is therefore in the range from 0 to 15.
 * 
 * @param num Hex string representation of the number.
 * @param idx Index of the digit which should be extracted.
 * @return int Converted hex digit as integer.
 */
static inline int extract_digit(uint8_t* num, int idx);

/**
 * @brief Forks the process, changes stdin and stdout to the given fd's and exec's the 
 * the given intmul program from the child process.
 * @details Forks the process, changes the child's stdout and stdin to the given 
 * out_fd and in_fd file descriptors and eventually exec's the intmul program from the
 * child process. The read in and write out pipe ends of the parent are close. 
 * For the parent, this function returns the child's process id.
 * 
 * @param in_fd Pipe for the child's stdin.
 * @param out_fd Pipe for the child's stdout.
 * @return pid_t child process id.
 */
static pid_t fork_setup_pipes(int in_fd[2], int out_fd[2]);

/**
 * @brief Writes a buffer to a file descriptor.
 * @detais Writes 'len' bytes of buffer to a file descriptor. This functions is
 * essentially a wrapper around fdopen, fwrite and fclose. The FILE objects is saved as
 * a static variable within the function in order to allow subsequent calls of this function. 
 * Writing to the previous stream can be signalled by passing 0 as the file descriptor ('fd').
 * If a function call is the last write operation for the current stream, the 'do_close' parameter
 * must be set to 1 so that the stream will be closed (which also causes the stream to be flushed) 
 * correctly.
 * 
 * @param fd Target file descriptor.
 * @param data Buffer to be written.
 * @param len Number of bytes to be written to the target.
 * @param do_close 1 indicates that there will be no further write operators to this fd 
 * and causes the stream to be closed after the function call. A value != 1 signals that the opened 
 * stream be kept open for the next function call. 
 */
static void write_pipe(int fd, char *data, int len, int do_close);

/**
 * Main method for the intmul program.
 * @brief Program entry point. Checks the command line arguments and executes 
 * the methods for reading the input numbers and performing the calculation.
 * 
 * @param argc Argument counter.
 * @param argv Argument vector.
 * @return int Program exit code (EXIT_SUCCESS)
 * 
 * @details Makes sure that no command line argument are given, initializes the
 * child process array and subsequently performs the integer multication by calling
 * the according functions.
 * Global variables: progname, children.
 */
int main(int argc, char **argv) {
    progname = argv[0];

    if(argc != 1) {
        usage();
    }

    memset(children, 0, sizeof(children));

    read_nums();

    multiply();

    cleanup_exit(EXIT_SUCCESS);
}

static void read_nums(void) {
    size_t linecap = 0;
    ssize_t linelen;

    if((linelen = getline(&a, &linecap, stdin)) <= 0) {
        if(feof(stdin) != 0) {
            cleanup_exit(EXIT_FAILURE);
        }
        fprintf(stderr, "[%s] getline failed: %s\n", progname, strerror(ferror(stdin)));
        cleanup_exit(EXIT_FAILURE);
    }

    linecap = 0;
    if((linelen = getline(&b, &linecap, stdin)) <= 0) {
        if(feof(stdin) != 0) {
            cleanup_exit(EXIT_FAILURE);
        }
        fprintf(stderr, "[%s] getline failed: %s\n", progname, strerror(ferror(stdin)));
        cleanup_exit(EXIT_FAILURE);
    }
}

static void multiply(void) {
    int alen = strlen(a);
    if(a[alen-1] == '\n') {
        alen--;
    }
    int blen = strlen(b);
    if(b[blen-1] == '\n') {
        blen--;
    }

    if(alen != blen) {
        fprintf(stderr, "[%s] the two integers do not have equal length (%u and %u)\n", progname, alen, blen);
        cleanup_exit(EXIT_FAILURE);
    }

    if(alen == 1) {
        int res = extract_digit((uint8_t*) a, 0) * extract_digit((uint8_t*) b, 0);
        fprintf(stdout, "%x\n", res);
    } else if(alen > 1) {
        if(alen % 2 != 0) {
            fprintf(stderr, "[%s] the number of digits is not even\n", progname);
            cleanup_exit(EXIT_FAILURE);
        }

        if((pipe(children[0].pipe_in_fd) | pipe(children[0].pipe_out_fd) | 
            pipe(children[1].pipe_in_fd) | pipe(children[1].pipe_out_fd) |
            pipe(children[2].pipe_in_fd) | pipe(children[2].pipe_out_fd) | 
            pipe(children[3].pipe_in_fd) | pipe(children[3].pipe_out_fd)) != 0) {
            fprintf(stderr, "[%s] failed to init pipes\n", progname);
            cleanup_exit(EXIT_FAILURE);
        }

        // Write inputs for child process
        children[0].pid = fork_setup_pipes(children[0].pipe_in_fd, children[0].pipe_out_fd);
        write_pipe(children[0].pipe_in_fd[1], a, alen/2, 0);
        write_pipe(0, b, alen/2, 1);
        children[0].pipe_in_fd[1] = -1;

        children[1].pid = fork_setup_pipes(children[1].pipe_in_fd, children[1].pipe_out_fd);
        write_pipe(children[1].pipe_in_fd[1], a, alen/2, 0);
        write_pipe(0, b + alen/2, alen/2, 1);
        children[1].pipe_in_fd[1] = -1;

        children[2].pid = fork_setup_pipes(children[2].pipe_in_fd, children[2].pipe_out_fd);
        write_pipe(children[2].pipe_in_fd[1], a + alen/2, alen/2, 0);
        write_pipe(0, b, alen/2, 1);
        children[2].pipe_in_fd[1] = -1;

        children[3].pid = fork_setup_pipes(children[3].pipe_in_fd, children[3].pipe_out_fd);
        write_pipe(children[3].pipe_in_fd[1], a + alen/2, alen/2, 0);
        write_pipe(0, b + alen/2, alen/2, 1);
        children[3].pipe_in_fd[1] = -1;

    
        // Allocate result buffers, open child output streams and read results
        for(int i = 0; i < 4; i++) {
            read_child_res(&children[i], alen);
        }

        calculate_res(alen);

        int status, child_err = 0;
        for(int i = 0; i < 4; i++) {
            waitpid(children[i].pid, &status, 0);
            if(WEXITSTATUS(status) != EXIT_SUCCESS) {
                fprintf(stderr, "[%s] child exited with status %u\n", progname, WEXITSTATUS(status));
                child_err = 1;
            }
        }

        if(child_err != 0) {
            cleanup_exit(EXIT_FAILURE);
        }
    }
}

static void calculate_res(int len) {
    // A multiplication of two numbers with n digit may only be 2*n digits long.
    // One hex digit == 4 bit => allocate len bytes for the result
    unsigned char *res;
    if((res = malloc(len)) == NULL) {
        fprintf(stderr, "[%s] malloc failed: %s\n", progname, strerror(errno));
        cleanup_exit(EXIT_FAILURE);
    }
    memset(res, 0, len);

    // Numbers are written from right to left, so start with the most right digit.
    int sum = 0, res_len = 0;
    for(int i = 0; i < 2*len; i++) {
        if(i < children[3].res_len) {
            sum += extract_digit(children[3].res, children[3].res_len - i - 1);
        }
        if(i < children[2].res_len + len/2 && i >= len/2) {
            sum += extract_digit(children[2].res, children[2].res_len - i + len/2 - 1);
        }
        if(i < children[1].res_len + len/2 && i >= len/2) {
            sum += extract_digit(children[1].res, children[1].res_len - i + len/2 - 1);
        }
        if(i < children[0].res_len + len && i >= len) {
            sum += extract_digit(children[0].res, children[0].res_len - i + len - 1);
        }

        res[len - i/2 - 1] |= (sum % 16) << 4*(i%2);
        sum /= 16;
        res_len = i/2;
    }

    for(int i = len - res_len - 1; i < len; i++) {
        if(fprintf(stdout, "%02x", res[i]) != 2) {
            fprintf(stderr, "[%s] fprintf failed: %s\n", progname, strerror(ferror(stdout)));
        }
    }
    
    if(fprintf(stdout, "\n") != 1) {
        fprintf(stderr, "[%s] fprintf failed: %s\n", progname, strerror(ferror(stdout)));
    }

    free(res);
    fflush(stdout);
}

static void read_child_res(struct child_process *child, int len) {
    FILE *out = fdopen(child->pipe_out_fd[0], "r");
    if(out == NULL) {
        fprintf(stderr, "[%s] fdopen failed: %s\n", progname, strerror(errno));
        cleanup_exit(EXIT_FAILURE);
    }

    if((child->res = malloc(len)) == NULL) {
        fprintf(stderr, "[%s] malloc failed: %s\n", progname, strerror(errno));
        fclose(out);
        cleanup_exit(EXIT_FAILURE);
    }

    child->res_len = fread(child->res, 1, len, out);
    if(child->res[child->res_len-1] == '\n') {
        child->res_len--;
    }

    if(ferror(out) != 0) {
        fprintf(stderr, "[%s] fread failed: %s\n", progname, strerror(ferror(out)));
        fclose(out);
        cleanup_exit(EXIT_FAILURE);
    }

    if(fclose(out) != 0) {
        fprintf(stderr, "[%s] fclose failed: %s\n", progname, strerror(errno));
        cleanup_exit(EXIT_FAILURE);
    }
    child->pipe_out_fd[0] = -1;
}

static void write_pipe(int fd, char *data, int len, int do_close) {
    static FILE *f;
    if(fd != 0) {
        if((f = fdopen(fd, "w")) == NULL) {
            fprintf(stderr, "[%s] fdopen failed: %s\n", progname, strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }
    }

    if(fwrite(data, 1, len, f) != len) {
        fprintf(stderr, "[%s] fwrite failed: %s\n", progname, strerror(ferror(f)));
        fclose(f);
        cleanup_exit(EXIT_FAILURE);
    }
    if(fwrite("\n", 1, 1, f) != 1) {
        fprintf(stderr, "[%s] fwrite failed: %s\n", progname, strerror(ferror(f)));
        fclose(f);
        cleanup_exit(EXIT_FAILURE);
    }
    
    if(do_close == 1) {
        if(fclose(f) != 0) {
            fprintf(stderr, "[%s] fclose failed: %s\n", progname, strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }
    } else {
        if(fflush(f) != 0) {
            fprintf(stderr, "[%s] fflush failed: %s\n", progname, strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }
    }
}

static pid_t fork_setup_pipes(int in_fd[2], int out_fd[2]) {
    pid_t pid = fork();
    switch(pid) {
    case -1:
        fprintf(stderr, "[%s] fork failed: %s\n", progname, strerror(errno));
        cleanup_exit(EXIT_FAILURE);
    case 0:
        // Change STDIN to in_pipe
        if(close(in_fd[1]) != 0) {
            fprintf(stderr, "[%s] close failed: %s\n", progname, strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }
        in_fd[1] = -1;
        if(dup2(in_fd[0], STDIN_FILENO) < 0) {
            fprintf(stderr, "[%s] dup2 failed: %s\n", progname, strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }
        if(close(in_fd[0]) != 0) {
            fprintf(stderr, "[%s] close failed: %s\n", progname, strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }
        in_fd[0] = -1;

        // Change STDOUT to in_pipe
        if(close(out_fd[0]) != 0) {
            fprintf(stderr, "[%s] close failed: %s\n", progname, strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }
        out_fd[0] = -1;
        if(dup2(out_fd[1], STDOUT_FILENO) < 0) {
            fprintf(stderr, "[%s] dup2 failed: %s\n", progname, strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }
        if(close(out_fd[1]) != 0) {
            fprintf(stderr, "[%s] close failed: %s\n", progname, strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }
        out_fd[1] = -1;

        execlp(progname, progname, NULL);
        fprintf(stderr, "[%s] execlp failed\n", progname);
        cleanup_exit(EXIT_FAILURE);
    default:
        if((close(out_fd[1]) | close(in_fd[0])) != 0) {
            fprintf(stderr, "[%s] close failed: %s\n", progname, strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }
        out_fd[1] = -1;
        in_fd[0] = -1;
        break;
    }
    return pid;
}

static inline int extract_digit(unsigned char* num, int idx) {
    char buf[2];
    buf[1] = '\0';
    buf[0] = num[idx];
    errno = 0;
    int res = strtol(buf, NULL, 16);
    if (errno != 0 && res == 0) {
        fprintf(stderr, "[%s] failed to convert to number: %s\n", progname, strerror(errno));
        cleanup_exit(EXIT_FAILURE);
    }
    return res;
}

static void cleanup_exit(int status) {
    free(a);
    free(b);

    for(int i = 0; i < 4; i++) {
        free(children[i].res);
        if(children[i].pipe_in_fd[0] != -1) {
            close(children[i].pipe_in_fd[0]);
        }
        if(children[i].pipe_in_fd[1] != -1) {
            close(children[i].pipe_in_fd[1]);
        }
        
        if(children[i].pipe_out_fd[0] != -1) {
            close(children[i].pipe_out_fd[0]);
        }
        if(children[i].pipe_out_fd[1] != -1) {
            close(children[i].pipe_out_fd[1]);
        }
    }  

    exit(status);
}

static void usage(void) {
    fprintf(stderr, "Usage: %s\n", progname);
    exit(EXIT_FAILURE);
}
