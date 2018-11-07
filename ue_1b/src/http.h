#ifndef HTTP_H
#define HTTP_H

#include <stdio.h>

// TODO: document structs

/**
 * @brief Http version.
 * @details Http version supported by this module and used for sending and validating
 * request and response headers.
 */
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
    long int status; // Response only
    char *status_text; // Response only
    
    char *method; // Request only
    char *file_path; // Request only

    int header_len; 
    http_header_t *header_first;

    long int body_len;

    void *body; // Response only
} http_frame_t;

extern void *http_errvar;

/**
 * @brief Check URL format and extract hostname and file path.
 * 
 * @param url URL that should be parsed.
 * @param hostname Pointer where the address of extracted hostname buffer will be stored.
 * @param file_path Pointer where the address of extracted file path buffer will be stored.
 * @return int HTTP_SUCCESS is extraction was successful, a different value of http_err_t otherwise.
 * 
 * @details Checks the given URL string for valid format and extracts the 
 * hostname and file path to two newly allocated buffers. The given hostname 
 * and file_path pointers will point to the buffers when the function returned 
 * successfully. Returns HTTP_SUCCESS if successful and a respective error 
 * code of the http_err_t enum otherwise.
 */
int parse_url(char *url, char **hostname, char **file_path);

/**
 * @brief Initialize a new http_frame_t on the heap.
 * 
 * @param frame Pointer where the address to the http frame will be stored.
 * @return int HTTP_SUCCESS if the frame initialization was successful, HTTP_ERR_INTERNAL
 * if malloc returned with an error.
 * 
 * @details Allocates heap space for a http_frame_t object and initializes all values with 0.
 * The address to the frame will be stored to the given pointer.
 */
int http_frame(http_frame_t **frame);

/**
 * @brief Frees a dynamically allocated http frame object.
 * 
 * @param frame The frame which should be freed.
 * 
 * @details Frees the memory allocated for the given http frame, including all
 * its values. This function assumes (and may therefore only be called if those
 * assuptions apply to the frame) that all values (e.g. status_text) including the 
 * http headers objects are also pointers to dynamically allocated memory. 
 */
void http_free_frame(http_frame_t *frame);

/**
 * @brief Send a http request.
 * 
 * @param sock Socket where the request will be sent to. 
 * @param req Http frame which describes the request.
 * @return int HTTP_SUCCESS if the request was successfully sent and a error value 
 * as defined in http_err_t otherwise.
 * 
 * @details Sends a http request with the method req->method to sock, containing
 * all headers in the linked list beginning with req->header_first and, if 
 * req->content_len > 0 the request body req->body. All headers (especially)
 * the Content-Length must be already set correctly. 
 */
int http_send_req(FILE* sock, http_frame_t *req);

int http_send_res(FILE* sock, http_frame_t *res, FILE *body);

int http_recv_req(FILE* sock, http_frame_t **req);

/**
 * @brief Receive a http response from the given socket.
 * 
 * @param sock Socket where the response should be read from.
 * @param res Pointer where the address of the http response frame will be stored. 
 * @param out Output stream where the response body will be written to. 
 * @return int HTTP_SUCCESS if the response was successfully received and a 
 * error value as defined in http_err_t otherwise. 
 *  
 * @details Reads an http response from sock. If the response status == 200, it will
 * also read the response body and write it to out. This function will not free the
 * http response frame when an error occurs while reading the response as it might
 * contain relevant debugging information. Thus, if res != NULL after the function 
 * returns and the response frame will not be used further, http_free_frame should be
 * called manually. 
 */
int http_recv_res(FILE *sock, http_frame_t **res, FILE *out);

#endif
