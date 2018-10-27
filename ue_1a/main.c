/**
 * @file main.c
 * @author Markus Klein (e11707252@student.tuwien.ac.at)
 * @brief Main module for exercise a1
 * @version 1.0
 * @date 2018-10-27
 * 
 * @details This program implements a variant of the unix command line tool "diff" 
 * called "mydiff" as specified in the exercise description.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <string.h>
#include <ctype.h>

static char *progname; /**< Program name. Name of the executable used for usage and error messages. */

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
 * Wrapper of fopen with error handling.
 * @brief fopen with error handling.
 * 
 * @param path Path to the file that should be opened.
 * @param mode Mode option passed to fopen.
 * @return FILE* FILE object returned by fopen.
 * 
 * @details Tries to open and return the given file. Prints an error message to stderr and
 * terminates the program with EXIT_FAILURE if an error occures.
 * Global variables: progname.
 */
static FILE* fopen_checked(char *path, char *mode);

/**
 * fclose with error handling.
 * @brief Wrapper of fclose with error handling.
 * 
 * @param f FILE object that should be close.
 * 
 * @details Tries to close the given file. Prints an error message to stderr and
 * terminates the program with EXIT_FAILURE if an error occures.
 * Global variables: progname.
 */
static void fclose_checked(FILE *f);

/**
 * Implementation of the diff algorithm for mydiff.
 * @brief Compares two files line by line and prints the number of 
 * different charakters to an output stream.
 * 
 * @param file1 FILE object of first file used for the comparision.
 * @param file2 FILE object of second file used for the comparision.
 * @param out FILE object where the differences will be written to.
 * @param ignore_case When 1, the comparison is case insensitive.
 * 
 * @details Reads the files file1 and file2 line by line and compares each line character
 * by character. The number of different characters per line (if > 0) are are 
 * written to the given out stream. File comparision stops when one of the 
 * files streams reaches EOF; line comparison stop when one of the lines reaches 
 * the line end. 
 * Global variables: progname.
 */
void diff(FILE *file1, FILE *file2, FILE *out, int ignore_case);

/**
 * Main method for the mydiff program.
 * @brief Program entry point. Parses the command line arguments, opens 
 * the file streams and calls the diff function.
 * 
 * @param argc Argument counter.
 * @param argv Argument vector.
 * @return int Program exit code (EXIT_SUCCESS)
 * 
 * @details Reads the command line arguments via getopt and checks for the correct 
 * number of arguments. Subsequently opens the two input file and the output file 
 * (defaults to stdout) and calls the main diff algorithm implemented of the diff function.
 * Global variables: progname.
 */
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
        outfile = fopen_checked(outfile_path, "w");
    } else {
        outfile = stdout;
    }
    file1 = fopen_checked(argv[0], "r");
    file2 = fopen_checked(argv[1], "r");
    
    diff(file1, file2, outfile, ignore_case);
    
    fclose_checked(file1);
    fclose_checked(file2);
    if(outfile_path != NULL) {
        fclose_checked(outfile);
    }
    return EXIT_SUCCESS;
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

static FILE* fopen_checked(char *path, char *mode) {
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
