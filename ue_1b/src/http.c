#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <strings.h>

#include "http.h"

void *http_errvar = NULL;

static int write_headers(FILE *sock, http_frame_t *res);

static int read_headers(FILE *sock, http_frame_t **res);

static int read_first_line(FILE *sock, char **line, char **first, char **second, char **third);

static int stream_pipe(FILE *src, FILE *drain, int len);

int parse_url(char *url, char **hostname, char **file_path) {
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

int http_frame(http_frame_t **frame) {
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

int http_send_req(FILE* sock, http_frame_t *req) {
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

int http_send_res(FILE* sock, http_frame_t *res, FILE *body) {
    if(fprintf(sock, "%s %lu %s\r\n", HTTP_VERSION, res->status, res->status_text) < 0) {
        http_errvar = sock;
        return HTTP_ERR_STREAM;
    }

    write_headers(sock, res);

    if(body != NULL) {
        int ret = stream_pipe(body, sock, -1);
        if(ret != HTTP_SUCCESS) {
            return ret;
        }
    }

    return HTTP_SUCCESS;
}

int http_recv_res(FILE *sock, http_frame_t **res, FILE *out) {
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

int http_recv_req(FILE* sock, http_frame_t **req) {
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
        return ret;
    }

    // Request bodies are not supported, ignore them
    return HTTP_SUCCESS;
}

static int read_first_line(FILE *sock, char **line, char **first, char **second, char **third) {
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

static int write_headers(FILE *sock, http_frame_t *res) {
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

static int read_headers(FILE *sock, http_frame_t **res) {
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

static int stream_pipe(FILE *src, FILE *drain, int len) {
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
