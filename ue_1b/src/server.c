/**
 * @file server.c
 * @author Markus Klein (e11707252@student.tuwien.ac.at)
 * @brief Implementation of the http server for exercise 1b.
 * @version 1.0
 * @date 2018-11-07
 * @details This module contains the implementation of a simple http that is able to
 * server static file from a directory using http GET requests. The code in this module 
 * consists mostly of setup code resource management while the specifics on the http
 * protocol are provided by the http module. 
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <libgen.h>
#include <strings.h>

#include "http.h"
#include "utils.h"

/**
 * @brief Client backlog.
 * @details Client backlog paramter passed to listen indicating the number of 
 * clients that will be keept in the queue for being accepted.
 */
#define LISTEN_BACKLOG 50

/**
 * @brief Macro for replying an error message.
 * @details Replies an http response with the given status number and status text.
 * Assumes that the http frame called "res", the client connection stream "conn" and the 
 * and the request frame "req" exists in the context where this macro is used. 
 */
#define SEND_ERR_RES(s, st) \
    res.status = s; \
    res.status_text = st; \
    send_res(&res, conn); \
    http_free_frame(req);

/**
 * @brief Program name.
 * @details Name of the executable used for usage and error messages.
 */
static char *progname;

/**
 * @brief Path to document root.
 * @details Path to the document root of the webserver, this path will be prepended
 * to request file path's.
 */
static char *docroot;

/**
 * @brief Index file name.
 * @details If the client requests a directory (request path ends with "/"), the
 * index file name will be appended to the request path.
 */
static char *index_file = "index.html";

/**
 * @brief Flag denoting whether the program should be terminated.
 * @details This variable is used by the signal handlers to indicated that a signal
 * was caught and the program should be terminated after the current request
 * has been handled. 
 */
static volatile sig_atomic_t quit = 0;

/**
 * @brief File descriptor for the server socket.
 * @details -1 if not open
 */
static int sockfd = -1;

/**
 * Print usage. 
 * @brief Prints synopsis of the http server program.
 * 
 * @details Prints the usage message of the server on sterr and 
 * terminates the program with EXIT_FAILURE.
 * Global variables: progname.
 */
static void usage(void);

/**
 * @brief Sets up the server socket.
 * 
 * @param port Port on which the server should be started.
 * 
 * @details Opens a passive socket, binds it to the given port and start listening
 * on the socket. The file descriptor of the socket is stored to sockfd. 
 */
static void open_socket(char *port);

/**
 * Cleanup and terminate.
 * @brief Free allocated memory, close open streams and terminate program with the
 * given status code.
 * 
 * @param status Returns status of the program.
 * 
 * @details Closes open file streams (pointers != NULL) and exits the program using 
 * exit(), returning the given status.
 */
static void cleanup_exit(int status);

/**
 * @brief Signal handler for SIGINT and SIGTERM.
 * 
 * @param signal Caught signal.
 * 
 * @details Signal handler. Initiates the termination of the program by setting
 * the quit flag. The server will finish handling the current request (if there is any)
 * and terminate afterwards.
 */
static void handle_signal(int signal);

/**
 * @brief Contains the main loop for the server.
 * @details Continuously accept client and handle their requests (via the 
 * handle_request function). 
 */
static void run_server(void);

/**
 * @brief Handle a single client request.
 * 
 * @param conn Client connection stream.
 * 
 * @details Receives a request from a client and tries to reply the requested file.
 * In addition to the error behavior defined in the exercise description (404 if file 
 * not found, 501 if method not supported) this function implements the following error
 * handling procedures:
 * - Close the socket without sending a reply if a stream error on the client connection occurs.
 * - Terminate the server if a memory allocation error (or a different unexpected error) occurs.
 * - Reply with an 500 internal server error otherwise (e.g. request file failed to open).
 */
static void handle_request(FILE *conn);

