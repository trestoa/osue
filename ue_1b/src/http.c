/**
 * @file http.c
 * @author Markus Klein (e11707252@student.tuwien.ac.at)
 * @brief Implementation of the http functions defined in http.h
 * @version 1.0
 * @date 2018-11-07
 * @details This module contains the implementation of the function defined in http.h.
 * As reading requests/responses and writing requests/responses have a lot in common 
 * most of the code (such as for reading headers, piping body between socket and files)
 *  is abstracted into common static functions.
 */

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <strings.h>

#include "http.h"

void *http_errvar = NULL;

/**
 * @brief Writes http headers.
 * 
 * @param sock Stdio stream where the headers should be serialized to.
 * @param res Http frame containing the headers.
 * @return http_err_t HTTP_SUCCESS if writing the headers was successful, and an 
 * error value as defined in http_err_t otherwise. 
 * 
 * @details Writes a linked list (starting with res->header_first) of headers 
 * fields (objects of http_header_t) to the given stdio stream. Each header 
 * will be written as single line ending with "\r\n". After all headers were 
 * written, the end header part of the http message will be indicated with an
 * empty line "\r\n".
 * Global variables: http_errvar.
 */
static http_err_t write_headers(FILE *sock, http_frame_t *res);

/**
 * @brief Reads headers from stream.
 * 
 * @param sock Stdio stream where the headers should be read from.
 * @param res Http frame where the start of the header list show be stored to.
 * @return http_err_t HTTP_SUCCESS if reading the headers was successful, and an 
 * error value as defined in http_err_t otherwise.
 * 
 * @details This is the reverse function of write_headers and reads all header
 * fields of a http message from the given stream into a linked list of dynamically
 * allocated http_header_t objects. A pointer to the first field will be stored to
 * res->header_first.
 * Global variables: http_errvar.
 */
static http_err_t read_headers(FILE *sock, http_frame_t **res);

/**
 * @brief Reads the three tokens of the first line of an http message.
 * 
 * @param sock Stream where the line should be read from.
 * @param line Pointer where the line pointer should be written to.
 * @param first Pointer where the pointer to the first token shoud be written to.
 * @param second Pointer where the pointer to the second token shoud be written to.
 * @param third Pointer where the pointer to the third token shoud be written to.
 * @return http_err_t http_err_t HTTP_SUCCESS if reading the tokens was successful, and an 
 * error value as defined in http_err_t otherwise.
 * 
 * @details Extracts three space separated tokens (the last token will 
 * contain the remaining line) from the given stream and stores the token pointer
 * to the given addresses. Line will pointer to the begin of the allocated memory space.
 * Global variables: http_errvar.
 */
static http_err_t read_first_line(FILE *sock, char **line, char **first, char **second, char **third);

/**
 * @brief Pipes the src to drain.
 * 
 * @param src Source of the data pipe.
 * @param drain Drain of the data pipe.
 * @param len Number of characters to be read from src and written to drain. The value
 * -1 indicates that the operation should last until EOF of src is reached.
 * @return http_err_t HTTP_SUCCESS if the pipe operation was successful, and an 
 * error value as defined in http_err_t otherwise.
 * 
 * @details Continously reads a block of data (up to 1024 bytes at a time) from src and
 * writes it to drain. Used for sending and receiving request/response body.
 * Global variables: http_errvar.
 */
static http_err_t stream_pipe(FILE *src, FILE *drain, int len);

/**
 * @brief Helper function for reading the remaining request if a protocol error occured
 * while parsing a request.
 * 
 * @param sock Stream where the request was sent to. 
 * @return http_err_t HTTP_SUCCESS if the operation was successfull and HTTP_ERR_STREAM
 * if an IO error occured will reading.
 * 
 * @details Reads the remaining part of a request (reads until a empty line "\r\n" is 
 * encountered) without performing any action on the received data. 
 * Global variables: http_errvar.
 */
static http_err_t skip_msg(FILE *sock);

http_err_t parse_url(char *url, char **hostname, char **file_path) {
    // 7 == length of "http://"
    if(strncmp(url, "http://", 7) != 0) {
        return HTTP_ERR_URL_FORMAT;
    }
    url += 7;

    char *hostname_end = strpbrk(url, ";/?:@=&");
    if(hostname_end == NULL) {
        return HTTP_ERR_URL_FORMAT;
    }

    size_t hostname_len = hostname_end - url;
    if(hostname_len == 0) {
        return HTTP_ERR_URL_FORMAT;
    }
    *hostname = malloc(hostname_len + 1);
    if(*hostname == NULL) {
        return HTTP_ERR_INTERNAL;
    }
    memset(*hostname, '\0', hostname_len + 1);
    strncpy(*hostname, url, hostname_len/sizeof(char));

    size_t file_path_len = strlen(hostname_end);
    *file_path = malloc(file_path_len + 1);
    if(*file_path == NULL) {
        return HTTP_ERR_INTERNAL;
    }
    memset(*file_path, '\0', file_path_len + 1);
    strcpy(*file_path, hostname_end);

    return HTTP_SUCCESS;
}

