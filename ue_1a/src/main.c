/**
 * @file main.c
 * @author Markus Klein (e11707252@student.tuwien.ac.at)
 * @brief Main module for exercise a1
 * @version 1.0
 * @date 2018-10-27
 * 
 * @details This is the main module for the command line tool "mydiff". It contains
 * the code for setup, resource management and teardown of the application.
 * For the actual implementation of the diff algorithm, {@see mydiff.h}
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <string.h>
#include <ctype.h>

#include "mydiff.h"

/**
 * @brief Program name.
 * @details Name of the executable used for usage and error messages.
 */
char *progname;

/**
 * @brief Output file.
 * @details File where file differences are written to. Defined here as module wide
 * variable so that cleanup_exit can access it during program shutdown.
 */
static FILE *outfile = NULL;

/**
 * @brief First input file.
 * @details This is the first file passed to the diff algorithm. Defined here as 
 * module wide variable so that cleanup_exit can access it during program shutdown.
 */
static FILE *file1 = NULL; 

/**
 * @brief Second input file.
 * @details This is the second file passed to the diff algorithm. Defined here as 
 * module wide variable so that cleanup_exit can access it during program shutdown.
 */
static FILE *file2 = NULL; 

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
 * @details Tries to close the given file. Prints an error message to stderr if an 
 * error occures. However, this method does not trigger a program termination on 
 * failure as this function is only used during program cleanup when the program 
 * shutdown is alrady in prograss.
 * Global variables: progname.
 */
static void fclose_checked(FILE *f);

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
 * Global variables: progname, outfile, file1, file2.
 */
int main(int argc, char **argv) {
    progname = argv[0];
    int ignore_case = 0;
    char* outfile_path = NULL;

    // Parse cli arguments
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

    // Open input/output files and call main algorithm
    if(outfile_path != NULL) {
        outfile = fopen_checked(outfile_path, "w");
    } else {
        outfile = stdout;
    }
    file1 = fopen_checked(argv[0], "r");
    file2 = fopen_checked(argv[1], "r");
    
    diff(file1, file2, outfile, ignore_case);
    
    cleanup_exit(EXIT_SUCCESS);
}

void cleanup_exit(int status) {
    if(file1 != NULL) {
        fclose_checked(file1);
    }

    if(file2 != NULL) {
        fclose_checked(file2);
    }

    if(outfile != NULL) {
        fclose_checked(outfile);
    }

    exit(status);
}

static void usage(void) {
    fprintf(stderr, "Usage: %s [-i] [-o outfile] file1 file2\n", progname);
    exit(EXIT_FAILURE);
}

static FILE* fopen_checked(char *path, char *mode) {
    FILE *f = fopen(path, mode);
    if(f == NULL) {
        fprintf(stderr, "[%s] fopen on %s failed: %s\n", progname, path, strerror(errno));
        cleanup_exit(EXIT_FAILURE);
    }
    return f;
}

static void fclose_checked(FILE *f) {
    if(fclose(f) != 0) {
        fprintf(stderr, "[%s] fclose failed: %s\n", progname, strerror(errno));
    }
}
