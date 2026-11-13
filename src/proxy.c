#include <stdio.h>
#include <stdlib.h>
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

    socket_t proxy_sock;
    socket_t client_sock;
    socket_t server_sock;
    sockreader_t client_reader;
    sockreader_t server_reader;
    request_t request;
    response_t response;
    httpcode_t httpcode;
    int sockcode;

    // Create server socket, listen on port
    if (!create_socket(&proxy_sock, "0.0.0.0", 5000, SOCKET_SERVER)) {
        return 1;
    }

    while (true) {
        // Accept client socket
        if (!socket_accept(&proxy_sock, &client_sock)) {
            continue;
        }
        // todo: launch thread
        init_sockreader(&client_reader, &client_sock);

        // Wait for request from client
        httpcode_t httpcode = http_receive_request(&client_reader, &sockcode, &request);

        printf("%d %s\n", sockcode, http_code_to_str(httpcode));
        print_request(&request);

        if (httpcode != HTTP_OK) {
            // make err response
            free_request(&request);
            continue;
        }

        // Convert request to text
        int len = http_request_text_size(&request);
        char *buf = (char *)malloc(len + 1);
        http_request_to_text(&request, buf);

        printf("\"%s\"\n", buf);

        // Check cache or forward to server
        // Receive response
        // Forward response

        free_request(&request);
        close_socket(&client_sock);
    }
}