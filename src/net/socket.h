#ifndef SOCKET_H
#define SOCKET_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define SOCKET_END 0
#define SOCKET_ERR -1
#define SOCKET_TIMEOUT -2

/** Helper type alias for ipv4 address struct */
typedef struct sockaddr_in addr_t;

typedef struct {
    /** File descriptor for the socket */
    int fd;
    /** Server address (my address if I am a server) */
    addr_t addr;
    /** Whether the socket is bound to the address (i.e. true if I am a server) */
    bool is_bound;
    /** Whether the socket is listening to TCP connections */
    bool is_listening;
} socket_t;

/**
 * Creates a TCP socket and fills a socket_t structure with the socket info. Returns whether the
 * socket was successfully created.
 * 
 * @param out_sock Uninitialized socket structure to fill
 * @param ip Socket IP address string in "xxx.xxx.xxx.xxx" format.
 * @param port Socket port
 * @param bind Whether to bind the port to the socket - servers should bind the port, clients
 *      should not.
 * @return Whether the socket was successfully created
 */
bool create_socket(socket_t *out_sock, char *ip, uint16_t port, bool bind_);

/**
 * Listen for connections on the socket. Only available for TCP sockets. Socket must be bound
 * (server socket) before listening.
 * 
 * @param sock Socket
 * @param max_connections Maximum number of connections before more connections are refused
 * @return Whether the socket is successfully listening
 */
bool socket_listen(socket_t *sock, int max_connections);

/** Closes the given socket. Do not use `sock` again after closing. */
void close_socket(const socket_t *sock);

/**
 * Receive bytes from the socket, with a timeout.
 * 
 * @param sock_fd Socket file descriptor
 * @param buf Pointer to the buffer to store data
 * @param len Maximum number of bytes to receive
 * @param out_src_addr Output parameter for the address of the sender. Pass NULL to discard.
 * @param timeout_sec Timeout duration in seconds
 * @return Number of bytes received on success, `SOCKET_END` on graceful shutdown, `SOCKET_ERR` on
 *      error, and `SOCKET_TIMEOUT` on timeout.
 */
int socket_recv_timeout(
    const socket_t *sock, void *buf, int len, addr_t *out_src_addr, int timeout_sec
);

/** Receive bytes from the socket. Wrapper for `recvfrom()` */
int socket_recv(const socket_t *sock, void *buf, int len, addr_t *out_src_addr);

/** Send bytes to the socket. Wrapper for `sendto()`. */
int socket_send(const socket_t *sock, const void *buf, int len, const addr_t *dst_addr);

/** Empties the OS socket receive buffer */
void socket_recv_flush(const socket_t *sock);

/**
 * Populates an addr_t given an IP address and port.
 * 
 * @param out_addr Output parameter for the result addr_t
 * @param ip IP address in network-byte order
 * @param port Port in network-byte order
 */
void make_addr(addr_t *out_addr, uint32_t ip, uint16_t port);

/**
 * Converts a 32 bit integer IP address in network-byte order to a string. All calls to get_ip_str
 * share a buffer so make a copy if you want to store more than one IP string at the same time.
 */
const char *ip_to_str(uint32_t ip);

/** Extract the IP address from an addr_t as a 32 bit integer in network-byte order */
uint32_t get_ip(const addr_t *addr);
/** Extract the Port from an addr_t as a 16 bit integer in network-byte order */
uint16_t get_port(const addr_t *addr);
/** Extract the IP address from and addr_t and convert to host-byte order */
uint32_t get_iph(const addr_t *addr);
/** Extract the Port from an addr_t and convert it to host-byte order */
uint16_t get_porth(const addr_t *addr);
/** Extract the IP address from an addr_t and convert it to a string. */
const char *get_ip_str(const addr_t *addr);

#endif