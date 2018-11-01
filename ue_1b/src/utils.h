#ifndef UTILS_H
#define UTILS_H

#define ERRPRINTF(format, ...) \
    fprintf(stderr, "[%s] " format, progname, __VA_ARGS__)

#define ERRPUTS(msg) \
    fprintf(stderr, "[%s] " msg, progname)

char *progname;

#endif
