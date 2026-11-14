#ifndef SOCKET_H
#define SOCKET_H

#include <stdint.h>
#include <stdbool.h>
#include <arpa/inet.h>

#define SOCKET_SUCCESS  1
#define SOCKET_CLOSED   0
#define SOCKET_ERR     -1
#define SOCKET_TIMEOUT -2

#define SOCKET_MAX_TCP_CONNECTIONS 3
#define SOCKET_RECV_LINE_MAX_LEN   1023

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

typedef struct {
    const socket_t *sock;
    int timeout_sec;
    /** Length of the line */
    int len;
    /** Total number of data bytes in the buffer (including line len & excess) */
    int data_len;
    /**
     * Buffer containing the line text. The buffer will be null terminated but the line will *not*
     * be, as the buffer may contain parts of the next line(s). User `reader.len`.
     */
    char line[SOCKET_RECV_LINE_MAX_LEN + 1];
} sockreader_t;

/**
 * Creates a TCP socket and fills a socket_t structure with the socket info. Returns whether the
 * socket was successfully created.
 * 
 * @param out_sock Uninitialized socket structure to fill
 * @param addr Socket address. 0.0.0.0 should generally be used for server sockets.
 * @param type Socket type (SOCK_CLIENT or SOCK_SERVER). Server sockets bind to the given IP and
 *      listens for connections.
 * @return Whether the socket was successfully created
 */
bool create_socket(socket_t *out_sock, const addr_t *addr, socktype_t type);

/**
 * Waits for TCP connections on a given server socket, and outputs a client socket when a
 * connection is accepted.
 * 
 * @param server_sock Server socket
 * @param out_client_sock Output parameter for the accepted client socket
 * @return Whether the accept succeeded
 */
bool socket_accept(const socket_t *server_sock, socket_t *out_client_sock);

/**
 * Connect a client socket to the server.
 * 
 * @param sock Client socket
 * @return Whether the connect succeeded
 */
bool socket_connect(const socket_t *sock);

/** Closes the given socket. Do not use `sock` again after closing. */
void close_socket(const socket_t *sock);

/**
 * Receive bytes from the socket with a timeout. If timeout is <= 0, waits forever.
 * 
 * @param sock_fd Socket file descriptor
 * @param buf Pointer to the buffer to store data
 * @param len Maximum number of bytes to receive
 * @param timeout_sec Timeout duration in seconds
 * @return Number of bytes received on success, `SOCKET_CLOSED` on graceful shutdown,
 *      `SOCKET_ERR` on error, and `SOCKET_TIMEOUT` on timeout.
 */
int socket_recv(const socket_t *sock, void *buf, int len, int timeout_sec);

/**
 * Receive bytes from the socket with no timeout (waits forever).
 * 
 * @param sock Pointer to the socket
 * @param buf Buffer for data to be put into
 * @param len Number of bytes to receive
 * @return Number of bytes received on success, `SOCKET_CLOSED` on graceful shutdown, or
 *      `SOCKET_ERR` on error.
 */
int socket_recv_wait(const socket_t *sock, void *buf, int len);

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
 * Initialize a socket reader
 * 
 * @param reader Uninitialized reader struct
 * @param sock Socket to read
 * @param timeout_sec Seconds to wait before timeout. Pass 0 to wait forever.
 */
void init_sockreader(sockreader_t *reader, const socket_t *sock, int timeout_sec);

/**
 * Callback function to process a chunk of bytes received from a socket
 * 
 * @param reader Pointer back to the socket reader
 * @param chunk Pointer to the data chunk
 * @param chunk_len Length of the chunk in bytes
 * @param ctx Arbitrary data to pass in
 * @return Status code. Return `SOCKET_SUCCESS` to indicate no error.
 */
typedef int (*sockread_callback_t)(sockreader_t *reader, char *chunk, int chunk_len, void *ctx);

/**
 * Receive data from the socket until a certain number of bytes is received. The bytes will be
 * received in chunks and processed by the callback function.
 * 
 * @param reader Socket reader
 * @param bytes Number of bytes to receive
 * @param callback Callback function to process a chunk of bytes. If the callback does not return
 *      success, the receive will be cancelled. Pass NULL to ignore data.
 * @param ctx Arbitrary data to pass into callback
 * @return `SOCKET_SUCCESS` on success, `SOCKET_CLOSED` on graceful shutdown, or `SOCKET_ERR` on
 *      error. If the callback returns something other than `SOCKET_SUCCESS`, returns that.
 */
int sockreader_readbytes(
    sockreader_t *reader,
    int bytes,
    sockread_callback_t callback,
    void *ctx
);

/**
 * Receive data from the socket until a complete line is received. 
 * 
 * @param reader Socket reader
 * @return `SOCKET_SUCCESS` on success, `SOCKET_CLOSED` on graceful shutdown, or `SOCKET_ERR` on
 *      error.
 */
int sockreader_readline(sockreader_t *reader);

/**
 * Populates an addr_t given an IP address string and port.
 * 
 * @param out_addr Output parameter for the result addr_t
 * @param ip IPv4 address string
 * @param port Port in host-byte order
 * @return Whether the address was created (IP is valid)
 */
bool make_addr(addr_t *out_addr, const char *ip, uint16_t port);

/**
 * Resolves a hostname via DNS and outputs an `addr_t`. Outputs only the first address returned by
 * the DNS query.
 * 
 * @param hostname Domain name to resolve, eg "foo.example.com"
 * @param out_addr Output parameter for the IP address of the host
 * @return Whether the DNS query successfully returned an address
 */
bool resolve_hostname(const char *hostname, uint16_t port, addr_t *out_addr);

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

/** Converts a socket code to a string */
const char *sockcode_to_str(int code);

/** Debug function to print socket info */
void print_socket(const socket_t *sock);

#endif