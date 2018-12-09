#include <stdlib.h>
#include <stdio.h>
#include <sys/errno.h>
#include <string.h>

static char *progname;

static char *a = NULL, *b = NULL;

static void read_nums();

static void cleanup_exit(int status);

int main(int argc, char **argv) {
    progname = argv[0];

    read_nums();
    printf("a: %u b: %s", strlen(a), b);

    cleanup_exit(EXIT_SUCCESS);
}

static void read_nums() {
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

static void cleanup_exit(int status) {
    free(a);
    free(b);
    exit(status);
}