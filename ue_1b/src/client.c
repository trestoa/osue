// TODO: Documentation
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "utils.h"
#include "http.h"

static void usage(void);

int main(int argc, char **argv) {
    progname = argv[0];
    char *service = service = "http", 
         *outdir = NULL, 
         *outfile = NULL,
         *url = NULL;

    int c;
    while((c = getopt(argc, argv, "p:o:d:")) != -1) {
        switch(c) {
        case 'p':
            service = optarg;
            break;
        case 'o':
            outfile = optarg;
            break;
        case 'd':
            outdir = optarg;
            break;
        case '?':
        default:
            usage();
        }
    }
    argc -= optind;
    argv += optind;

    if(argc != 1) { 
        usage();
    }

    if(outdir != NULL && outfile != NULL) {
        ERRPUTS("Either the -d or -o argument may be present, but not both them.");
        usage();
    }

    url = argv[0];

    char *hostname, *file_path;
    if(parse_url(url, &hostname, &file_path) != 0) {
        ERRPUTS("Invalid URL format.");
        exit(EXIT_FAILURE);
    }
    printf("Requesting %s, %s\n", hostname, file_path);
    
    return EXIT_SUCCESS;
}

static void usage(void) {
    fprintf(stderr, "Usage: %s [-p PORT] [ -o FILE | -d DIR ] URL\n", progname);
    exit(EXIT_FAILURE);
}
