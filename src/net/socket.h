#ifndef SOCKET_H
#define SOCKET_H

#include <stdint.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define SOCKET_END 0
#define SOCKET_ERR -1
#define SOCKET_TIMEOUT -2

/** Helper type alias for ipv4 address struct */
typedef struct sockaddr_in addr_t;

/**
 * Creates a socket and returns the socket file descriptor. Prints and error message and returns -1
 * if the socket can't be created.
 * 
 * @param ip Socket IP address string in "xxx.xxx.xxx.xxx" format. If this is NULL, `INADDR_ANY`
 *      will be used and the socket will bind to the port.
 * @param port Port
 * @param out_addr Output parameter for the address structure
 * @return Socket file descriptor or -1 if an error occurs.
 */
int create_socket(char *ip, uint16_t port, addr_t *out_addr);

/** Closes the given socket */
void close_socket(int sock_fd);

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
    int sock_fd, void *buf, int len, addr_t *out_src_addr, int timeout_sec
);

/** Receive bytes from the socket. Wrapper for `recvfrom()` */
int socket_recv(int sock_fd, void *buf, int len, addr_t *out_src_addr);

/** Send bytes to the socket. Wrapper for `sendto()`. */
int socket_send(int sock_fd, const void *buf, int len, const addr_t *dst_addr);

/** Empties the OS socket receive buffer */
void socket_recv_flush(int sock_fd);

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