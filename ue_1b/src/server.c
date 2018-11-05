#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <libgen.h>

#include "http.h"
#include "utils.h"

#define LISTEN_BACKLOG 50

static char *progname;

static char *docroot, *index_file = "index.html";

static volatile sig_atomic_t quit = 0;

static struct addrinfo *ai = NULL;
static int sockfd = -1;

static void usage(void);

static void open_socket(char *port);

static void cleanup_exit(int status);

static void handle_signal(int signal);

static void run_server(void);

static void handle_request(FILE *conn);

int main(int argc, char **argv) {
    char *port = "8080";
    progname = argv[0];

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
    FILE *conn;
    while(!quit) {
        connfd = accept(sockfd, NULL, NULL);
        if(connfd < 0) {
            if(errno == EINTR) {
                continue;    
            }
            ERRPRINTF("accept failed: %s\n", strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }
        if((conn = fdopen(connfd, "w+")) == NULL) {
            ERRPRINTF("fdopen connfd failed: %s\n", strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }

        handle_request(conn);

        if(fclose(conn) != 0) {
            ERRPRINTF("fclose conn failed: %s\n", strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }
    }
    printf("Signal caught, exiting.\n");
}

static void handle_request(FILE *conn) {
    // TODO: error handling: especially for http_send_res calls
    http_frame_t res, *req;
    memset(&res, 0, sizeof(res));

    http_header_t conn_header = {"Connection", "close", NULL};
    
    time_t t = time(NULL);
    struct tm *tm = gmtime(&t);
    // We assume that 100 characters will be sufficient for the date
    char timestr[100];
    if(tm == NULL) {
        ERRPUTS("gmtime failed");
        fclose(conn);
        cleanup_exit(EXIT_FAILURE);
    }
    // TODO: set locale?
    if(strftime(timestr, sizeof(timestr), "%a, %d %b %Y %T %Z", tm) == 0) {
        ERRPUTS("gmtime failed");
        fclose(conn);
        cleanup_exit(EXIT_FAILURE);
    }
    http_header_t date_header = {"Date", timestr, &conn_header};

    res.header_first = &date_header;

    int ret = http_recv_req(conn, &req);
    if(ret != HTTP_SUCCESS) {
        switch(ret) {
        case HTTP_ERR_INTERNAL:
            ERRPRINTF("error while receiving request: %s\n", strerror(errno));
            fclose(conn);
            cleanup_exit(EXIT_FAILURE);
        case HTTP_ERR_STREAM:
            if (feof(conn) != 0) {
                ERRPUTS("error while receiving request: EOF reached");
                return;
            }
            ERRPRINTF("error while receiving request: %s\n", strerror(ferror(conn)));
            fclose(conn);
            cleanup_exit(EXIT_FAILURE);
        case HTTP_ERR_PROTOCOL:
            ERRPUTS("malformed request received");
            res.status = 400;
            res.status_text = "Bad Request";
            http_send_res(conn, &res, NULL);
            return;
        default:
            ERRPRINTF("error while receiving request: unknown error: %u\n", ret);
            fclose(conn);
            cleanup_exit(EXIT_FAILURE);
        }
    } else {
        // TODO: case insensitive?
        if(strcmp(req->method, "GET") != 0) {
            res.status = 501;
            res.status_text = "Not Implemented";
            http_send_res(conn, &res, NULL);
        } else {
            // Add one character for null byte
            int docroot_trailing_slash = docroot[strlen(docroot)-1] == '/';
            
            int path_len = strlen(req->file_path) + strlen(docroot) + 1;
            int add_index = req->file_path[strlen(req->file_path)-1] == '/';
            if(add_index == 1) {
                path_len += strlen(index_file);
            }
            if(docroot_trailing_slash == 1) {
                path_len++;
            }

            char *file_path = malloc(path_len);
            if(file_path == NULL) {
                ERRPRINTF("malloc failed: %s\n", strerror(errno));
                fclose(conn);
                cleanup_exit(EXIT_FAILURE);
            }

            if(docroot_trailing_slash == 1) {
                snprintf(file_path, path_len, "%s/%s", docroot, req->file_path);
            } else {
                snprintf(file_path, path_len, "%s%s", docroot, req->file_path);
            }
            if(add_index == 1) {
                strcat(file_path, index_file);
            }
            
            FILE *file;
            if((file = fopen(file_path, "r")) == NULL) {
                if(errno == ENOENT) {
                    res.status = 404;
                    res.status_text = "Not Found";
                    http_send_res(conn, &res, NULL);
                    return;
                }
            }

            if(fseek(file, 0L, SEEK_END) != 0) {
                ERRPRINTF("fseek on %s failed: %s\n", file_path, strerror(errno));
                fclose(conn);
                cleanup_exit(EXIT_FAILURE);
            }
            int file_len = ftell(file);
            // With a 64 bit integer, 21 characters are needed at most
            char file_len_str[21];
            snprintf(file_len_str, sizeof(file_len_str), "%u", file_len);
            if(file_len < 0) {
                ERRPRINTF("ftell on %s failed: %s\n", file_path, strerror(errno));
                fclose(conn);
                cleanup_exit(EXIT_FAILURE);
            }
            if(fseek(file, 0, SEEK_SET) != 0) {
                ERRPRINTF("rewind on %s failed: %s\n", file_path, strerror(errno));
                fclose(conn);
                cleanup_exit(EXIT_FAILURE);
            }

            http_header_t c_length_header = {"Content-Length", file_len_str, &date_header};
            res.status = 200;
            res.status_text = "OK";
            res.header_first = &c_length_header;
            ret = http_send_res(conn, &res, file);
            if(fclose(file) != 0) {
                ERRPRINTF("fclose on %s failed: %s\n", file_path, strerror(errno));
                fclose(conn);
                cleanup_exit(EXIT_FAILURE);
            }
        }

        printf("Request: %s %s\n", req->method, req->file_path);
    }
}

static void cleanup_exit(int status) {
    if(ai != NULL) {
        freeaddrinfo(ai);
    }

    if(sockfd >= 0 ) {
        close(sockfd);
    }
    exit(status);
}
