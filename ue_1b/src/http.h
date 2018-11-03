#ifndef HTTP_H
#define HTTP_H

#include <stdio.h>

#define HTTP_VERSION "HTTP/1.1"

typedef enum http_err {
    // Operation was successful
    HTTP_SUCCESS = 0, 

    // URL parsing failed due to invalid fomat
    HTTP_ERR_URL_FORMAT = 1,

    // An error outside the boundries of this module occoured -> consult errno()
    HTTP_ERR_INTERNAL = 2,

    // An error stdio error occured -> consult ferror()
    HTTP_ERR_STREAM = 3,

    // Protocol error occured during read or write from the network (e.g. invalid message format)
    HTTP_ERR_PROTOCOL = 4
} http_err_t;

typedef struct http_header {
    char *name;
    char *value;
    struct http_header *next;
} http_header_t;

typedef struct http_frame {
    long int status;
    char *status_text;
    
    char *method;
    char *file_path;
    char *port;

    int header_len;
    http_header_t *header_first;

    long int body_len;
    void *body;
} http_frame_t;

/**
 * @brief Check URL format and extract hostname and file path.
 * 
 * @param url URL that should be parsed.
 * @param hostname Pointer where the adress of extracted hostname buffer will be stored.
 * @param file_path Pointer where the adress of extracted file path buffer will be stored.
 * @return int HTTP_SUCCESS is extraction was successful, a different value of http_err_t otherwise.
 * 
 * @details Checks the given URL string for valid format and extracts the 
 * hostname and file path to two newly allocated buffers. The given hostname 
 * and file_path pointers will point to the buffers when the function returned 
 * successfully. Returns HTTP_SUCCESS if successful and a respective error 
 * code of the http_err_t enum otherwise.
 */
int parse_url(char *url, char **hostname, char **file_path);

int http_frame(http_frame_t **frame);

char *http_strerr(int err);

void http_free_frame(http_frame_t *frame);

//int http_req_get_frame(http_frame_t **frame, char *hostname, char *port, char *file_path);

int http_send_req(FILE* sock, http_frame_t *req);

//int http_send_res_frame(FILE* sock, http_frame_t *res);

//int http_recv_req_frame(FILE* sock, http_frame_t **req);

/**
 * @brief 
 * 
 * @param sock 
 * @param res 
 * @param out 
 * @return int
 *  
 * @details Does not free response http frame on error.
 */
int http_recv_res(FILE *sock, http_frame_t **res, FILE *out);

#endif
