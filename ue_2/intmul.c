#include <stdlib.h>
#include <stdio.h>
#include <sys/errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static char *progname;

static char *a = NULL, *b = NULL;

static int pipe_a_fd_in[2], pipe_a_fd_out[2];
static int pipe_b_fd_in[2], pipe_b_fd_out[2];
static int pipe_c_fd_in[2], pipe_c_fd_out[2];
static int pipe_d_fd_in[2], pipe_d_fd_out[2];

static void read_nums(void);

static void multiply(void);

static inline int extract_digit(char* num, int idx);

static void cleanup_exit(int status);

static pid_t fork_setup_pipes(int in_fd[2], int out_fd[2]);

static void write_pipe(int fd, char *data, int len, int do_close, FILE **f);

static void output_from_child(int fd);

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
        int res = extract_digit(a, 0) * extract_digit(b, 0);
        fprintf(stderr, "res: %u\n", res);
        fputc(res, stdout);
        fflush(stdout);
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
        // For process d
        output_from_child(pipe_d_fd_out[0]);

        // For process b and c
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
        
        fprintf(stderr, "d done");

        int carry = 0;
        char buf_b[16], buf_c[16];
        int act_read_b, act_read_c;
        while(feof(child_out_b) == 0 || feof(child_out_b) == 0) {
            if((act_read_b = fread(buf_b, 1, sizeof(buf_b), child_out_b)) != sizeof(buf_b)) {
                if(act_read_b == -1 && feof(child_out_b) == 0) {
                    fclose(child_out_b);
                    fclose(child_out_c);
                    fprintf(stderr, "[%s] fread failed: %s\n", progname, strerror(ferror(child_out_b)));
                    cleanup_exit(EXIT_FAILURE);
                }
            }

            if((act_read_c = fread(buf_c, 1, sizeof(buf_c), child_out_c)) != sizeof(buf_c)) {
                if(act_read_c == -1 && feof(child_out_c) == 0) {
                    fclose(child_out_b);
                    fclose(child_out_c);
                    fprintf(stderr, "[%s] fread failed: %s\n", progname, strerror(ferror(child_out_c)));
                    cleanup_exit(EXIT_FAILURE);
                }
            }

            int act_read_max = (act_read_c > act_read_b ? act_read_c : act_read_b);
            for(int i = 0; i < act_read_max; i++) {
                int b, c;
                if(i < act_read_b) {
                    b = buf_b[i];
                } else {
                    b = 0;
                }

                if(i < act_read_c) {
                    c = buf_c[i];
                } else {
                    c = 0;
                }

                if(fputc((b + c + carry) % 0xFF, stdout) < 0) {
                    fclose(child_out_b);
                    fclose(child_out_c);
                    fprintf(stderr, "[%s] fputc failed: %s\n", progname, strerror(ferror(stdout)));
                    cleanup_exit(EXIT_FAILURE);
                }
                carry = (b + c + carry - 0xFF);
                if(carry < 0) {
                    carry = 0;
                }
            }

            if(fflush(stdout) != 0) {
                fprintf(stderr, "[%s] fflush failed: %s\n", progname, strerror(errno));
                cleanup_exit(EXIT_FAILURE);
            }
            
        }
        if(fclose(child_out_b) != 0) {
            fprintf(stderr, "[%s] fclose failed: %s\n", progname, strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }
        if(fclose(child_out_c) != 0) {
            fprintf(stderr, "[%s] fclose failed: %s\n", progname, strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }

        fprintf(stderr, "b c done");

        // For process a
        FILE *child_out = fdopen(pipe_a_fd_out[0], "r");
        if(child_out == NULL) {
            fprintf(stderr, "[%s] fdopen failed: %s\n", progname, strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }
        char buf[16];
        int act_read = sizeof(buf);
        while(act_read == sizeof(buf)) {
            if((act_read = fread(buf, 1, sizeof(buf), child_out)) != sizeof(buf)) {
                if(act_read == -1 && feof(child_out) == 0) {
                    fclose(child_out);
                    fprintf(stderr, "[%s] fread failed: %s\n", progname, strerror(ferror(child_out)));
                    cleanup_exit(EXIT_FAILURE);
                }
            }

            for(int i = 0; i < act_read; i++) {
                if(fputc((buf[i] + carry) % 0xFF, stdout) < 0) {
                    fclose(child_out);
                    fprintf(stderr, "[%s] fputc failed: %s\n", progname, strerror(ferror(stdout)));
                    cleanup_exit(EXIT_FAILURE);
                }
                carry = (buf[i] + carry - 0xFF);
                if(carry < 0) {
                    carry = 0;
                }
            }
        }
        if(fclose(child_out) != 0) {
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

static void output_from_child(int fd) {
    FILE *child_out = fdopen(fd, "r");
    if(child_out == NULL) {
        fprintf(stderr, "[%s] fdopen failed: %s\n", progname, strerror(errno));
        cleanup_exit(EXIT_FAILURE);
    }

    char buf[16];
    int act_read = sizeof(buf);
    while(act_read == sizeof(buf)) {
        if((act_read = fread(buf, 1, sizeof(buf), child_out)) != sizeof(buf)) {
            if(act_read == -1 && feof(child_out) == 0) {
                fclose(child_out);
                fprintf(stderr, "[%s] fread failed: %s\n", progname, strerror(ferror(child_out)));
                cleanup_exit(EXIT_FAILURE);
            }
        }

        if(fwrite(buf, 1, act_read, stdout) != act_read) {
            fclose(child_out);
            fprintf(stderr, "[%s] fwrite failed: %s\n", progname, strerror(ferror(child_out)));
            cleanup_exit(EXIT_FAILURE);
        }
        if(fflush(stdout) != 0) {
            fprintf(stderr, "[%s] fflush failed: %s\n", progname, strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }
    }
    if(fclose(child_out) != 0) {
        fprintf(stderr, "[%s] fclose failed: %s\n", progname, strerror(errno));
        cleanup_exit(EXIT_FAILURE);
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

        execlp(progname, NULL);
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

static inline int extract_digit(char* num, int idx) {
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