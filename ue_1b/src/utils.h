/**
 * @file utils.h
 * @author Markus Klein (e11707252@student.tuwien.ac.at)
 * @brief This module contains utility macros for error printing used both 
 * by the client and the server. 
 * @version 1.0
 * @date 2018-11-10
 * @details Contains the ERRPRINTF and ERRPUTS which are a convenient wrapper
 * of fprintf for printing error messages which automatically contain the
 * program name as required by the coding rules and guidelines.
 */

#ifndef UTILS_H
#define UTILS_H

/**
 * @brief fprintf with program name.
 * @details Applies the given arguments to printf, but prepends the messages with
 * the program name. Assumes that the progname variable is present in the context 
 * where this macro is used.
 */
#define ERRPRINTF(format, ...) \
    fprintf(stderr, "[%s] " format, progname, __VA_ARGS__)

/**
 * @brief puts to stderr with program name.
 * @details Applies the given argument to printf, but prepends the messages with
 * the program name. Assumes that the progname variable is present in the context 
 * where this macro is used.
 */
#define ERRPUTS(msg) \
    fprintf(stderr, "[%s] " msg, progname)

#endif
