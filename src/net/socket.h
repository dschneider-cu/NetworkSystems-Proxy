#ifndef SOCKET_H
#define SOCKET_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define SOCKET_SUCCESS  1
#define SOCKET_CLOSED   0
#define SOCKET_ERR     -1
#define SOCKET_TIMEOUT -2

#define SOCKET_MAX_TCP_CONNECTIONS 3

/** Helper type alias for ipv4 address struct */
typedef struct sockaddr_in addr_t;

typedef enum {
    SOCKET_CLIENT,
    SOCKET_SERVER
} socktype_t;

typedef struct {
    /** File descriptor for the socket */
    int fd;
    /** Socket type (SOCKET_CLIENT or SOCKET_SERVER) */
    socktype_t type;
    /** Socket address (source address if this is a server socket; destination address if this is a
     *  client socket) */
    addr_t addr;
} socket_t;

/**
 * Creates a TCP socket and fills a socket_t structure with the socket info. Returns whether the
 * socket was successfully created.
 * 
 * @param out_sock Uninitialized socket structure to fill
 * @param ip Socket IP address string in "xxx.xxx.xxx.xxx" format. "0.0.0.0" should generally be
 *      used for server sockets.
 * @param port Socket port
 * @param type Socket type (SOCK_CLIENT or SOCK_SERVER). Server sockets bind to the given IP and
 *      listens for connections.
 * @return Whether the socket was successfully created
 */
bool create_socket(socket_t *out_sock, const char *ip, uint16_t port, socktype_t type);

/**
 * Waits for TCP connections on a given server socket, and outputs a client socket when a
 * connection is accepted.
 * 
 * @param server_sock Server socket
 * @param out_client_sock Output parameter for the accepted client socket
 * @return Whether the accept succeeded
 */
bool socket_accept(const socket_t *server_sock, socket_t *out_client_sock);

/** Closes the given socket. Do not use `sock` again after closing. */
void close_socket(const socket_t *sock);

/**
 * Receive bytes from the socket, with a timeout.
 * 
 * @param sock_fd Socket file descriptor
 * @param buf Pointer to the buffer to store data
 * @param len Maximum number of bytes to receive
 * @param timeout_sec Timeout duration in seconds
 * @return Number of bytes received on success, `SOCKET_CLOSED` on graceful shutdown,
 *      `SOCKET_ERR` on error, and `SOCKET_TIMEOUT` on timeout.
 */
int socket_recv_timeout(const socket_t *sock, void *buf, int len, int timeout_sec);

/**
 * Receive bytes from the socket.
 * 
 * @param sock Pointer to the socket
 * @param buf Buffer for data to be put into
 * @param len Number of bytes to receive
 * @return Number of bytes received on success, `SOCKET_CLOSED` on graceful shutdown, or
 *      `SOCKET_ERR` on error.
 */
int socket_recv(const socket_t *sock, void *buf, int len);

/**
 * Send bytes to the socket.
 * 
 * @param sock Pointer to the socket
 * @param buf Buffer for data to send
 * @param len Number of bytes to send
 * @return Number of bytes sent on success, `SOCKET_CLOSED` on graceful shutdown, or `SOCKET_ERR`
 *      on error.
 */
int socket_send(const socket_t *sock, const void *buf, int len);

/**
 * Send bytes to the socket until all bytes are sent.
 * 
 * @param sock Pointer to the socket
 * @param buf Buffer for data to send
 * @param len Number of bytes to send
 * @param out_sent Number of bytes actually sent. On success, this is equal to `len`.
 * @return `SOCKET_SUCCESS` on success, `SOCKET_CLOSED` on graceful shutdown, or `SOCKET_ERR` on
 *      error.
 */
int socket_send_all(const socket_t *sock, const void *buf, int len, int *out_sent);

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

/** Debug function to print socket info */
void print_socket(const socket_t *sock);

#endif