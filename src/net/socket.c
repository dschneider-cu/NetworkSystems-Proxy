#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "socket.h"
#include "../utils.h"

bool create_socket(socket_t *out_sock, const addr_t *addr, socktype_t type) {
    // Create socket (TCP)
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        pelog("Error creating socket");
        return false;
    }
    
    if (type == SOCKET_SERVER) {
        // Enable SO_REUSEADDR; prevents "Address already in use" after closing the server and
        // restarting
        int opt = 1;
        if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            pelog("Error setting socket option");
            close(sock_fd);
            return false;
        }
        
        // Bind socket to address & port
        if (bind(sock_fd, (struct sockaddr *)addr, sizeof(addr_t)) < 0) {
            pelog("Error binding socket");
            close(sock_fd);
            return false;
        }
        vlog(
            "Socket %d created & bound to %s:%d",
            sock_fd, get_ip_str(addr), get_porth(addr)
        );
        
        // Listen for client connections
        if (listen(sock_fd, SOCKET_MAX_TCP_CONNECTIONS) < 0) {
            pelog("Error listening to socket");
            close(sock_fd);
            return false;
        }
        vlog("Listening on port %d...", get_porth(addr));
    } else {
        vlog(
            "Socket %d created for %s:%d",
            sock_fd, get_ip_str(addr), get_porth(addr)
        );
    }
    
    out_sock->addr = *addr;
    out_sock->fd = sock_fd;
    out_sock->type = type;
    return true;
}

bool socket_accept(const socket_t *server_sock, socket_t *out_client_sock) {
    if (server_sock->type != SOCKET_SERVER) {
        elog("accept() can only be used on server sockets");
        return false;
    }

    socklen_t client_len = sizeof(out_client_sock->addr);
    int client_fd = accept(
        server_sock->fd,
        (struct sockaddr *)&out_client_sock->addr,
        &client_len
    );

    if (client_fd < 0) {
        pelog("Error accepting connection");
        return false;
    }

    vlog("Connection accepted: %d", client_fd);
    out_client_sock->fd = client_fd;
    out_client_sock->type = SOCKET_CLIENT;
    return true;
}

bool socket_connect(const socket_t *sock) {
    if (sock->type != SOCKET_CLIENT) {
        elog("connect() can only be used on client sockets");
        return false;
    }

    if (connect(sock->fd, (struct sockaddr *)&sock->addr, sizeof(sock->addr)) < 0) {
        pelog("Error connecting");
        return false;
    }

    return true;
}

void close_socket(const socket_t *sock) {
    close(sock->fd);
    vlog("Socket %d closed", sock->fd);
}

int socket_recv(const socket_t *sock, void *buf, int len, int timeout_sec) {
    if (timeout_sec <= 0) {
        // Wait forever
        return socket_recv_wait(sock, buf, len);
    }

    // set up the file descriptor set
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(sock->fd, &fds);
    
    // set up the struct timeval for the timeout
    struct timeval tv;
    tv.tv_sec = timeout_sec;
    tv.tv_usec = 0;

    // wait until timeout or data received
    int n = select(sock->fd + 1, &fds, NULL, NULL, &tv);
    if (n == 0) { return SOCKET_TIMEOUT; }
    if (n == -1) { return SOCKET_ERR; }

    // data must be here, so do a normal recv()
    return socket_recv_wait(sock, buf, len);
}

int socket_recv_wait(const socket_t *sock, void *buf, int len) {
    int result = recv(sock->fd, buf, len, 0);
    if (result == 0) { return SOCKET_CLOSED; }
    else if (result < 0) { return SOCKET_ERR; }
    else { return result; }
}

int socket_send(const socket_t *sock, const void *buf, int len) {
    int result = send(sock->fd, buf, len, MSG_NOSIGNAL);
    if (result == 0) { return SOCKET_CLOSED; }
    else if (result < 0) {
        pevlog("Socket error");
        return SOCKET_ERR;
    }
    else { return result; }
}

int socket_send_all(const socket_t *sock, const void *buf, int len, int *out_sent) {
    int total = 0;
    int left = len;
    int sent;

    while (left > 0) {
        sent = socket_send(sock, buf, left);
        if (sent <= 0) {
            if (out_sent != NULL) *out_sent = total;
            return sent;
        }

        total += sent;
        left -= sent;
    }

    if (out_sent != NULL) *out_sent = total;
    return SOCKET_SUCCESS;
}

void socket_recv_flush(const socket_t *sock) {
    char buffer[1024];
    ssize_t n;
    
    // Read and discard until the buffer is empty
    while ((n = recv(sock->fd, buffer, sizeof(buffer), MSG_DONTWAIT)) > 0) {}
}

void init_sockreader(sockreader_t *reader, const socket_t *sock, int timeout_sec) {
    reader->sock = sock;
    reader->timeout_sec = timeout_sec;
    reader->len = 0;
    reader->data_len = 0;
    reader->line[0] = '\0';
}

