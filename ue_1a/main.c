#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <string.h>
#include <ctype.h>

// TODO doxygen
static char *progname;

static void usage(void);
static FILE* openf_checked(char *path, char *mode);
static void fclose_checked(FILE *f);

void diff(FILE *file1, FILE *file2, FILE *out, int ignore_case);

int main(int argc, char **argv) {
    progname = argv[0];
    int ignore_case = 0;
    char* outfile_path = NULL;

    int c;
    while((c = getopt(argc, argv, "io:")) != -1) {
        switch(c) {
        case 'i': 
            ignore_case = 1;
            break;
        case 'o':
            outfile_path = optarg;
            break;
        case '?':
        default:
            usage();
        }
    }
    argc -= optind;
    argv += optind;

    if(argc != 2) {
        usage();
    }

    FILE *outfile, *file1, *file2;
    if(outfile_path != NULL) {
        outfile = openf_checked(outfile_path, "w");
    } else {
        outfile = stdout;
    }
    file1 = openf_checked(argv[0], "r");
    file2 = openf_checked(argv[1], "r");
    
    diff(file1, file2, outfile, ignore_case);
    
    fclose_checked(file1);
    fclose_checked(file2);
    if(outfile_path != NULL) {
        fclose_checked(outfile);
    }
    exit(EXIT_SUCCESS);
}

void diff(FILE *file1, FILE *file2, FILE *out, int ignore_case) {
    unsigned int linecount = 1, diffcount = 0;
    char *line1 = NULL, *line2 = NULL;
    size_t linecap1 = 0, linecap2 = 0;
    ssize_t linepos, linelen1, linelen2;

    while(1) {
        if((linelen1 = getline(&line1, &linecap1, file1)) <= 0) {
            if(feof(file1) != 0) {
                break;
            }
            fprintf(stderr, "[%s] getline failed: %s\n", progname, strerror(ferror(file1)));
            exit(EXIT_FAILURE);
        }

        if((linelen2 = getline(&line2, &linecap2, file2)) <= 0) {
            if(feof(file2) != 0) {
                break;
            }
            fprintf(stderr, "[%s] getline failed: %s\n", progname, strerror(ferror(file2)));
            exit(EXIT_FAILURE);
        }

        // Check for linelen1-1 and linelen2-1 here as the returned char* contations the delimiter character
        for(linepos = 0; linepos < linelen1-1 && linepos < linelen2-1; linepos++) {
            char c1 = line1[linepos], c2 = line2[linepos];
            if(ignore_case == 1) {
                c1 = tolower(c1);
                c2 = tolower(c2);
            }

            if(c1 != c2) {
                diffcount++;
            }
        }

        if(diffcount > 0) {
            if(fprintf(out, "Line: %u, Characters: %u\n", linecount, diffcount) < 0) {
                fprintf(stderr, "[%s] fprintf failed: %s\n", progname, strerror(ferror(out)));
                exit(EXIT_FAILURE);
            }
        }
        linecount++;
        diffcount = 0;
    }
    free(line1);
    free(line2);
}

static void usage(void) {
    fprintf(stderr, "Usage: %s [-i] [-o outfile] file1 file2\n", progname);
    exit(EXIT_FAILURE);
}

static FILE* openf_checked(char *path, char *mode) {
    FILE *f = fopen(path, mode);
    if(f == NULL) {
        fprintf(stderr, "[%s] fopen on %s failed: %s\n", progname, path, strerror(errno));
        exit(EXIT_FAILURE);
    }
    return f;
}

static void fclose_checked(FILE *f) {
    if(fclose(f) != 0) {
        fprintf(stderr, "[%s] fclose failed: %s\n", progname, strerror(errno));
        exit(EXIT_FAILURE);
    }
}
