/**
 * @file client.c
 * @author Markus Klein (e11707252@student.tuwien.ac.at)
 * @brief Implementation of the http client for exercise 1b.
 * @version 1.0
 * @date 2018-11-07
 * @details Implementation of a very simplified http client which
 * is able to perform GET requests. The code in this module consists
 * mostly of setup code resource management while the specifics on the
 * http protocol are provided by the http module. 
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <libgen.h>

#include "utils.h"
#include "http.h"

/**
 * @brief Status code for protocol errors.
 * @details The client exists with this status code if an http protocol
 * error occurs (e.g. the server returns an invalid response).
 */
#define EXIT_PROTOCOL_ERR 2

/**
 * @brief Status code for http errors.
 * @details The client exists with this status code if the server response
 * has a status code != 200 (OK).
 */
#define EXIT_STATUS_ERR 3

/**
 * @brief Program name.
 * @details Name of the executable used for usage and error messages.
 */
static char *progname;

/**
 * @brief Server port.
 * Port number of the server (-p cli argument)
 */
static char *port = "http";

/**
 * @brief Output directory path option argument.
 * Output directory to which the response body should be written to.
 * (the -d cli argument)
 */
static char *outdir_opt = NULL;

/**
 * @brief Output file path option argument.
 * Output file where the the response body should be written to.
 * (the -o cli argument)
 */
static char *outfile_opt = NULL;

// These are all pointers to allocated memory space -> free needed.
/**
 * @brief Hostname part of the URL
 * @details Hostname part of the request URL (e.g. "localhost" in 
 * http://localhost/somedir/index.html). 
 * Dynamically allocated memory, needs to be freed!
 */
static char *hostname = NULL;
            
/**
 * @brief File path part of the URL
 * @details File path  part of the request URL (e.g. "/somedir/index.html" in 
 * http://localhost/somedir/index.html).
 * Dynamically allocated memory, needs to be freed!
 */
static char *file_path = NULL;

/**
 * @brief Server connection.
 * @details Server connection stram.
 */
static FILE *sock = NULL;

/**
 * @brief Indicates whether outfile_path needs to be freed.
 * @details if == 1 outfile_path points to dynamically memory which needs to be cleared
 * on program termination.
 */
static int outfile_path_alloc = 0;

/**
 * @brief Path to outfile.
 * @details Path where the reponse body should be written to.
 */
static char *outfile_path = NULL;

/**
 * @brief Output stream.
 * Stream for the output file.
 */
static FILE *out = NULL;

/**
 * Print usage. 
 * @brief Prints synopsis of the http client program.
 * 
 * @details Prints the usage message of the client on sterr and 
 * terminates the program with EXIT_FAILURE.
 * Global variables: progname.
 */
static void usage(void);

/**
 * @brief Establish connection to server.
 * @details Opens a socket connection to the server using the hostname and 
 * port stored in the variables "hostname" and "port". The resulting stdio 
 * stream is stored to the "sock" variable. 
 * Global variables: hostname, port, sock.
 */
static void connect_to_server(void);

/**
 * @brief Handle error codes returned by functions of the http module.
 * 
 * @param err Error code returned by the function (a values defined in http_err_t).
 * @param cause Description during which operation the error occured, used 
 * for the error message printed to stdout.
 * 
 * @details Handles the given error by printing an appropriate error message and 
 * initiating a sane shutdown of the program with the correponding exit status. 
 */
static void handle_http_err(int err, char *cause);

/**
 * @brief Removes the located at outfile_path.
 * @details Removes the located at outfile_path if != NULL, ignoring a possible ENOENT
 * error (other errors are handled by printing an error message).
 * Global variables: outfile_path.
 */
static void clear_outfile(void);

/**
 * Cleanup and terminate.
 * @brief Free allocated memory, close open streams and terminate program with the
 * given status code.
 * 
 * @param status Returns status of the program.
 * 
 * @details Closes open file streams (pointers != NULL) and exits the program using 
 * exit(), returning the given status.
 * Global variables: hostname, file_path, outfile_path, outfile_path_alloc, sock, out.
 */
