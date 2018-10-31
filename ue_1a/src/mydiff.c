/**
 * @file mydiff.c
 * @author Markus Klein (e11707252@student.tuwien.ac.at) 
 * @brief Implementation of the mydiff module.
 * @version 1.0
 * @date 2018-10-31
 * 
 * @details The implementation of the diff algorithm is split into the function diff 
 * and diff_line. The function diff handles the file IO and iterates over the two input
 * files line per line, diff_line iterates character by character and calculates the
 * number of different symbols per line.
 */

#include <unistd.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <string.h>
#include <ctype.h>

#include "mydiff.h"

extern char *progname;

extern void cleanup_exit(int status);

/**
 * @brief Counts the number different characters of the two given strings.
 * 
 * @param line1 First string used for the comparision.
 * @param line2 Second string used for the comparision.
 * @param linelen1 Length of the first string (including newline character).
 * @param linelen2 Length of the second string (including newline character).
 * @param ignore_case When 1, the comparison is case insensitive.
 * @return int Number of different characters.
 *
 * @details Compares the given strings character by character and counts the number 
 * of different characters. Stops with the storter line, if linelen1 != linelen2.
 */
static unsigned int diff_line(char *line1, char *line2, ssize_t linelen1, ssize_t linelen2, int ignore_case);

void diff(FILE *file1, FILE *file2, FILE *out, int ignore_case) {
    unsigned int diffcount, linecount = 1;
    char *line1 = NULL, *line2 = NULL;
    size_t linecap1 = 0, linecap2 = 0;
    ssize_t linelen1, linelen2;

    while(1) {
        if((linelen1 = getline(&line1, &linecap1, file1)) <= 0) {
            if(feof(file1) != 0) {
                break;
            }
            fprintf(stderr, "[%s] getline failed: %s\n", progname, strerror(ferror(file1)));
            free(line1);
            free(line2);
            cleanup_exit(EXIT_FAILURE);
        }

        if((linelen2 = getline(&line2, &linecap2, file2)) <= 0) {
            if(feof(file2) != 0) {
                break;
            }
            fprintf(stderr, "[%s] getline failed: %s\n", progname, strerror(ferror(file2)));
            free(line1);
            free(line2);
            cleanup_exit(EXIT_FAILURE);
        }

        diffcount = diff_line(line1, line2, linelen1, linelen2, ignore_case);

        if(diffcount > 0) {
            if(fprintf(out, "Line: %u, Characters: %u\n", linecount, diffcount) < 0) {
                fprintf(stderr, "[%s] fprintf failed: %s\n", progname, strerror(ferror(out)));
                free(line1);
                free(line2);
                cleanup_exit(EXIT_FAILURE);
            }
        }
        linecount++;
        diffcount = 0;
    }
    free(line1);
    free(line2);
}

unsigned int diff_line(char *line1, char *line2, ssize_t linelen1, ssize_t linelen2, int ignore_case) {
    // Check for linelen1-1 and linelen2-1 here as the returned char* contains the delimiter character
    unsigned int diffcount = 0;
    for(ssize_t linepos = 0; linepos < linelen1-1 && linepos < linelen2-1; linepos++) {
        char c1 = line1[linepos], c2 = line2[linepos];
        if(ignore_case == 1) {
            c1 = tolower(c1);
            c2 = tolower(c2);
        }

        if(c1 != c2) {
            diffcount++;
        }
    }
    return diffcount;
}
