#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "net/socket.h"
#include "net/http.h"
#include "utils.h"

bool VERBOSE = true;

int main(void) {
    const char *h[4] = {
        "Hoost",
        "foo.com",
        "Foo",
        "hello world"
    };

    request_t *r = http_make_request(
        "GET",
        "http://foo.com:8bc/",
        "HTTP/1.1",
        h, 2
    );

    print_request(r);

    url_t u;
    httpcode_t c = http_parse_url(&u, r);
    printf("\n");
    printf("%s\n", http_code_to_str(c));
    if (c == HTTP_OK) {
        printf("Host: \"%.*s\"\nPath: \"%.*s\"\nPort: %d\n", u.host_len, u.host, u.path_len, u.path, u.port);
    }
    
    free_request(r);
    free(r);

    return 0;

    socket_t sock;

    if (!create_socket(&sock, "127.0.0.1", 5000, SOCKET_CLIENT)) {
        return 1;
    }

    if (!socket_connect(&sock)) {
        return 1;
    }

    char input[4096];
    
    printf("> ");
    while (fgets(input, sizeof(input), stdin) != NULL) {
        // Remove the trailing newline
        size_t len = strlen(input);
        if (len > 0 && input[len - 1] == '\n') {
            input[--len] = '\0';
        }

        for (size_t i = 0; i < len-1; i++) {
            if (input[i] == '\\' && input[i + 1] == 'n') {
                input[i] = '\r';
                input[i+1] = '\n';
                i++; // Skip the \n
            }
        }

        if (len > 0) {
            int sent;
            int result = socket_send_all(&sock, input, len, &sent);

            if (result == SOCKET_ERR) {
                elog("Socket send failed");
                break;
            }

            if (result == SOCKET_CLOSED) {
                elog("Socket was closed by the peer");
                break;
            }

            printf("Sent %d bytes\n", sent);
        }

        printf("> ");
    }
}