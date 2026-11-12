#include <stdio.h>
#include "net/socket.h"
#include "net/http.h"
#include "utils.h"

bool VERBOSE = true;

int main() {
    printf("Hello world\n");

    socket_t server_sock;
    socket_t client_sock;
    sockreader_t reader;
    request_t request;

    // Create server socket, listen on port
    if (!create_socket(&server_sock, "0.0.0.0", 5000, SOCKET_SERVER)) {
        return 1;
    }

    while (true) {
        // Accept client socket
        if (!socket_accept(&server_sock, &client_sock)) {
            continue;
        }
        // todo: launch thread
        init_sockreader(&reader, &client_sock);

        int sockcode;
        httpcode_t httpcode = http_receive_request(&reader, &sockcode, &request);

        printf("%d %s\n", sockcode, http_code_to_str(httpcode));
        print_request(&request);

        if (httpcode == HTTP_OK) {
            // Check cache or forward to server
            // Receive response
            // Forward response
        } else {
            // make err response
        }

        free_request(&request);
        close_socket(&client_sock);
    }
}