static void cleanup_exit(int status);

/**
 * @brief Sends a GET request to the server and handles the response.
 * @details Uses the functions of the http module to send a http GET request
 * to the server and receive the corresponding reply. As required by the exercise
 * description the Host and Connection headers are set in the request. The request 
 * body is written either to a file or to stdout. If the server responds with an 
 * malformed response, the programs exists with EXIT_PROTOCOL_ERR. If a reponse
 * status code != 200 is encountered, the exit code will be EXIT_STATUS_ERR.
 * Global variables: hostname, file_path, sock, outfile_path, out.
 */
static void perform_request(void);

/**
 * @brief Computes the output file where the request body should be stored.
 * @details Depending on the argument passed to the program, this function either
 * sets outfile_path to NULL (if neither the -o nor the -d option were present) or 
 * to the output file path. outfile_path_alloc is set to 1 if outfile_path points to
 * dynamically allocated memory.
 * Global variables: hostname, file_path, outfile_path, outfile_path_alloc.
 */
static void extract_out_file(void);

/**
 * @brief Main method for the http server. Parses the command line arguments and
 * calls the functions which perform the actual http request.
 * 
 * @param argc Argument counter.
 * @param argv Argument vector.
 * @return int Program exit code (EXIT_SUCCESS)
 * 
 * @details Reads the command line arguments via getopt and checks for the correct 
 * number of arguments. If the argument count and provided options are correct, the 
 * subroutines for connecting to the server, performing the request, handling the
 * response and shutting down the program are called. 
 * Global variables: progname, outfile_opt, outdir_opt.
 */
