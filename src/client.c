#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include "net/socket.h"
#include "net/http.h"
#include "net/cache.h"
#include "utils.h"

#define CLIENT_RECV_BUF_SIZE 64
bool VERBOSE = true;

int client_send_input(char *input, const socket_t *sock) {
    // Remove the trailing newline
    int len = strlen(input);
    if (len > 0 && input[len - 1] == '\n') {
        input[--len] = '\0';
    }

    for (int i = 0; i < len-1; i++) {
        if (input[i] == '\\' && input[i + 1] == 'n') {
            input[i] = '\r';
            input[i+1] = '\n';
            i++; // Skip the \n
        }
    }

    if (len > 0) {
        int sent;
        int result = socket_send_all(sock, input, len, &sent);

        if (result == SOCKET_ERR) {
            elog("Socket send failed");
            return result;
        }

        if (result == SOCKET_CLOSED) {
            elog("Socket was closed by the peer");
            return result;
        }

        printf("Sent %d bytes\n", sent);
    }

    return SOCKET_SUCCESS;
}

void *client_receive(void *arg) {
    socket_t sock = *((socket_t *)arg);
    char buf[CLIENT_RECV_BUF_SIZE];

    while (true) {
        int bytes = socket_recv_wait(&sock, buf, CLIENT_RECV_BUF_SIZE - 1);
        if (bytes == SOCKET_ERR) {
            elog("Recv failed with %s", sockcode_to_str(bytes));
            break;
        } else if (bytes == SOCKET_TIMEOUT) {
            elog("Recv timed out.");
        } else if (bytes == SOCKET_CLOSED) {
            elog("Socket closed by proxy.");
            break;
        } else {
            buf[bytes] = '\0';
            printf("[%d] %s\n", bytes, buf);
        } 
    }
    return NULL;
}

int main(void) {

    // const char *key = "einstein_quote";
    // const char *content1 = "Everything should be made as simple as";
    // const char *content2 = " possible, but not simpler.\n";
    // const char *content3 = "- Albert Einstein";

    // cache_entry_t entry;
    // bool b;
    // b = cache_put(&entry, key, content1, strlen(content1), 0);
    // printf("%d %s\n", b, entry.filepath);

    // b = cache_append(&entry, content2, strlen(content2));
    // printf("%d\n", b);

    // b = cache_append(&entry, content3, strlen(content3));
    // printf("%d\n", b);

    // cache_close_entry(&entry);

    // return 0;

    socket_t sock;
    char input[512];
    pthread_t thread_id;

    addr_t addr;
    if (!make_addr(&addr, "127.0.0.1", 5000)) { return 1; }
    if (!create_socket(&sock, &addr, SOCKET_CLIENT)) { return 1; }
    if (!socket_connect(&sock)) { return 1; }

    // Dispatch thread to receive bytes
    if (pthread_create(&thread_id, NULL, client_receive, (void *)&sock) != 0) {
        return 1;
    }
    pthread_detach(thread_id);
    
    printf("> ");
    while (fgets(input, sizeof(input), stdin) != NULL) {
        if (client_send_input(input, &sock) != SOCKET_SUCCESS) {
            break;
        }
        printf("> ");
    }
}