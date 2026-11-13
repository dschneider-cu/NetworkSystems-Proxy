#include <stdio.h>
#include <stdlib.h>
#include "net/socket.h"
#include "net/http.h"
#include "utils.h"

bool VERBOSE = true;

httpcode_t content_callback(response_t *response, char *chunk, int chunk_len, void *ctx) {
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

        // Launch thread

        init_sockreader(&client_reader, &client_sock);

        // Wait for request from client
        httpcode_t httpcode = http_receive_request(&client_reader, &sockcode, &request);
        if (httpcode != HTTP_OK) {
            // make err response
            free_request(&response);
            continue;
        }

        // Check blocklist

        // Check cache

        // Forward request to server

        free_request(&request);

        // Wait for response from server
        httpcode = http_receive_response(
            &server_reader,
            &sockcode,
            &response
        );
        if (httpcode != HTTP_OK) {
            http_make_response(&response, httpcode, NULL, 0, NULL, 0);
        }

        // Forward response to client

        // Receive response content from server
        httpcode = http_receive_response_content(
            &server_reader,
            &sockcode,
            &response,
            content_callback, // Forward response content to client
            NULL // todo
        );


        free_response(&response);
        close_socket(&client_sock);
    }
}