/**
 * @brief Get the file path for a given requested file
 * 
 * @param req_path Request path from the http request (must start with a slash)
 * @return char* File path to the requested file.
 * 
 * @details Contains the document root with the requests file path and appends the 
 * index file name if the requested file ends with a slash. The returns values is a
 * pointer to a dynamically allocated memory space and therefore needs to be freed.
 */
static char *get_file_path(char *req_path);

/**
 * @brief Helper function for sending a reponse to the client.
 * 
 * @param res Response http frame which should be sent.
 * @param conn Client connection stream.
 * 
 * @details Wraps http_send_res and implementes error handling for the function.
 */
static void send_res(http_frame_t *res, FILE *conn);

/**
 * @brief Main method for the http server. Parses the command line arguments,
 * intializes signal handling and calls the main server function.
 * 
 * @param argc Argument counter.
 * @param argv Argument vector.
 * @return int Program exit code (EXIT_SUCCESS)
 * 
 * @details Reads the command line arguments via getopt and checks for the correct 
 * number of arguments. If the argument count and provided options are correct, signal
 * for SIGINT and SIGTERM are set up and the run_server function with the main loop 
 * is called.
 */
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
    struct addrinfo hints, *ai;
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
        freeaddrinfo(ai);
        ERRPRINTF("socket failed: %s\n", strerror(errno));
        cleanup_exit(EXIT_FAILURE);
    }

    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);

    if(bind(sockfd, ai->ai_addr, ai->ai_addrlen) < 0) {
        freeaddrinfo(ai);
        ERRPRINTF("bind failed: %s\n", strerror(errno));
        cleanup_exit(EXIT_FAILURE);
    }
    freeaddrinfo(ai);

    if(listen(sockfd, LISTEN_BACKLOG) < 0) {
        ERRPRINTF("listen failed: %s\n", strerror(errno));
        cleanup_exit(EXIT_FAILURE);
    }
}

static void run_server(void) {
    FILE *conn;
    int connfd;
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
            if(close(connfd) != 0) {
                ERRPRINTF("close connfd failed: %s\n", strerror(errno));
            }
            continue;
        }

        handle_request(conn);

        if(fclose(conn) != 0) {
            ERRPRINTF("fclose conn failed: %s\n", strerror(errno));
        }
        conn = NULL;
    }
    printf("Signal caught, exiting.\n");
}

