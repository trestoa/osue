/**
 * @file mydiff.h
 * @author Markus Klein (e11707252@student.tuwien.ac.at) 
 * @brief This modules contains the implemention of the diff algorithm.
 * @version 1.0
 * @date 2018-10-31
 * 
 * @details This module contains the diff methods, which implements the diff
 * algorithm specified in the assignment for exercise 1a.
 */

#ifndef MYDIFF_H
#define MYDIFF_H

#include <stdio.h>

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

#endif
