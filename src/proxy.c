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

void proxy_send_err(const socket_t *sock, httpcode_t code, response_t *out_response) {
    http_make_response(&out_response, code, NULL, 0, NULL, 0);
    int sockcode = http_send_response(out_response, sock);
    if (sockcode != SOCKET_SUCCESS) {
        elog("Error sending response to client: %s", sockcode_to_str(sockcode));
    }
}

void proxy_thread_main(socket_t *client_sock) {
    socket_t server_sock;
    sockreader_t client_reader;
    sockreader_t server_reader;
    request_t request;
    response_t response;
    httpcode_t httpcode;
    int sockcode;

    init_sockreader(&client_reader, client_sock);

    // Wait for request from client
    httpcode = http_receive_request(&client_reader, &sockcode, &request);
    if (httpcode != HTTP_OK) {
        evlog("Error receiving request from client: %s %s",
            httpcode_to_str(httpcode), sockcode_to_str(sockcode));
        free_request(&request);
        proxy_send_err(client_sock, httpcode, &response);
        return;
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
        elog("Error receiving response from server: %s %s",
            httpcode_to_str(httpcode), sockcode_to_str(sockcode));
        free_response(&response);
        proxy_send_err(client_sock, httpcode, &response);
        return;
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
    if (httpcode != HTTP_OK) {
        elog("Error receiving response from server: %s %s",
            httpcode_to_str(httpcode), sockcode_to_str(sockcode));
        proxy_send_err(client_sock, httpcode, &response);
        return;
    }

    free_response(&response);
    close_socket(client_sock);
    free(client_sock);
}

int main() {
    printf("Hello world\n");

    socket_t proxy_sock;
    socket_t client_sock;

    // Create server socket, listen on port
    if (!create_socket(&proxy_sock, "0.0.0.0", 5000, SOCKET_SERVER)) {
        return 1;
    }

    while (true) {
        // Accept client socket
        if (!socket_accept(&proxy_sock, &client_sock)) {
            elog("Error accepting client");
            continue;
        }

        // Copy client socket struct to the heap (pass ownership to thread)
        socket_t *client_sock_copy = (socket_t *)malloc(sizeof(socket_t));
        *client_sock_copy = client_sock;

        // Launch thread
        proxy_thread_main(client_sock_copy);
    }
}