int sockreader_readbytes(
    sockreader_t *reader,
    int bytes,
    sockread_callback_t callback,
    void *ctx
) {
    if (bytes < 0) { return SOCKET_ERR; }

    int bytes_left = bytes;
    int offset = reader->len; // Consume current line
    int code = SOCKET_SUCCESS;

    while (bytes_left > 0) {
        // If the bytes in the buffer have all been processed, clear buffer and receive more
        if (offset >= reader->data_len) {
            int recv_len = min(bytes_left, SOCKET_RECV_LINE_MAX_LEN);

            code = socket_recv(reader->sock, reader->line, recv_len, reader->timeout_sec);
            if (code <= 0) { break; }

            reader->data_len = code;
            offset = 0;
        }

        // Process chunk with callback
        int chunk_len = min(reader->data_len - offset, bytes_left);
        if (callback != NULL) {
            code = callback(reader, reader->line + offset, chunk_len, ctx);
            if (code != SOCKET_SUCCESS) { break; }
        }

        offset += chunk_len;
        bytes_left -= chunk_len;
    }

    // Remove processed bytes from the buffer, move excess to the beginning
    if (offset > 0) {
        reader->data_len -= offset;
        if (reader->data_len > 0) {
            memmove(reader->line, reader->line + offset, reader->data_len);
        }
    }
    reader->line[reader->data_len] = '\0';

    return code;
}

int sockreader_readline(sockreader_t *reader) {
    // Remove the current line from the buffer and slide the next line to the beginning
    if (reader->len > 0 && reader->data_len > reader->len) {
        memmove(reader->line, reader->line + reader->len, reader->data_len - reader->len);
    }
    reader->data_len -= reader->len;
    reader->len = 0;
    reader->line[reader->data_len] = '\0';

    // Find next line in the buffer
    int nl_idx = strnfind(reader->line, '\n', reader->data_len);
    if (nl_idx != -1) {
        // Full line is in the buffer
        reader->len = nl_idx + 1;
        return SOCKET_SUCCESS;
    }

    // Line is not complete - wait for more data
    while (true) {
        if (reader->data_len >= SOCKET_RECV_LINE_MAX_LEN) {
            elog("Line exceeded maximum length");
            return SOCKET_ERR;
        }

        int received = socket_recv(
            reader->sock,
            reader->line + reader->data_len,
            SOCKET_RECV_LINE_MAX_LEN - reader->data_len,
            reader->timeout_sec
        );
        if (received <= 0) { return received; }

        int old_datalen = reader->data_len;
        reader->data_len += received;

        // Search for a newline in the received data
        nl_idx = strnfind(reader->line + old_datalen, '\n', received);
        
        if (nl_idx != -1) {
            reader->len = old_datalen + (nl_idx + 1);
            return SOCKET_SUCCESS;
        }
    }
}

bool make_addr(addr_t *out_addr, const char *ip, uint16_t port) {
    // Zero out the structure
    memset(out_addr, 0, sizeof(addr_t));

    // Configure server ipv4 address
    out_addr->sin_family = AF_INET;
	out_addr->sin_port = htons(port);
    
    // Convert IP from string & set server IP
    if (inet_pton(AF_INET, ip, &out_addr->sin_addr) <= 0) {
        elog("Error: bad IP address: %s", ip);
        return false;
    }
    
    return true;
}

bool resolve_hostname(const char *hostname, uint16_t port, addr_t *out_addr) {
    struct addrinfo hints;
    struct addrinfo *result = NULL;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(hostname, NULL, &hints, &result);

    if (status != 0) return false;

    addr_t *addr = (addr_t *)result->ai_addr;
    *out_addr = *addr;
    out_addr->sin_port = htons(port);

    freeaddrinfo(result);
    return true;
}

const char *ip_to_str(uint32_t ip) {
    static char buffer[INET_ADDRSTRLEN];
    struct in_addr addr;
    addr.s_addr = ip;

    inet_ntop(AF_INET, &addr, buffer, sizeof(buffer));
    buffer[INET_ADDRSTRLEN-1] = '\0'; // just in case
    return buffer;
}

uint32_t get_ip(const addr_t *addr) {
    return addr->sin_addr.s_addr;
}

uint16_t get_port(const addr_t *addr) {
    return addr->sin_port;
}

uint32_t get_iph(const addr_t *addr) {
    return ntohl(get_ip(addr));
}

uint16_t get_porth(const addr_t *addr) {
    return ntohs(get_port(addr));
}

const char *get_ip_str(const addr_t *addr) {
    return ip_to_str(get_ip(addr));
}

const char *sockcode_to_str(int code) {
    if (code >= 1) {
        return "SOCKET_SUCCESS";
    } else if (code == SOCKET_CLOSED) {
        return "SOCKET_CLOSED";
    } else if (code == SOCKET_ERR) {
        return "SOCKET_ERR";
    } else if (code == SOCKET_TIMEOUT) {
        return "SOCKET_TIMEOUT";
    } else {
        return "SOCKET_ERR_UNKNOWN";
    }
}

void print_socket(const socket_t *sock) {
    const char *type;
    if (sock->type == SOCKET_CLIENT) {
        type = "Client";
    } else if (sock->type == SOCKET_SERVER) {
        type = "Server";
    } else {
        type = "Unknown";
    }
    printf(
        "- Socket -\nAddress: %s:%u\nFD: %d\nType: %s\n",
        get_ip_str(&sock->addr), get_porth(&sock->addr), sock->fd, type
    );
}
