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
    if (!create_socket(&server_sock, "0.0.0.0", 5000, SOCKET_SERVER)) {
        return 1;
    }
    // print_socket(&server_sock);

    
    while (true) {
        if (!socket_accept(&server_sock, &client_sock)) {
            return 1;
        }
        // print_socket(&client_sock);
        init_sockreader(&reader, &client_sock);

        // while (true) {
        //     int r = sockreader_readline(&reader);
        //     if (r != SOCKET_SUCCESS) {
        //         break;
        //     }

        //     printf("Lin: %d %.*s\n", reader.len, reader.len, reader.line);
        // }
        int sockcode;
        httpcode_t httpcode;
        request_t *request = http_receive_request(&reader, &sockcode, &httpcode);

        printf("%d %s\n", sockcode, http_code_to_str(httpcode));
        print_request(request);

        if (request != NULL) free_request(request);
        close_socket(&client_sock);
    }
}