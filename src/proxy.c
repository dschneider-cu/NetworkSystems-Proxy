#include <stdio.h>
#include <stdlib.h>
#include "net/socket.h"
#include "net/http.h"
#include "utils.h"

bool VERBOSE = true;

httpcode_t content_callback(response_t *response, char *chunk, int chunk_len, void *ctx) {
    socket_t *client_sock = (socket_t *)ctx;

    // Forward content chunk to client
    vlog("Chunk [%d bytes] \"%.*s\"\n", chunk_len, chunk_len, chunk);
    int bytes_sent;
    int sockcode = socket_send_all(client_sock, chunk, chunk_len, &bytes_sent);
    vlog("Sent %d bytes to client", bytes_sent);
    if (sockcode != SOCKET_SUCCESS) {
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    // todo: (extra credit) parse <a> and prefetch

    return HTTP_OK;
}

void proxy_send_err(const socket_t *sock, httpcode_t code) {
    response_t resp;
    http_make_response(&resp, code, NULL, 0, NULL, 0);
    int sockcode = http_send_response(&resp, sock);
    if (sockcode != SOCKET_SUCCESS) {
        elog("Error sending response to client: %s", sockcode_to_str(sockcode));
    }
    free_response(&resp);
}

/**
 * Connects to the host in the given request and outputs a client socket to communicate with the
 * server */
httpcode_t proxy_connect_to_server(request_t *request, socket_t *out_server_sock) {
    // Parse URL in request & extract hostname
    url_t url;
    httpcode_t httpcode = http_parse_url(&url, request);
    if (httpcode != HTTP_OK) {
        elog("Failed to parse URL from request: %s", request->url);
        return httpcode;
    }
    vlog("Server hostname: %.*s", url.host_len, url.host);

    // todo: Update Host header & path in request

    // Resolve hostname into server IP address
    char *hostname = strnclone(url.host, url.host_len);
    addr_t server_addr;
    if (!resolve_hostname(hostname, url.port, &server_addr)) {
        elog("Failed to resolve hostname: %s", hostname);
        free(hostname);
        return HTTP_BAD_GATEWAY;
    }
    free(hostname);
    vlog("Resolved server IP: %s:%d", get_ip_str(&server_addr), get_porth(&server_addr));

    // Create a socket for the server
    if (!create_socket(out_server_sock, &server_addr, SOCKET_CLIENT)) {
        elog("Failed to create socket for server");
        return HTTP_BAD_GATEWAY;
    }

    // Connect to server (tcp handshake)
    if (!socket_connect(out_server_sock)) {
        elog("Failed to connect to server");
        return HTTP_BAD_GATEWAY;
    }

    return HTTP_OK;
}

void proxy_thread_main(socket_t *client_sock) {
    socket_t server_sock;
    sockreader_t client_reader;
    sockreader_t server_reader;
    request_t request;
    response_t response;
    httpcode_t httpcode;
    int sockcode;

    // todo: timeout
    // todo: connection keep-open?
    // todo: transfer chunked?

    init_request(&request);
    init_response(&response);
    init_sockreader(&client_reader, client_sock);

    // Wait for request from client
    httpcode = http_receive_request(&client_reader, &sockcode, &request);
    if (httpcode != HTTP_OK) {
        evlog("Error receiving request from client: %s %s",
            httpcode_to_str(httpcode), sockcode_to_str(sockcode));
        proxy_send_err(client_sock, httpcode);
        goto cleanup;
    }
    vlog("Received request from client:");
    if (VERBOSE) print_request(&request);

    // todo: Check blocklist

    // todo: Check cache

    // Connect to server
    httpcode = proxy_connect_to_server(&request, &server_sock);
    if (httpcode != HTTP_OK) {
        proxy_send_err(client_sock, httpcode);
        goto cleanup;
    }
    init_sockreader(&server_reader, &server_sock);

    // Forward request to server
    sockcode = http_send_request(&request, &server_sock);
    if (sockcode != SOCKET_SUCCESS) {
        elog("Failed to send request to server: %s", sockcode_to_str(sockcode));
        httpcode = sockcode == SOCKET_TIMEOUT ? HTTP_GATEWAY_TIMEOUT : HTTP_BAD_GATEWAY;
        proxy_send_err(client_sock, httpcode);
        goto cleanup;
    }
    vlog("Forwarded request to server");

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
        proxy_send_err(client_sock, httpcode);
        goto cleanup;
    }
    vlog("Received response from server:");
    if (VERBOSE) print_response(&response);

    // Forward response to client
    sockcode = http_send_response(&response, client_sock);
    if (sockcode != SOCKET_SUCCESS) {
        elog("Failed to send response to client: %s", sockcode_to_str(sockcode));
        goto cleanup;
    }
    vlog("Forwarded response to client");

    // Wait for response content from server
    httpcode = http_receive_response_content(
        &server_reader,
        &sockcode,
        &response,
        content_callback, // Forward response content to client
        client_sock
    );
    if (httpcode != HTTP_OK) {
        elog("Error receiving response content from server: %s %s",
            httpcode_to_str(httpcode), sockcode_to_str(sockcode));
        // proxy_send_err(client_sock, httpcode);
        goto cleanup;
    }
    vlog("Forwarded response content to client");

    // Clean up resources
    cleanup:
        free_request(&request);
        free_response(&response);
        close_socket(client_sock);
        free(client_sock);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: ./proxy <port>\n");
        return 1;
    }

    // Parse port from command line args
    const char *proxy_ip = "0.0.0.0";
    int proxy_port;
    if (!strtoint(argv[1], &proxy_port, NULL, true)) {
        printf("Error: port must be a number\n");
        return 1;
    }

    socket_t proxy_sock;
    socket_t client_sock;
    addr_t proxy_addr;

    // Create server socket, listen on port
    if (!make_addr(&proxy_addr, proxy_ip, (uint16_t)proxy_port)) { return 1; }
    if (!create_socket(&proxy_sock, &proxy_addr, SOCKET_SERVER)) { return 1; }

    while (true) {
        // Accept client socket
        if (!socket_accept(&proxy_sock, &client_sock)) {
            elog("Error accepting client");
            continue;
        }

        // Copy client socket struct to the heap (pass ownership to thread)
        socket_t *client_sock_copy = (socket_t *)malloc(sizeof(socket_t));
        *client_sock_copy = client_sock;

        // todo: Launch thread
        proxy_thread_main(client_sock_copy);
    }
}