#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static struct child_process {
    int pid;
    int pipe_in_fd[2];
    int pipe_out_fd[2];
    unsigned char *res;
    int res_len;
} children[4];

static char *progname;

static char *a = NULL, *b = NULL;

static void usage(void); 

static void read_nums(void);

static void multiply(void);

static void calculate_res(int len);

static void read_child_res(struct child_process *child, int len);

static inline int extract_digit(uint8_t* num, int idx);

static void cleanup_exit(int status);

static pid_t fork_setup_pipes(int in_fd[2], int out_fd[2]);

static void write_pipe(int fd, char *data, int len, int do_close);

int main(int argc, char **argv) {
    progname = argv[0];

    if(argc > 1) {
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
    unsigned char *res;
    if((res = malloc(len)) == NULL) {
        fprintf(stderr, "[%s] malloc failed: %s\n", progname, strerror(errno));
        cleanup_exit(EXIT_FAILURE);
    }
    memset(res, 0, len);

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
    return strtol(buf, NULL, 16);
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
