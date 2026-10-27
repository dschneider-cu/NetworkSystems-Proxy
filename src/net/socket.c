#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "socket.h"
#include "../utils.h"

bool create_socket(socket_t *out_sock, char *ip, uint16_t port, bool bind_) {
    // Configure server ipv4 address
    out_sock->addr.sin_family = AF_INET;
	out_sock->addr.sin_port = htons(port);

    // Convert IP from string & set server IP
    if (inet_pton(AF_INET, ip, &out_sock->addr.sin_addr) <= 0) {
        elog("Error: bad IP address: %s", ip);
        return false;
    }

    // Create socket (TCP)
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        pelog("Error creating socket");
        return false;
    }

    // Bind socket to port
    if (bind_) {
        // Enable SO_REUSEADDR; prevents "Address already in use" after closing the server and
        // restarting
        int opt = 1;
        if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            perror("Error setting socket option");
            return false;
        }

        if (bind(sock_fd, (struct sockaddr *)&out_sock->addr, sizeof(addr_t)) < 0) {
            pelog("Error binding socket");
            close(sock_fd);
            return false;
        }
        vlog(
            "Socket %d created & bound to %s:%d",
            sock_fd, get_ip_str(&out_sock->addr), get_porth(&out_sock->addr)
        );
    } else {
        vlog(
            "Socket %d created for %s:%d",
            sock_fd, get_ip_str(&out_sock->addr), get_porth(&out_sock->addr)
        );
    }

    out_sock->fd = sock_fd;
    out_sock->is_bound = bind_;
    out_sock->is_listening = false;
    return true;
}

bool socket_listen(socket_t *sock, int max_connections) {
    if (!sock->is_bound) {
        elog("Socket must be bound before listening");
        return false;
    }
    if (listen(sock->fd, max_connections) < 0) {
        pelog("Error listening to socket");
        sock->is_listening = false;
        return false;
    }
    sock->is_listening = true;
    return true;
}

void close_socket(const socket_t *sock) {
    close(sock->fd);
    vlog("Socket %d closed", sock->fd);
}

int socket_recv_timeout(
    const socket_t *sock, void *buf, int len, addr_t *out_src_addr, int timeout_sec
) {
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
    return socket_recv(sock->fd, buf, len, out_src_addr);
}

int socket_recv(const socket_t *sock, void *buf, int len, addr_t *out_src_addr) {
    socklen_t src_addrlen = sizeof(addr_t);
    return recvfrom(
        sock->fd, buf, len, 0,
        (struct sockaddr *)out_src_addr, &src_addrlen
    );
}

int socket_send(const socket_t *sock, const void *buf, int len, const addr_t *dst_addr) {
    return sendto(
        sock->fd, buf, len, 0,
        (const struct sockaddr *)dst_addr, sizeof(addr_t)
    );
}

void socket_recv_flush(const socket_t *sock) {
    char buffer[1024];
    ssize_t n;
    
    // Read and discard until the buffer is empty
    while ((n = recv(sock->fd, buffer, sizeof(buffer), MSG_DONTWAIT)) > 0) {}
}

void make_addr(addr_t *out_addr, uint32_t ip, uint16_t port) {
    // Zero out the structure
    memset(out_addr, 0, sizeof(out_addr));
    
    out_addr->sin_family = AF_INET;
    out_addr->sin_port = port;
    out_addr->sin_addr.s_addr = ip;
}

const char *ip_to_str(uint32_t ip) {
    static char buffer[INET_ADDRSTRLEN];
    struct in_addr addr;
    addr.s_addr = ip;

    inet_ntop(AF_INET, &addr, buffer, sizeof(buffer));
    buffer[INET6_ADDRSTRLEN-1] = '\0'; // just in case
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