static void handle_request(FILE *conn) {
    http_frame_t res, *req;
    memset(&res, 0, sizeof(res));

    http_header_t conn_header = {"Connection", "close", NULL};
    res.header_first = &conn_header;

    int ret = http_recv_req(conn, &req);
    if(ret != HTTP_SUCCESS) {
        switch(ret) {
        case HTTP_ERR_INTERNAL:
            ERRPRINTF("error while receiving request: %s\n", strerror(errno));
            fclose(conn);
            http_free_frame(req);
            cleanup_exit(EXIT_FAILURE);
        case HTTP_ERR_STREAM:
            ERRPRINTF("error while receiving request: %s\n", strerror(ferror(http_errvar)));
            http_free_frame(req);
            return;
        case HTTP_ERR_PROTOCOL:
            ERRPUTS("malformed request received");
            SEND_ERR_RES(400, "Bad Request");
            return;
        default:
            ERRPRINTF("error while receiving request: unknown error: %u\n", ret);
            fclose(conn);
            http_free_frame(req);
            cleanup_exit(EXIT_FAILURE);
        }
    }

    printf("> %s %s\n", req->method, req->file_path);

    if(strcasecmp(req->method, "GET") != 0) {
        SEND_ERR_RES(501, "Not Implemented");
        return;
    }

    FILE *body = NULL;
    char *file_path = get_file_path(req->file_path);
    // Add one character for null byte
    if((body = fopen(file_path, "r")) == NULL) {
        if(errno == ENOENT) {
            free(file_path);

            SEND_ERR_RES(404, "Not Found");
            return;
        }

        ERRPRINTF("fopen on %s failed: %s\n", file_path, strerror(errno));
        free(file_path);
        SEND_ERR_RES(500, "Internal Server Error");
        return;
    } 

    if(fseek(body, 0L, SEEK_END) != 0) {
        ERRPRINTF("fseek on %s failed: %s\n", file_path, strerror(errno));
        if(fclose(body) != 0) {
            ERRPRINTF("fclose on %s failed: %s\n", file_path, strerror(errno));
        }
        free(file_path);
        SEND_ERR_RES(500, "Internal Server Error");
        return;
    }
    int file_len = ftell(body);
    // With a 64 bit integer, 21 characters are needed at most
    char file_len_str[21];
    snprintf(file_len_str, sizeof(file_len_str), "%u", file_len);
    if(file_len < 0) {
        ERRPRINTF("ftell on %s failed: %s\n", file_path, strerror(errno));
        if(fclose(body) != 0) {
            ERRPRINTF("fclose on %s failed: %s\n", file_path, strerror(errno));
        }
        free(file_path);
        SEND_ERR_RES(500, "Internal Server Error");
        return;
    }
    if(fseek(body, 0, SEEK_SET) != 0) {
        ERRPRINTF("rewind on %s failed: %s\n", file_path, strerror(errno));
        if(fclose(body) != 0) {
            ERRPRINTF("fclose on %s failed: %s\n", file_path, strerror(errno));
        }
        free(file_path);
        SEND_ERR_RES(500, "Internal Server Error");
        return;
    }

    time_t t = time(NULL);
    struct tm *tm = gmtime(&t);
    // We assume that 100 characters will be sufficient for the date
    char timestr[100];
    if(tm == NULL) {
        ERRPUTS("gmtime failed");
        free(file_path);
        fclose(conn);
        cleanup_exit(EXIT_FAILURE);
    }
    if(strftime(timestr, sizeof(timestr), "%a, %d %b %Y %T %Z", tm) == 0) {
        ERRPUTS("strftime failed");
        free(file_path);
        fclose(conn);
        cleanup_exit(EXIT_FAILURE);
    }
    http_header_t c_len_header = {"Content-Length", file_len_str, &conn_header};
    http_header_t date_header = {"Date", timestr, &c_len_header};
    res.status = 200;
    res.status_text = "OK";
    res.header_first = &date_header;
    res.body = body;
    res.body_len = -1;
    send_res(&res, conn);
    if(fclose(body) != 0) {
        ERRPRINTF("fclose on %s failed: %s\n", file_path, strerror(errno));
    }
    http_free_frame(req);
    free(file_path);
}

static void send_res(http_frame_t *res, FILE *conn) {
    printf("< %lu %s\n", res->status, res->status_text);
    int ret = http_send_res(conn, res);
    if(ret != HTTP_SUCCESS) {
        switch(ret) {
        case HTTP_ERR_STREAM:
            ERRPRINTF("error while sending response: %s\n", strerror(ferror(http_errvar)));
            break;
        default:
            ERRPRINTF("error while sending response: unknown error: %u\n", ret);
            fclose(conn);
            cleanup_exit(EXIT_FAILURE);
        }
    }
}

static char *get_file_path(char *req_path) {
    int docroot_trailing_slash = docroot[strlen(docroot)-1] == '/';
            
    int path_len = strlen(req_path) + strlen(docroot) + 1;
    int add_index = req_path[strlen(req_path)-1] == '/';
    if(add_index == 1) {
        path_len += strlen(index_file);
    }
    if(docroot_trailing_slash == 1) {
        path_len++;
    }

    char *file_path = malloc(path_len);
    if(file_path == NULL) {
        ERRPRINTF("malloc failed: %s\n", strerror(errno));
        cleanup_exit(EXIT_FAILURE);
    }

    if(docroot_trailing_slash == 1) {
        snprintf(file_path, path_len, "%s/%s", docroot, req_path);
    } else {
        snprintf(file_path, path_len, "%s%s", docroot, req_path);
    }
    if(add_index == 1) {
        strcat(file_path, index_file);
    }

    return file_path;
}

static void cleanup_exit(int status) {
    if(sockfd >= 0 ) {
        close(sockfd);
    }
    exit(status);
}
