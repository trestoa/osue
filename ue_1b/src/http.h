/**
 * @file http.h
 * @author Markus Klein (e11707252@student.tuwien.ac.at)
 * @brief Contains function and data structures for sending and receiving http messages.
 * @version 1.0
 * @date 2018-11-07
 * @details This module contains functions which essentially implement parts of the 
 * http protocol and allow users to send and receive http requests and responses.
 * For storing the data of a http message and passing it between the used and this 
 * module, the http_frame_t type is used. 
 * The return values of most functions indicate whether the operation succeeded and 
 * if not, which type of error occured. For this purpose, functions return a value 
 * of the http_err_t type. Additionally for stdio stream errors, the http_errval 
 * variable is set to the stream which caused the error to happen. 
 */

#ifndef HTTP_H
#define HTTP_H

#include <stdio.h>

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

/**
 * @brief Struct for storing data of a single header field.
 * @details This is a helper structure for http_frame_t and contains the data
 * for a single header field where name contains the left part of the header 
 * and value the right path (the serialization to the http header string would
 * be "<name>: <value>"). In addition to that, it contains a pointer to a potential
 * next header field. 
 */
typedef struct http_header {
    char *name;
    char *value;
    struct http_header *next;
} http_header_t;

/**
 * @brief Stores information about a http message (request or reply).
 * @details This structure is used for passing around http message data within
 * the program and in particular between function of the module and the calling
 * function. It contains fields for both request and response messages, where some 
 * field are request only (method, file_path) and some are response only 
 * (status, status_text). Headers are stored as a linked list of http_header_t 
 * pointers. 
 */
typedef struct http_frame {
    long int status; // Response only
    char *status_text; // Response only
    
    char *method; // Request only
    char *file_path; // Request only

    int header_len; 
    http_header_t *header_first;

    long int body_len;
    void *body;
} http_frame_t;

/**
 * @brief Error variable used to indicated error causes 
 * (in particular, streams that caused an error).
 * @details This global variable is used to indicate methods the cause of
 * errors by pointing (corrently only, but generally not limited) to streams
 * where the error occured. 
 */
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
http_err_t parse_url(char *url, char **hostname, char **file_path);

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
http_err_t http_frame(http_frame_t **frame);

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
http_err_t http_send_req(FILE* sock, http_frame_t *req);

/**
 * @brief Send a http reponse.
 * 
 * @param sock Socket where the response will be sent to. 
 * @param res Http frame which describes the response.
 * @param body Stdio stream where the body will be read from. 
 * @return http_err_t HTTP_SUCCESS if the request was successfully sent and a error value 
 * as defined in http_err_t otherwise.
 * 
 * @details Sends a http response with the status code res->status and res->status_text
 * to sock, containing all headers in the linked list beginning with res->header_first
 * and, if != NULL, the request body res->body. All headers (especially)
 * the Content-Length must be already set correctly. 
 * A res->body_len of -1 indicates that the stream should be read until EOF.
 */
http_err_t http_send_res(FILE* sock, http_frame_t *res);

/**
 * @brief Receive a http request from the given socket.
 * 
 * @param sock Socket where the request should be read from.
 * @param req Pointer where the address of the http request frame will be stored. 
 * @return http_err_t HTTP_SUCCESS if the request was successfully received and a 
 * error value as defined in http_err_t otherwise. 
 * 
 * @details Reads an http request from sock and stores that request data in a newly 
 * allocated http_frame_t struct. This function does not support request bodies and 
 * will also not consider the Content-Length header and drop the request body from
 * the stream. Requests with request bodies will therefore leave the body unread
 * on the stream.
 */
http_err_t http_recv_req(FILE* sock, http_frame_t **req);

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
http_err_t http_recv_res(FILE *sock, http_frame_t **res, FILE *out);

#endif
