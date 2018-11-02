#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

#include "http.h"

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

int http_send_req(FILE* sock, http_frame_t *req) {
    if(fprintf(sock, "%s %s %s\r\n", req->method, req->file_path, HTTP_VERSION) < 0) {
        return HTTP_ERR_STREAM;
    }

    for(http_header_t *cur_header = req->header_first; cur_header != NULL; cur_header = cur_header->next) {
        if(fprintf(sock, "%s: %s\r\n", cur_header->name, cur_header->value) < 0) {
            return HTTP_ERR_STREAM;
        }
    }
    if(fputs("\r\n\r\n", sock) == EOF) {
        return HTTP_ERR_STREAM;
    }

    if(req->body_len != 0) {
        if(fwrite(req->body, 1, req->body_len, sock) != req->body_len) {
            return HTTP_ERR_STREAM;
        }
    }
    if(fflush(sock) != 0) {
        return HTTP_ERR_INTERNAL;
    }
    return HTTP_SUCCESS;
}

int http_recv_res(FILE* sock, http_frame_t **res, FILE *out) {
    int ret = http_frame(res);
    if(ret != HTTP_SUCCESS){
        return ret;
    }
    
    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    // Read first header line with response status
    if((linelen = getline(&line, &linecap, sock)) <= 0) {
        return HTTP_ERR_STREAM;
    }

    char* tok = strtok(line, " ");
    if(tok == NULL || strcmp(tok, HTTP_VERSION) != 0) {
        return HTTP_ERR_PROTOCOL;
    }

    errno = 0;
    tok = strtok(NULL, " ");
    (*res)->status = strtol(tok, NULL, 10);
    // Check for various possible errors (code from man 3 strtol)
    if ((errno == ERANGE && ((*res)->status == LONG_MAX || (*res)->status == LONG_MIN))
            || (errno != 0 && (*res)->status == 0)) {
        return HTTP_ERR_PROTOCOL;
    }
    // The rest of the first header line is ignored here for simplicity
    // Continue parsing the other headers

    while(1) {
        if((linelen = getline(&line, &linecap, sock)) <= 0) {
            if(feof(sock) != 0) {
                break;
            }
            return HTTP_ERR_STREAM;
        }
        printf(line);
    }

    return HTTP_SUCCESS;
}