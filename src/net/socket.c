#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "socket.h"
#include "../utils.h"

int create_socket(char *ip, uint16_t port, addr_t *out_addr) {
    // Configure server ipv4 address
    out_addr->sin_family = AF_INET;
	out_addr->sin_port = htons(port);

    // Convert IP from string & set server IP
    if (ip != NULL) {
        if (inet_pton(AF_INET, ip, &out_addr->sin_addr) <= 0) {
            elog("Error: bad server address: %s", ip);
            return -1;
        }
    } else {
        out_addr->sin_addr.s_addr = INADDR_ANY;
    }

    // Create socket
    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        pelog("Something bad happened");
        return -1;
    }

    // Bind socket to port
    if (ip == NULL) {
        if (bind(sock_fd, (struct sockaddr *)out_addr, sizeof(addr_t)) < 0) {
            pelog("Error binding socket");
            close(sock_fd);
            return -1;
        }
        vlog(
            "Socket %d created & bound to %s:%d",
            sock_fd, get_ip_str(out_addr), get_porth(out_addr)
        );
    } else {
        vlog(
            "Socket %d created for %s:%d",
            sock_fd, get_ip_str(out_addr), get_porth(out_addr)
        );
    }

    return sock_fd;
}

void close_socket(int sock_fd) {
    close(sock_fd);
    vlog("Socket %d closed", sock_fd);
}

int socket_recv_timeout(
    int sock_fd, void *buf, int len, addr_t *out_src_addr, int timeout_sec
) {
    // set up the file descriptor set
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(sock_fd, &fds);
    
    // set up the struct timeval for the timeout
    struct timeval tv;
    tv.tv_sec = timeout_sec;
    tv.tv_usec = 0;

    // wait until timeout or data received
    int n = select(sock_fd + 1, &fds, NULL, NULL, &tv);
    if (n == 0) { return SOCKET_TIMEOUT; }
    if (n == -1) { return SOCKET_ERR; }

    // data must be here, so do a normal recv()
    return socket_recv(sock_fd, buf, len, out_src_addr);
}

int socket_recv(int sock_fd, void *buf, int len, addr_t *out_src_addr) {
    socklen_t src_addrlen = sizeof(addr_t);
    return recvfrom(
        sock_fd, buf, len, 0,
        (struct sockaddr *)out_src_addr, &src_addrlen
    );
}

int socket_send(int sock_fd, const void *buf, int len, const addr_t *dst_addr) {
    return sendto(
        sock_fd, buf, len, 0,
        (const struct sockaddr *)dst_addr, sizeof(addr_t)
    );
}

void socket_recv_flush(int sock_fd) {
    char buffer[1024];
    ssize_t n;
    
    // Read and discard until the buffer is empty
    while ((n = recv(sock_fd, buffer, sizeof(buffer), MSG_DONTWAIT)) > 0) {}
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
