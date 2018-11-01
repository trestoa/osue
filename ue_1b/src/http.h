#ifndef HTTP_H
#define HTTP_H

typedef enum http_err {
    // Operation was successful
    HTTP_SUCCESS = 0, 

    // URL parsing failed due to invalid fomat
    HTTP_ERR_URL_FORMAT = 1,

    // An error outside the boundries of this module occoured -> consult errno()
    HTTP_ERR_INTERNAL = 2 
} http_err_t;

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



#endif
