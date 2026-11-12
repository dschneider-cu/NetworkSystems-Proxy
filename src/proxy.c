#include <stdio.h>
#include "net/socket.h"
#include "net/http.h"
#include "utils.h"

bool VERBOSE = true;

httpcode_t callback(response_t *response, char *chunk, int chunk_len) {
    printf("Chunk [%d bytes] \"%.*s\"\n", chunk_len, chunk_len, chunk);
    return HTTP_OK;
}

int main() {
    printf("Hello world\n");

    socket_t server_sock;
    socket_t client_sock;
    sockreader_t reader;
    request_t request;
    response_t response;
    httpcode_t code;

    // Create server socket, listen on port
    if (!create_socket(&server_sock, "0.0.0.0", 5000, SOCKET_SERVER)) {
        return 1;
    }

    while (true) {
        // Accept client socket
        if (!socket_accept(&server_sock, &client_sock)) {
            continue;
        }
        // todo: launch thread
        init_sockreader(&reader, &client_sock);

        int sockcode;
        code = http_receive_response(&reader, &sockcode, &response, callback);
        if (code == HTTP_OK) {
            print_response(&response);
        } else {
            printf("Err: %d %s %d\n", code, http_code_to_str(code), sockcode);
        }


        // sockreader_readline(&reader);
        // printf("Line: \"%.*s\"\n", reader.len, reader.line);
        // code = http_parse_status_line(&response, reader.line, reader.len);
        // printf("Code: %s\nResp Code: %d %s\n", http_code_to_str(code), response.respcode, http_code_to_str(response.respcode));

        // printf("Line: \"%.*s\"\n", reader.len, reader.line);
        // sockreader_readline(&reader);
        // printf("Line: \"%.*s\"\n", reader.len, reader.line);
        // sockreader_readbytes(&reader, 10, callback, NULL);
        // printf("Leftover: \"%s\" %d\n", reader.line, reader.data_len);

        // int sockcode;
        // httpcode_t httpcode = http_receive_request(&reader, &sockcode, &request);

        // printf("%d %s\n", sockcode, http_code_to_str(httpcode));
        // print_request(&request);

        // if (httpcode == HTTP_OK) {
        //     // Check cache or forward to server
        //     // Receive response
        //     // Forward response
        // } else {
        //     // make err response
        // }

        // free_request(&request);
        close_socket(&client_sock);
    }
}