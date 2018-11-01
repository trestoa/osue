#include <string.h>
#include <stdlib.h>

#include "http.h"

int parse_url(char *url, char **hostname, char **file_path) {
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

/*
int http_req_get_frame(http_frame_t **frame, char *hostname, char *port, char *file_path) {
    int res = http_frame(frame);
    if(res != HTTP_SUCCESS) {
        return res;
    }

    // strdup all parameters as to make sure that the struct can be cleaned up again afterwards
    (*frame)->method = strdup("GET");
    if((*frame)->method == NULL) {
        return HTTP_ERR_INTERNAL;
    }

    (*frame)->port = strdup(port);
    if((*frame)->port == NULL) {
        return HTTP_ERR_INTERNAL;
    }

    (*frame)->file_path = strdup(file_path);
    if((*frame)->file_path == NULL) {
        return HTTP_ERR_INTERNAL;
    }

    (*frame)->header_len = 1;
    (*frame)->headers = malloc(2 * sizeof(http_header_t*));
    if((*frame)->headers == NULL) {
        return HTTP_ERR_INTERNAL;
    }
    // "Host: " is 6 characters + zero byte -> allocate length of hostname + 7
    size_t hostlen = strlen(hostname) + 7;
    (*frame)->headers[0] = malloc(hostlen * sizeof(char));
    if((*frame)->headers[0] == NULL) {
        return HTTP_ERR_INTERNAL;
    }
    snprintf((*frame)->headers[0], hostlen, "Host: %s", hostname);

    (*frame)->headers[1] = strdup("Connection: close");
    if((*frame)->headers[1] == NULL) {
        return HTTP_ERR_INTERNAL;
    }

    return HTTP_SUCCESS;
}*/

int http_send_req_frame(FILE* sock, http_frame_t *req) {
    printf("%s %s %s\r\n", req->method, req->file_path, HTTP_VERSION);
    if(fprintf(sock, "%s %s %s\r\n", req->method, req->file_path, HTTP_VERSION) < 0) {
        return HTTP_ERR_STREAM;
    }
    for(int i = 0; i < req->header_len; i++) {
        printf("%s\r\n", req->headers[i]);
        if(fprintf(sock, "%s\r\n", req->headers[i]) < 0) {
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

int http_recv_res_frame(FILE* sock, http_frame_t **res) {
    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    do {
        if((linelen = getline(&line, &linecap, sock)) <= 0) {
            printf("err\n");
            break;
        }
        printf(line);
    } while(feof(sock) == 0);

    return HTTP_SUCCESS;
}