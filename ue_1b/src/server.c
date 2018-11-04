#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>
#include <string.h>

#include "http.h"
#include "utils.h"

#define LISTEN_BACKLOG 50

static char *progname;

static volatile sig_atomic_t quit = 0;

static struct addrinfo *ai = NULL;
static int sockfd = -1;
static FILE *conn = NULL;

static void usage(void);

static void open_socket(char *port);

static void cleanup_exit(int status);

static void handle_signal(int signal);

static void run_server(void);

int main(int argc, char **argv) {
    progname = argv[0];

    char *index_file = "index.html";
    char *port = "8080";
    char *docroot;
    
    int c;
    while((c = getopt(argc, argv, "p:i:")) != -1) {
        switch(c) {
        case 'p':
            port = optarg;
            break;
        case 'i':
            index_file = optarg;
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

    docroot = argv[0];

    // Setup shutdown handler
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    open_socket(port);
    printf("Server listening on port %s...\n", port);

    run_server();

    cleanup_exit(EXIT_SUCCESS);
}

static void usage(void) {
    fprintf(stderr, "Usage: %s [-p PORT] [-i INDEX] DOC_ROOT\n", progname);
    exit(EXIT_FAILURE);
}

static void handle_signal(int signal) {
    quit = 1;
}

static void open_socket(char *port) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int res = getaddrinfo(NULL, port, &hints, &ai); 
    if(res != 0) {
        ERRPRINTF("getaddrinfo failed: %s\n", gai_strerror(res));
        cleanup_exit(EXIT_FAILURE);
    }

    sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if(sockfd < 0) {
        ERRPRINTF("socket failed: %s\n", strerror(errno));
        cleanup_exit(EXIT_FAILURE);
    }

    if(bind(sockfd, ai->ai_addr, ai->ai_addrlen) < 0) {
        ERRPRINTF("bind failed: %s\n", strerror(errno));
        cleanup_exit(EXIT_FAILURE);
    }

    if(listen(sockfd, LISTEN_BACKLOG) < 0) {
        ERRPRINTF("listen failed: %s\n", strerror(errno));
        cleanup_exit(EXIT_FAILURE);
    }
}

static void run_server(void) {
    int connfd;
    while(!quit) {
        connfd = accept(sockfd, NULL, NULL);
        if(connfd < 0) {
            if(errno != EINTR) {
                continue;    
            }
            ERRPRINTF("accept failed: %s\n", strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }

        if((conn = fdopen(connfd, "w+")) == NULL) {
            ERRPRINTF("fdopen failed: %s\n", strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }

        http_frame_t *req;
        int ret = http_recv_req(conn, &req);
        if(ret != NULL) {
            switch(ret) {
            case HTTP_ERR_INTERNAL:
                ERRPRINTF("error while receiving request: %s\n", strerror(errno));
                cleanup_exit(EXIT_FAILURE);
            case HTTP_ERR_STREAM:
                if (feof(conn) != 0) {
                    ERRPUTS("error while receiving request: EOF reached");
                    break;
                }
                ERRPRINTF("error while receiving request: %s\n", strerror(ferror(conn)));
                cleanup_exit(EXIT_FAILURE);
            case HTTP_ERR_PROTOCOL:
                ERRPUTS("malformed request received");
                // TODO: Send error response
                break;
            default:
                ERRPRINTF("error while receiving request: unknown error: %u\n", ret);
                cleanup_exit(EXIT_FAILURE);
            }
        } else {
            printf("Request: %s %s\n", req->method, req->file_path);
        }

        
        fclose(conn);
        conn = NULL;
    }
    printf("Signal caught, exiting.\n");
}

static void handle_http_err(int err, char *cause) {
    
}

static void cleanup_exit(int status) {
    if(ai != NULL) {
        freeaddrinfo(ai);
    }

    if(sockfd >= 0 ) {
        close(sockfd);
    }

    if(conn != NULL) {
        fclose(conn);
    }
    exit(status);
}
