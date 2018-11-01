// TODO: Documentation
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "utils.h"
#include "http.h"

// These are all pointers to non allocated memory space -> no free needed.
static char *service = "http", 
     *outdir = NULL, 
     *outfile = NULL,
     *url = NULL;

// These are all pointers to allocated memory space -> free needed.
static char *hostname = NULL,
            *file_path = NULL;

static struct addrinfo *ai = NULL;
static FILE *sock = NULL;

static void usage(void);

static void connect_to_server(void);

static void cleanup_exit(int status);

int main(int argc, char **argv) {
    progname = argv[0];

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
    if(parse_url(url, &hostname, &file_path) != 0) {
        ERRPUTS("Invalid URL format.");
        exit(EXIT_FAILURE);
    }
    
    connect_to_server();
    
    cleanup_exit(EXIT_SUCCESS);
}

static void usage(void) {
    fprintf(stderr, "Usage: %s [-p PORT] [ -o FILE | -d DIR ] URL\n", progname);
    exit(EXIT_FAILURE);
}

static void connect_to_server(void) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    int res = getaddrinfo(hostname, service, &hints, &ai);
    if(res != 0) {
        ERRPRINTF("getaddrinfo failed: %s\n", gai_strerror(res));
        cleanup_exit(EXIT_FAILURE);
    }

    int sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if(sockfd < 0) {
        ERRPRINTF("socket failed: %s\n", strerror(errno));
        cleanup_exit(EXIT_FAILURE);
    }

    if(connect(sockfd, ai->ai_addr, ai->ai_addrlen) < 0) {
        ERRPRINTF("connect failed: %s\n", strerror(errno));
        cleanup_exit(EXIT_FAILURE);
    }

    if((sock = fdopen(sockfd, "w+")) == NULL) {
        ERRPRINTF("fdopen failed: %s\n", strerror(errno));
        cleanup_exit(EXIT_FAILURE);
    }
}

static void cleanup_exit(int status) {
    free(hostname);
    free(file_path);
    freeaddrinfo(ai);

    if(sock != NULL) {
        fclose(sock);
    }
}