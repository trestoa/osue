/**
 * @file client.c
 * @author Markus Klein (e11707252@student.tuwien.ac.at)
 * @brief 
 * @version 1.0
 * @date 2018-11-07
 * 
 * 
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

#define EXIT_PROTOCOL_ERR 2
#define EXIT_STATUS_ERR 3

static char *progname;

// These are all pointers to non allocated memory space -> no free needed.
static char *port = "http", 
     *outdir = NULL, 
     *outfile = NULL;

// These are all pointers to allocated memory space -> free needed.
static char *hostname = NULL,
            *file_path = NULL;

static struct addrinfo *ai = NULL;
static FILE *sock = NULL, *out = NULL;

static void usage(void);

static void connect_to_server(void);

static void handle_http_err(int err, char *cause);

static void cleanup_exit(int status);

static void perform_request(void);

static void open_out_file(void);

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
    int ret = parse_url(url, &hostname, &file_path);
    if(ret != 0) {
        handle_http_err(ret, url);
        exit(EXIT_FAILURE);
    }
    
    connect_to_server();
    perform_request();
    
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
    int res = getaddrinfo(hostname, port, &hints, &ai);
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
    open_out_file();

    // Receive response
    ret = http_recv_res(sock, &res, out);
    if(ret != HTTP_SUCCESS) {
        http_free_frame(res);
        handle_http_err(ret, "error while receiving response");
    }

    if(res->status != 200) {
        ERRPRINTF("server returned with status: %lu %s\n", res->status, res->status_text);
        http_free_frame(res);
        cleanup_exit(EXIT_STATUS_ERR);
    }
    http_free_frame(res);
}

static void open_out_file(void) {
    if(outfile == NULL && outdir == NULL) {
        out = stdout;
    } else {
        char *path;
        int outfile_alloc = 0;
        if(outfile != NULL) {
            path = outfile;
        } else {
            char *filename = basename(file_path);
            if(strcmp(filename, "/")) {
                filename = "index.html";
            }

            int trailing_slash = outdir[strlen(outdir)-1] == '/';
            // Add 1 additional character
            int path_len = strlen(outdir) + strlen(filename) + (trailing_slash == 1 ? 0 : 1) + 1;

            outfile_alloc = 1;
            path = malloc(path_len * sizeof(*path));
            if(path == NULL) {
                ERRPRINTF("malloc failed: %s\n", strerror(errno));
                cleanup_exit(EXIT_FAILURE);
            }
            
            if(trailing_slash == 0) {
                snprintf(path, path_len, "%s/%s", outdir, filename);
            } else {
                snprintf(path, path_len, "%s%s", outdir, filename);
            }
        }

        if((out = fopen(path, "w")) == NULL) {
            ERRPRINTF("fopen on %s failed: %s\n", path, strerror(errno));
            if(outfile_alloc == 1) {
                free(path);
            }
            cleanup_exit(EXIT_FAILURE);
        }
        if(outfile_alloc == 1) {
            free(path);
        }
    }
}

static void handle_http_err(int err, char *cause) {
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
        ERRPUTS("Protocol error!");
        cleanup_exit(EXIT_PROTOCOL_ERR);
    default:
        ERRPRINTF("%s: unknown error", cause);
    }
    cleanup_exit(EXIT_FAILURE);
}

static void cleanup_exit(int status) {
    free(hostname);
    free(file_path);
    if(ai != NULL) {
        freeaddrinfo(ai);
    }

    if(sock != NULL) {
        fclose(sock);
    }

    if(out != NULL) {
        fclose(out);
    }
    exit(status);
}