int main(int argc, char **argv) {
    progname = argv[0];
    char *url = NULL;

    int c;
    while((c = getopt(argc, argv, "p:o:d:")) != -1) {
        switch(c) {
        case 'p':
            port = optarg;
            break;
        case 'o':
            outfile_opt = optarg;
            break;
        case 'd':
            outdir_opt = optarg;
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

    if(outdir_opt != NULL && outfile_opt != NULL) {
        ERRPUTS("Either the -d or -o argument may be present, but not both them.\n");
        usage();
    }

    url = argv[0];
    int ret = parse_url(url, &hostname, &file_path);
    if(ret != 0) {
        handle_http_err(ret, url);
        exit(EXIT_FAILURE);
    }

    extract_out_file();
    
    connect_to_server();
    perform_request();
    
    cleanup_exit(EXIT_SUCCESS);
}

static void usage(void) {
    fprintf(stderr, "Usage: %s [-p PORT] [ -o FILE | -d DIR ] URL\n", progname);
    exit(EXIT_FAILURE);
}

static void connect_to_server(void) {
    struct addrinfo hints, *ai;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    int res = getaddrinfo(hostname, port, &hints, &ai);
    if(res != 0) {
        ERRPRINTF("getaddrinfo failed: %s\n", gai_strerror(res));
        cleanup_exit(EXIT_FAILURE);
    }

    int sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if(sockfd < 0) {
        ERRPRINTF("socket failed: %s\n", strerror(errno));
        freeaddrinfo(ai);
        cleanup_exit(EXIT_FAILURE);
    }

    if(connect(sockfd, ai->ai_addr, ai->ai_addrlen) < 0) {
        ERRPRINTF("connect failed: %s\n", strerror(errno));
        freeaddrinfo(ai);
        cleanup_exit(EXIT_FAILURE);
    }
    freeaddrinfo(ai);

    if((sock = fdopen(sockfd, "w+")) == NULL) {
        ERRPRINTF("fdopen failed: %s\n", strerror(errno));
        cleanup_exit(EXIT_FAILURE);
    }
}

static void perform_request(void) {
    http_frame_t frame, *res;
    memset(&frame, 0, sizeof(frame));
    frame.method = "GET";
    frame.file_path = file_path;
    frame.header_len = 2;
    
    // "Host: " is 6 characters + null byte -> allocate length of hostname + 7
    http_header_t conn_header = {"Connection", "close", NULL};
    http_header_t host_header = {"Host", hostname, &conn_header};
    frame.header_first = &host_header;

    // Send request
    int ret = http_send_req(sock, &frame);
    if(ret != HTTP_SUCCESS) {
        handle_http_err(ret, "error while sending request");
    }
    
    // TODO: only open outfile when request successful
    // Open output file
    out = stdout;
    if(outfile_path != NULL) {
        if((out = fopen(outfile_path, "w")) == NULL) {
            ERRPRINTF("fopen on %s failed: %s\n", outfile_path, strerror(errno));
            cleanup_exit(EXIT_FAILURE);
        }
    }

    // Receive response
    ret = http_recv_res(sock, &res, out);
    if(ret != HTTP_SUCCESS) {
        http_free_frame(res);
        handle_http_err(ret, "error while receiving response");
    }

    if(res->status != 200) {
        ERRPRINTF("server returned with status: %lu %s\n", res->status, res->status_text);
        clear_outfile();
        http_free_frame(res);
        cleanup_exit(EXIT_STATUS_ERR);
    }
    http_free_frame(res);
}

static void extract_out_file(void) {
    if(outfile_opt == NULL && outdir_opt == NULL) {
        outfile_path = NULL;
    } else {
        if(outfile_opt != NULL) {
            outfile_path = outfile_opt;
        } else {
            outfile_path_alloc = 1;
            char *filename;
            if(file_path[strlen(file_path)-1] == '/') {
                filename = "index.html";
            } else {
                filename = basename(file_path);
            }

            int trailing_slash = outdir_opt[strlen(outdir_opt)-1] == '/';
            // Add 1 additional character for null byte
            int path_len = strlen(outdir_opt) + strlen(filename) + (trailing_slash == 1 ? 0 : 1) + 1;

            outfile_path = malloc(path_len * sizeof(*outfile_path));
            if(outfile_path == NULL) {
                ERRPRINTF("malloc failed: %s\n", strerror(errno));
                cleanup_exit(EXIT_FAILURE);
            }
            
            if(trailing_slash == 0) {
                snprintf(outfile_path, path_len, "%s/%s", outdir_opt, filename);
            } else {
                snprintf(outfile_path, path_len, "%s%s", outdir_opt, filename);
            }
        }
    }
}

static void handle_http_err(int err, char *cause) {
    int status = EXIT_FAILURE;
    switch(err) {
    case HTTP_ERR_URL_FORMAT:
        ERRPRINTF("'%s' is not a valid url\n", cause);
        break;
    case HTTP_ERR_INTERNAL:
        ERRPRINTF("%s: %s\n", cause, strerror(errno));
        break;
    case HTTP_ERR_STREAM:
        if (feof(sock) != 0) {
            ERRPRINTF("%s: EOF reached\n", cause);
            break;
        }
        ERRPRINTF("%s: %s\n", cause, strerror(ferror(sock)));
        break;
    case HTTP_ERR_PROTOCOL:
        ERRPUTS("Protocol error!\n");
        status = EXIT_PROTOCOL_ERR;
        break;
    default:
        ERRPRINTF("%s: unknown error", cause);
    }
    clear_outfile();
    cleanup_exit(status);
}

static void clear_outfile(void) {
    if(outfile_path != NULL) {
        if(remove(outfile_path) != 0) {
            if(errno != ENOENT) {
                ERRPRINTF("remove %s failed: %s\n", outfile_path, strerror(errno));
            }
        }
    }
}

static void cleanup_exit(int status) {
    free(hostname);
    free(file_path);
    if(outfile_path_alloc == 1) {
        free(outfile_path);
    }

    if(sock != NULL) {
        fclose(sock);
    }

    if(out != NULL) {
        fclose(out);
    }
    exit(status);
}
