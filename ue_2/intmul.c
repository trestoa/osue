#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

 #define MAX(a,b) \
     (a > b ? a : b)

static char *progname;

static char *a = NULL, *b = NULL;

static int pipe_a_fd_in[2], pipe_a_fd_out[2];
static int pipe_b_fd_in[2], pipe_b_fd_out[2];
static int pipe_c_fd_in[2], pipe_c_fd_out[2];
static int pipe_d_fd_in[2], pipe_d_fd_out[2];

static void read_nums(void);

static void multiply(void);

static inline int extract_digit(uint8_t* num, int idx);

static void cleanup_exit(int status);

static pid_t fork_setup_pipes(int in_fd[2], int out_fd[2]);

static void write_pipe(int fd, char *data, int len, int do_close, FILE **f);

int main(int argc, char **argv) {
    progname = argv[0];
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
    int alen = strlen(a) - 1;
    int blen = strlen(b) - 1;

    if(alen != blen) {
        fprintf(stderr, "[%s] the two integers do not have equal length\n", progname);
        cleanup_exit(EXIT_FAILURE);
    }

    if(alen == 1) {
        int res = extract_digit((uint8_t*) a, 0) * extract_digit((uint8_t*) b, 0);
        fprintf(stdout, "%x", res);
    } else if(alen > 1) {
        if(alen % 2 != 0) {
            fprintf(stderr, "[%s] the number of digits is not even\n", progname);
            cleanup_exit(EXIT_FAILURE);
        }

        if((pipe(pipe_a_fd_in) | pipe(pipe_a_fd_out) | pipe(pipe_b_fd_in) | pipe(pipe_b_fd_out) |
           pipe(pipe_c_fd_in) | pipe(pipe_c_fd_out) | pipe(pipe_d_fd_in) | pipe(pipe_d_fd_out)) != 0) {
            fprintf(stderr, "[%s] the two integers do not have equal length\n", progname);
            cleanup_exit(EXIT_FAILURE);
        }

        pid_t pid_a, pid_b, pid_c, pid_d;
        FILE *f;
        pid_a = fork_setup_pipes(pipe_a_fd_in, pipe_a_fd_out);
        write_pipe(pipe_a_fd_in[1], a, alen/2, 0, &f);
        write_pipe(0, b, alen/2, 1, &f);

        pid_b = fork_setup_pipes(pipe_b_fd_in, pipe_b_fd_out);
        write_pipe(pipe_b_fd_in[1], a, alen/2, 0, &f);
        write_pipe(0, b + alen/2, alen/2, 1, &f);

        pid_c = fork_setup_pipes(pipe_c_fd_in, pipe_c_fd_out);
        write_pipe(pipe_c_fd_in[1], a + alen/2, alen/2, 0, &f);
        write_pipe(0, b, alen/2, 1, &f);

        pid_d = fork_setup_pipes(pipe_d_fd_in, pipe_d_fd_out);
        write_pipe(pipe_d_fd_in[1], a + alen/2, alen/2, 0, &f);
        write_pipe(0, b + alen/2, alen/2, 1, &f);

        // Read and combine results
        FILE *child_out_a = fdopen(pipe_a_fd_out[0], "r");
        if(child_out_a == NULL) {
            fprintf(stderr, "[%s] fdopen failed: %s\n", progname, strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }
        FILE *child_out_b = fdopen(pipe_b_fd_out[0], "r");
        if(child_out_b == NULL) {
            fprintf(stderr, "[%s] fdopen failed: %s\n", progname, strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }
        FILE *child_out_c = fdopen(pipe_c_fd_out[0], "r");
        if(child_out_c == NULL) {
            fprintf(stderr, "[%s] fdopen failed: %s\n", progname, strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }
        FILE *child_out_d = fdopen(pipe_d_fd_out[0], "r");
        if(child_out_d == NULL) {
            fprintf(stderr, "[%s] fdopen failed: %s\n", progname, strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }

        // Allocate result buffers
        unsigned char *res_a, *res_b, *res_c, *res_d, *res;
        int res_a_len, res_b_len, res_c_len, res_d_len;
        
        if((res_a = malloc(alen)) == NULL) {
            fprintf(stderr, "[%s] malloc: %s\n", progname, strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }

        if((res_b = malloc(alen)) == NULL) {
            fprintf(stderr, "[%s] malloc: %s\n", progname, strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }

        if((res_c = malloc(alen)) == NULL) {
            fprintf(stderr, "[%s] malloc: %s\n", progname, strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }

        if((res_d = malloc(alen)) == NULL) {
            fprintf(stderr, "[%s] malloc: %s\n", progname, strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }

        if((res = malloc(alen)) == NULL) {
            fprintf(stderr, "[%s] malloc: %s\n", progname, strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }
        memset(res, 0, alen);

        // Read child results
        res_a_len = fread(res_a, 1, alen, child_out_a);
        res_b_len = fread(res_b, 1, alen, child_out_b);
        res_c_len = fread(res_c, 1, alen, child_out_c);
        res_d_len = fread(res_d, 1, alen, child_out_d);

        int i;
        int sum = 0;
        for(i = 0; i < 2*alen; i++) {
            if(i < res_d_len) {
                sum += extract_digit(res_d, res_d_len - i - 1);
            }
            if(i < res_b_len + alen/2 && i >= alen/2) {
                sum += extract_digit(res_b, res_b_len - i + alen/2 - 1);
            }
            if(i < res_c_len + alen/2 && i >= alen/2) {
                sum += extract_digit(res_c, res_c_len - i + alen/2 - 1);
            }
            if(i < res_a_len + alen && i >= alen) {
                sum += extract_digit(res_a, res_a_len - i + alen - 1);
            }

            res[alen - i/2 - 1] |= (sum % 16) << 4*(i%2);
            sum /= 16;
        }

        for(int j = alen - (i - 1)/2 - 1; j < alen; j++) {
            fprintf(stdout, "%02x", res[j]);
        }
        fprintf(stdout, "\n");
        fflush(stdout);
        
        if(fclose(child_out_a) != 0) {
            fprintf(stderr, "[%s] fclose failed: %s\n", progname, strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }
        if(fclose(child_out_b) != 0) {
            fprintf(stderr, "[%s] fclose failed: %s\n", progname, strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }
        if(fclose(child_out_c) != 0) {
            fprintf(stderr, "[%s] fclose failed: %s\n", progname, strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }
        if(fclose(child_out_d) != 0) {
            fprintf(stderr, "[%s] fclose failed: %s\n", progname, strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }

        int status, child_err = 0;
        waitpid(pid_a, &status, 0);
        if(WEXITSTATUS(status) != EXIT_SUCCESS) {
            fprintf(stderr, "[%s] child exited with status %u\n", progname, WEXITSTATUS(status));
            child_err = 1;
        }

        waitpid(pid_b, &status, 0);
        if(WEXITSTATUS(status) != EXIT_SUCCESS) {
            fprintf(stderr, "[%s] child exited with status %u\n", progname, WEXITSTATUS(status));
            child_err = 1;
        }

        waitpid(pid_c, &status, 0);
        if(WEXITSTATUS(status) != EXIT_SUCCESS) {
            fprintf(stderr, "[%s] child exited with status %u\n", progname, WEXITSTATUS(status));
            child_err = 1;
        }

        waitpid(pid_d, &status, 0);
        if(WEXITSTATUS(status) != EXIT_SUCCESS) {
            fprintf(stderr, "[%s] child exited with status %u\n", progname, WEXITSTATUS(status));
            child_err = 1;
        }

        if(child_err != 0) {
            cleanup_exit(EXIT_FAILURE);
        }
    }
}

static void write_pipe(int fd, char *data, int len, int do_close, FILE **f) {
    if(fd != 0) {
        if((*f = fdopen(fd, "w")) == NULL) {
            fprintf(stderr, "[%s] fdopen failed: %s\n", progname, strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }
    }

    if(fwrite(data, 1, len, *f) != len) {
        fprintf(stderr, "[%s] fwrite failed: %s\n", progname, strerror(ferror(*f)));
        fclose(*f);
        cleanup_exit(EXIT_FAILURE);
    }
    if(fwrite("\n", 1, 1, *f) != 1) {
        fprintf(stderr, "[%s] fwrite failed: %s\n", progname, strerror(ferror(*f)));
        fclose(*f);
        cleanup_exit(EXIT_FAILURE);
    }
    
    if(do_close == 1) {
        if(fclose(*f) != 0) {
            fprintf(stderr, "[%s] fclose failed: %s\n", progname, strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }
    } else {
        if(fflush(*f) != 0) {
            fprintf(stderr, "[%s] fflush failed: %s\n", progname, strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }
    }
}

static pid_t fork_setup_pipes(int in_fd[2], int out_fd[2]) {
    pid_t pid = fork();
    switch(pid) {
    case -1:
        fprintf(stderr, "[%s] fork failed\n", progname);
        cleanup_exit(EXIT_FAILURE);
    case 0:
        if(close(in_fd[1]) != 0) {
            fprintf(stderr, "[%s] close failed: %s\n", progname, strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }
        if(dup2(in_fd[0], STDIN_FILENO) < 0) {
            fprintf(stderr, "[%s] dup2 failed: %s\n", progname, strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }
        if(close(in_fd[0]) != 0) {
            fprintf(stderr, "[%s] close failed: %s\n", progname, strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }

        if(close(out_fd[0]) != 0) {
            fprintf(stderr, "[%s] close failed: %s\n", progname, strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }
        if(dup2(out_fd[1], STDOUT_FILENO) < 0) {
            fprintf(stderr, "[%s] dup2 failed: %s\n", progname, strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }
        if(close(out_fd[1]) != 0) {
            fprintf(stderr, "[%s] close failed: %s\n", progname, strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }

        execlp(progname, progname, NULL);
        fprintf(stderr, "[%s] execlp failed\n", progname);
        cleanup_exit(EXIT_FAILURE);
    default:
        if((close(out_fd[1]) | close(in_fd[0])) != 0) {
            fprintf(stderr, "[%s] close failed: %s\n", progname, strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }
        break;
    }
    return pid;
}

static inline int extract_digit(unsigned char* num, int idx) {
    char buf[2];
    buf[1] = '\0';
    buf[0] = num[idx];
    return strtol(buf, NULL, 16);
}

static void cleanup_exit(int status) {
    free(a);
    free(b);
    exit(status);
}