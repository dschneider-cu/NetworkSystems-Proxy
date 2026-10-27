#include <stdio.h>
#include "net/socket.h"
#include "utils.h"

bool VERBOSE = true;

int main() {
    printf("Hello world\n");

    socket_t server_sock;
    socket_t client_sock;
    if (!create_socket(&server_sock, "0.0.0.0", 5000, SOCKET_SERVER)) {
        return 1;
    }
    print_socket(&server_sock);
    
    while (true) {
        if (!socket_accept(&server_sock, &client_sock)) {
            return 1;
        }
        print_socket(&client_sock);
    }
}