http_err_t http_frame(http_frame_t **frame) {
    *frame = malloc(sizeof(http_frame_t));
    if(*frame == NULL) {
        return HTTP_ERR_INTERNAL;
    }
    memset(*frame, 0, sizeof(**frame));
    return HTTP_SUCCESS;
}

void http_free_frame(http_frame_t *frame) {
    free(frame->status_text);
    free(frame->method);
    free(frame->file_path);
    free(frame->body);

    http_header_t *last_header, *cur_header = frame->header_first;
    while(cur_header != NULL) {
        last_header = cur_header;
        cur_header = cur_header->next;
        free(last_header->name);
        free(last_header->value);
        free(last_header);
    }

    free(frame);
}

http_err_t http_send_req(FILE* sock, http_frame_t *req) {
    if(fprintf(sock, "%s %s %s\r\n", req->method, req->file_path, HTTP_VERSION) < 0) {
        http_errvar = sock;
        return HTTP_ERR_STREAM;
    }

    write_headers(sock, req);

    if(req->body_len != 0) {
        if(fwrite(req->body, 1, req->body_len, sock) != req->body_len) {
            http_errvar = sock;
            return HTTP_ERR_STREAM;
        }
    }
    if(fflush(sock) != 0) {
        return HTTP_ERR_INTERNAL;
    }
    return HTTP_SUCCESS;
}

http_err_t http_send_res(FILE* sock, http_frame_t *res) {
    if(fprintf(sock, "%s %lu %s\r\n", HTTP_VERSION, res->status, res->status_text) < 0) {
        http_errvar = sock;
        return HTTP_ERR_STREAM;
    }

    write_headers(sock, res);

    if(res->body != NULL) {
        int ret = stream_pipe(res->body, sock, res->body_len);
        if(ret != HTTP_SUCCESS) {
            return ret;
        }
    }

    return HTTP_SUCCESS;
}

http_err_t http_recv_res(FILE *sock, http_frame_t **res, FILE *out) {
    int ret = http_frame(res);
    if(ret != HTTP_SUCCESS){
        return ret;
    }
    
    char *first_line = NULL,
         *http_version = NULL,
         *status = NULL,
         *status_text = NULL;
    ret = read_first_line(sock, &first_line, &http_version, &status, &status_text);
    if(ret != HTTP_SUCCESS) {
        free(first_line);
        return ret;
    }

    // Check http version
    if(strcmp(http_version, HTTP_VERSION) != 0) {
        free(first_line);
        return HTTP_ERR_PROTOCOL;
    }
    // Convert status
    errno = 0;
    (*res)->status = strtol(status, NULL, 10);
    // Check for various possible errors (code from man 3 strtol)
    if ((errno == ERANGE && ((*res)->status == LONG_MAX || (*res)->status == LONG_MIN))
            || (errno != 0 && (*res)->status == 0)) {
        free(first_line);
        return HTTP_ERR_PROTOCOL;
    }
    // Save status text
    (*res)->status_text = strdup(status_text);
    if((*res)->status_text == NULL) {
        free(first_line);
        return HTTP_ERR_INTERNAL;
    }
    free(first_line);

    // Continue parsing the other headers
    ret = read_headers(sock, res);
    if(ret != HTTP_SUCCESS){
        return ret;
    }

    // Now write body to out
    // Only write body of status == 200
    if((*res)->status != 200) {
        return HTTP_SUCCESS;
    }

    ret = stream_pipe(sock, out, (*res)->body_len);
    if(ret != HTTP_SUCCESS) {
        return ret;
    }

    return HTTP_SUCCESS;
}

http_err_t http_recv_req(FILE* sock, http_frame_t **req) {
    int ret = http_frame(req);
    if(ret != HTTP_SUCCESS){
        return ret;
    }

    char *first_line = NULL,
         *method = NULL,
         *file_path = NULL,
         *http_version = NULL;
    ret = read_first_line(sock, &first_line, &method, &file_path, &http_version);
    if(ret != HTTP_SUCCESS) {
        free(first_line);
        return ret;
    }
    // Check http version
    if(strcmp(http_version, HTTP_VERSION) != 0) {
        free(first_line);
        if((ret = skip_msg(sock)) != HTTP_SUCCESS) {
            return ret;
        }
        return HTTP_ERR_PROTOCOL;
    }
    // Save method and file path
    (*req)->method = strdup(method);
    if((*req)->method == NULL) {
        free(first_line);
        return HTTP_ERR_INTERNAL;
    }
    (*req)->file_path = strdup(file_path);
    if((*req)->file_path == NULL) {
        free(first_line);
        return HTTP_ERR_INTERNAL;
    }
    free(first_line);

    // Read headers
    ret = read_headers(sock, req);
    if(ret != HTTP_SUCCESS){
        if(ret == HTTP_ERR_PROTOCOL) {
            int skip_ret;
            if((skip_ret = skip_msg(sock)) != HTTP_SUCCESS) {
                return skip_ret;
            }
        }
        return ret;
    }

    // Request bodies are not supported, ignore them
    return HTTP_SUCCESS;
}

