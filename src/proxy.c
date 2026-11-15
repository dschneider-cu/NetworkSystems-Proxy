#include <stdio.h>
#include <stdlib.h>
#include "net/socket.h"
#include "net/http.h"
#include "net/blocklist.h"
#include "net/cache.h"
#include "utils.h"

// Max time to wait while receiving requests from client
#define PROXY_CLIENT_RECV_TIMEOUT_SEC -1 // wait forever
// Max time to wait while receiving responses from server
#define PROXY_SERVER_RECV_TIMEOUT_SEC 5
bool VERBOSE = true;

#define BLOCKLIST_FILE "blocklist"
vec_t blocklist; // read-only


typedef struct {
    socket_t *client_sock;
    cache_entry_t *cache_entry;
} proxy_content_callback_ctx_t;

httpcode_t content_callback(response_t *response, char *chunk, int chunk_len, void *ctx) {
    proxy_content_callback_ctx_t *pctx = (proxy_content_callback_ctx_t *)ctx;
    socket_t *client_sock = pctx->client_sock;
    cache_entry_t *cache_entry = pctx->cache_entry;

    // Forward content chunk to client
    vlog("Chunk [%d bytes] \"%.*s\"\n", chunk_len, chunk_len, chunk);
    int bytes_sent;
    int sockcode = socket_send_all(client_sock, chunk, chunk_len, &bytes_sent);
    vlog("Sent %d bytes to client", bytes_sent);
    if (sockcode != SOCKET_SUCCESS) {
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    // Append content to cache entry
    if (!cache_append(cache_entry, chunk, chunk_len)) {
        elog("Failed to append content chunk to cache");
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

char *proxy_get_cache_key(const url_t *url) {
    if (strnfind(url->path, '?', url->path_len) != -1) {
        return NULL;
    }

    char *host = strnclone(url->host, url->host_len);
    char *path = strnclone(url->path, url->path_len);
    char *key = path_join(host, path);

    free(host);
    free(path);
    return key;
}

void proxy_put_cache(
    cache_entry_t *out_entry,
    const char *cache_key,
    const char *response_text,
    int response_text_len,
    int ttl
) {
    if (cache_key == NULL) {
        vlog("Path has query parameters; skipping cache");
        return;
    }

    if (cache_put(out_entry, cache_key, response_text, response_text_len, ttl)) {
        vlog("Cache entry created: %s %s", cache_key, out_entry->filepath);
    } else {
        elog("Failed to put response in cache");
        cache_close_entry(out_entry);
    }
}

bool proxy_check_cache(socket_t *client_sock, cache_entry_t *cache_entry, const char *cache_key) {
    void *freeptr;
    int content_len;
    char *content = cache_get(cache_entry, cache_key, &content_len, &freeptr);

    if (content != NULL) {
        // Forward cached response to client
        vlog("Cache HIT: %s", cache_key);
        int sockcode = socket_send_all(client_sock, content, content_len, NULL);
        if (sockcode != SOCKET_SUCCESS) {
            elog("Failed to send response to client: %s", sockcode_to_str(sockcode));
        } else {
            vlog("Sent cached response to client");
        }
        free(freeptr);
        cache_close_entry(cache_entry);
        return true;
    } else {
        vlog("Cache MISS: %s", cache_key);
    }

    cache_close_entry(cache_entry);
    return false;
}

httpcode_t proxy_resolve_server(request_t *request, url_t *out_url, addr_t *out_server_addr) {
    // Parse URL in request & extract hostname
    httpcode_t httpcode = http_parse_url(out_url, request);
    if (httpcode != HTTP_OK) {
        elog("Failed to parse URL from request: %s", request->url);
        return httpcode;
    }
    vlog("Server hostname: %.*s", out_url->host_len, out_url->host);

    // todo: Update Host header & path in request

    // Resolve hostname into server IP address
    char *hostname = strnclone(out_url->host, out_url->host_len);
    if (!resolve_hostname(hostname, out_url->port, out_server_addr)) {
        elog("Failed to resolve hostname: %s", hostname);
        free(hostname);
        return HTTP_BAD_GATEWAY;
    }
    vlog("Resolved server IP: %s:%d", get_ip_str(out_server_addr), get_porth(out_server_addr));

    free(hostname);
    return HTTP_OK;
}

/**
 * Connects to the the given address and outputs a client socket to communicate with the server */
httpcode_t proxy_connect_to_server(const addr_t *server_addr, socket_t *out_server_sock) {
    // Create a socket for the server
    if (!create_socket(out_server_sock, server_addr, SOCKET_CLIENT)) {
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

void proxy_handle_client(socket_t *client_sock, int cache_ttl) {
    /* -- Initialization -- */

    socket_t server_sock;
    sockreader_t client_reader;
    sockreader_t server_reader;
    request_t request;
    response_t response;
    httpcode_t httpcode;
    int sockcode;
    char *hostname = NULL;
    char *request_text = NULL;
    char *response_text = NULL;
    cache_entry_t cache_entry;
    char *cache_key = NULL;

    // todo: connection keep-open?
    // todo: transfer chunked?

    init_request(&request);
    init_response(&response);
    init_sockreader(&client_reader, client_sock, PROXY_CLIENT_RECV_TIMEOUT_SEC);
    init_cache_entry(&cache_entry);

    /* -- Receive request -- */

    // Wait for request from client
    httpcode = http_receive_request(
        &client_reader,
        &sockcode,
        &request
    );
    if (httpcode != HTTP_OK) {
        evlog("Error receiving request from client: %s %s",
            httpcode_to_str(httpcode), sockcode_to_str(sockcode));
        proxy_send_err(client_sock, httpcode);
        goto cleanup;
    }
    vlog("Received request from client:");
    if (VERBOSE) print_request(&request);

    url_t url;
    addr_t server_addr;
    httpcode = proxy_resolve_server(&request, &url, &server_addr);
    if (httpcode != HTTP_OK) {
        proxy_send_err(client_sock, httpcode);
        goto cleanup;
    }
    
    // Check blocklist
    hostname = strnclone(url.host, url.host_len);
    if (blocklist_contains(&blocklist, hostname) || blocklist_contains(&blocklist, get_ip_str(&server_addr))) {
        // Host is blocked!
        vlog("Host is blocked: %s %s", hostname, get_ip_str(&server_addr));
        proxy_send_err(client_sock, HTTP_FORBIDDEN);
        goto cleanup;
    }

    // Check cache
    cache_key = proxy_get_cache_key(&url);
    if (proxy_check_cache(client_sock, &cache_entry, cache_key)) {
        // Cache hit! Done.
        goto cleanup;
    }

    /* -- Forward request to server -- */

    // Connect to server
    httpcode = proxy_connect_to_server(&server_addr, &server_sock);
    if (httpcode != HTTP_OK) {
        proxy_send_err(client_sock, httpcode);
        goto cleanup;
    }
    init_sockreader(&server_reader, &server_sock, PROXY_SERVER_RECV_TIMEOUT_SEC);

    // Make request text
    int request_text_len;
    request_text = http_make_request_text(&request, &request_text_len);

    // Forward request to server
    sockcode = socket_send_all(&server_sock, request_text, request_text_len, NULL);
    if (sockcode != SOCKET_SUCCESS) {
        elog("Failed to send request to server: %s", sockcode_to_str(sockcode));
        httpcode = sockcode == SOCKET_TIMEOUT ? HTTP_GATEWAY_TIMEOUT : HTTP_BAD_GATEWAY;
        proxy_send_err(client_sock, httpcode);
        goto cleanup;
    }
    vlog("Forwarded request to server");

    /* -- Receive response from server & forward to client -- */

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

    // Make response text
    int response_text_len;
    response_text = http_make_response_text(&response, &response_text_len);

    // Forward response to client
    sockcode = socket_send_all(client_sock, response_text, response_text_len, NULL);
    if (sockcode != SOCKET_SUCCESS) {
        elog("Failed to send response to client: %s", sockcode_to_str(sockcode));
        goto cleanup;
    }
    vlog("Forwarded response to client");

    // Put response into cache
    proxy_put_cache(&cache_entry, cache_key, response_text, response_text_len, cache_ttl);
    
    proxy_content_callback_ctx_t ctx = {
        client_sock,
        &cache_entry
    };
    
    // Wait for response content from server
    httpcode = http_receive_response_content(
        &server_reader,
        &sockcode,
        &response,
        // Forward response content to client & append response content to cache entry
        content_callback,
        &ctx
    );
    if (httpcode != HTTP_OK) {
        elog("Error receiving response content from server: %s %s",
            httpcode_to_str(httpcode), sockcode_to_str(sockcode));
        goto cleanup;
    }
    vlog("Forwarded response content to client");

    /* -- Clean up resources -- */
    cleanup:
        free_request(&request);
        free_response(&response);
        cache_close_entry(&cache_entry);
        close_socket(client_sock);
        if (hostname != NULL) free(hostname);
        if (request_text != NULL) free(request_text);
        if (response_text != NULL) free(response_text);
        if (client_sock != NULL) free(client_sock);
        if (cache_key != NULL) free(cache_key);
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

    // Read blocklist
    blocklist = read_blocklist(BLOCKLIST_FILE);

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
        proxy_handle_client(client_sock_copy, 0); // todo: ttl
    }
}