static http_err_t read_first_line(FILE *sock, char **line, char **first, char **second, char **third) {
    size_t linecap = 0;
    ssize_t linelen;
    
    // Read first header line with response status
    if((linelen = getline(line, &linecap, sock)) <= 0) {
        http_errvar = sock;
        return HTTP_ERR_STREAM;
    }

    char* tok = strtok(*line, " ");
    if(tok == NULL) {
        return HTTP_ERR_PROTOCOL;
    }
    *first = tok;

    if((tok = strtok(NULL, " ")) == NULL) {
        return HTTP_ERR_PROTOCOL;
    }
    *second = tok;

    if((tok = strtok(NULL, "\r")) == NULL) {
        return HTTP_ERR_PROTOCOL;
    }
    *third = tok;

    return HTTP_SUCCESS;
}

static http_err_t write_headers(FILE *sock, http_frame_t *res) {
    for(http_header_t *cur_header = res->header_first; cur_header != NULL; cur_header = cur_header->next) {
        if(fprintf(sock, "%s: %s\r\n", cur_header->name, cur_header->value) < 0) {
            http_errvar = sock;
            return HTTP_ERR_STREAM;
        }
    }
    if(fputs("\r\n", sock) == EOF) {
        http_errvar = sock;
        return HTTP_ERR_STREAM;
    }
    return HTTP_SUCCESS;
}

static http_err_t read_headers(FILE *sock, http_frame_t **res) {
    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    
    // Body length of -1 indicates that no content-length header was present
    (*res)->body_len = -1;

    http_header_t *last_header = NULL, *cur_header;
    for(int len = 0; ; last_header = cur_header, len++) {
        if((linelen = getline(&line, &linecap, sock)) <= 0) {
            free(line);
            http_errvar = sock;
            return HTTP_ERR_STREAM;
        }

        if(strcmp(line, "\r\n") == 0) {
            break;
        }

        cur_header = malloc(sizeof(http_frame_t));
        if(cur_header == NULL) {
            free(line);
            return HTTP_ERR_INTERNAL;
        }
        memset(cur_header, 0, sizeof(*cur_header));
        if((*res)->header_first == NULL) {
            (*res)->header_first = cur_header;
        } else {
            last_header->next = cur_header;
        }

        char *sep = strpbrk(line, ":");
        if(sep == NULL) {
            free(line);
            return HTTP_ERR_PROTOCOL;
        }
        sep[0] = '\0';
        cur_header->name = strdup(line);
        if(cur_header->name == NULL) {
            free(line);
            return HTTP_ERR_INTERNAL;
        }

        // strip delimiter and whitespaces
        do {
            if((sep - line)/sizeof(*line) >= linelen) {
                free(line);
                return HTTP_ERR_PROTOCOL;
            }
            sep++;
        } while(sep[0] == ' ');
        cur_header->value = strdup(sep);
        if(cur_header->value == NULL) {
            free(line);
            return HTTP_ERR_INTERNAL;
        }

        // Check for content length
        if(strcasecmp(cur_header->name, "Content-Length") == 0) {
            errno = 0;
            (*res)->body_len = strtol(cur_header->value, NULL, 10);
            if(((errno == ERANGE && ((*res)->body_len == LONG_MAX || (*res)->body_len == LONG_MIN))
                || (errno != 0 && (*res)->body_len == 0))) {
                free(line);
                return HTTP_ERR_PROTOCOL;
            }
        }
    }
    free(line);
    return HTTP_SUCCESS;
}

static http_err_t stream_pipe(FILE *src, FILE *drain, int len) {
    char buf[1024];
    memset(buf, 0, sizeof(buf));
    int to_read, act_read, body_remaining;
    
    if(len != -1) {
        body_remaining = len;
    } else {
        body_remaining = sizeof(buf);
    }
    while(body_remaining > 0) {
        to_read = sizeof(buf) < body_remaining ? sizeof(buf) : body_remaining;
        if((act_read = fread(buf, 1, to_read, src)) != to_read) {
            if(len == -1 && feof(src) != 0) {
                body_remaining = 0;
            } else {
                http_errvar = src;
                return HTTP_ERR_STREAM;
            }
        }

        if(fwrite(buf, 1, act_read, drain) != act_read) {
            http_errvar = drain;
            return HTTP_ERR_STREAM;
        }
        
        if(len != -1) {
            body_remaining -= to_read;
        }
    }

    fflush(drain);
    return HTTP_SUCCESS;
}

static http_err_t skip_msg(FILE *sock) {
    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    while(strcmp(line, "\r\n") != 0) {
        if((linelen = getline(&line, &linecap, sock)) <= 0) {
            free(line);
            http_errvar = sock;
            return HTTP_ERR_STREAM;
        }
    }

    free(line);
    return HTTP_SUCCESS;
}
