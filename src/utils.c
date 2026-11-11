#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include "utils.h"

int parse_int(char *s) {
    char *endptr;
    long parsed = strtol(s, &endptr, 10);

    // Check for overflow/underflow
    if ((parsed == LONG_MAX || parsed == LONG_MIN) && errno == ERANGE) {
        return -1;
    }
    
    // Check if no digits were parsed
    if (endptr == s) {
        return -1;
    }

    return (int) parsed;
}

void init_rand() {
    srand((unsigned int) time(NULL));
}

double get_rand() {
    return (double) rand() / ((double) RAND_MAX + 1.0);
}

void fill_with_shuffled_range(int *arr, int start, int end) {
    int size = end - start;
    for (int i = 0; i < size; i++) {
        arr[i] = start + i;
    }

    for (int i = size - 1; i > 0; --i) {
        int j = rand() % (i + 1);

        int tmp = arr[i];
        arr[i] = arr[j];
        arr[j] = tmp;
    }
}

void print_hexdump(const void *data, int data_len) {
    const uint8_t *bdata = (const uint8_t *)data;
    
    for (int i = 0; i < data_len; i += HEXDUMP_COL_WIDTH) {
        int row_len = (data_len - i < HEXDUMP_COL_WIDTH) ? (data_len - i) : HEXDUMP_COL_WIDTH;

        // print hex for the current row
        for (int j = 0; j < HEXDUMP_COL_WIDTH; j++) {
            if (j < row_len) {
                printf("%02x ", bdata[i + j]);
            } else {
                printf("   "); // padding for final row
            }
        }

        // column separator
        printf("    ");

        // print ascii for the current row
        for (int j = 0; j < row_len; j++) {
            uint8_t c = bdata[i + j];
            if (isprint(c)) {
                putchar(c);
            } else {
                putchar('.');
            }
        }
        
        printf("\n");
    }
}

int rstrip(char *str) {
    if (str == NULL) return 0;
    
    int len = rnstrip(str, strlen(str));
    
    // Add the new null terminator to truncate the trailing spaces
    str[len] = '\0';
    return len;
}

int rnstrip(const char *str, int len) {
    if (str == NULL) return 0;

    while (len > 0 && isspace((unsigned char) str[len - 1])) { len--; }
    return len;
}

const char* lstrip(const char *str, int *out_num_stripped) {
    int i = 0;
    while (str[i] != '\0' && isspace((unsigned char) str[i])) {
        i++;
    }
    if (out_num_stripped != NULL) {
        *out_num_stripped = i;
    }
    return str + i;
}

const char *strip(char *str, int *out_new_len) {
    int len = rstrip(str);
    int lstripped;
    const char *s = lstrip(str, &lstripped);
    if (out_new_len != NULL) {
        *out_new_len = len - lstripped;
    }
    return s;
}

bool streq(const char *s1, const char *s2) {
    if (s1 == NULL || s2 == NULL) { return false; }
    return strcmp(s1, s2) == 0;
}

bool strneq(const char *s1, const char *s2, int n) {
    if (s1 == NULL || s2 == NULL) { return false; }
    return strncmp(s1, s2, n) == 0;
}

bool streq_icase(const char *s1, const char *s2) {
    if (s1 == NULL || s2 == NULL) { return false; }
    if (s1 == s2) { return true; }

    while (*s1 && *s2) {
        if (tolower((unsigned char) *s1) != tolower((unsigned char) *s2)) {
            return false;
        }
        // move forward by 1 character
        s1++;
        s2++;
    }

    // strings match if they reached their null-terminators at the same time
    return *s1 == *s2;
}

bool strstartswith(const char *s, const char *prefix) {
    if (s == NULL || prefix == NULL) { return false; }
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

int strnfind(const char *s, char c, int n) {
    if (s == NULL || n == 0) { return -1; }
    for (int i = 0; i < n; i++) {
        if (s[i] == c) { return i; }
        else if (s[i] == '\0') { return -1; }
    }
    return -1;
}

bool is_str_allin(const char *s, size_t len, const char *chset) {
    if (s == NULL || chset == NULL) { return false; }

    for (size_t i = 0; i < len; i++) {
        // End of string
        if (s[i] == '\0') { break; }

        // Check if the current character is in chset
        if (strchr(chset, s[i]) == NULL) {
            return false;
        }
    }

    return true;
}

bool strtoint(const char *s, int *out, int *out_len, bool allow_trailing) {
    char *endptr;
    long val = strtol(s, &endptr, 10);

    if (
        (errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) // out of long range
        || (val > INT_MAX || val < INT_MIN)                       // out of int range
        || (s == endptr)                                          // no parsed digits
        || (!allow_trailing && *endptr != '\0')                   // leftover characters
    ) {
        return false;
    }

    *out_len = endptr - s;
    *out = (int)val;
    return true;
}

void linkedlist_init(linkedlist_t *list) {
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
}

void linkedlist_push(linkedlist_t *list, void *data) {
    llnode_t *node = (llnode_t *)malloc(sizeof(llnode_t));
    node->data = data;
    node->next = NULL;

    if (list->size == 0) {
        list->head = node;
    } else {
        list->tail->next = node;
    }
    list->tail = node;
    list->size++;
}

void linkedlist_free(linkedlist_t *list, bool free_data) {
    llnode_t *node = list->head;
    llnode_t *next;
    while (node != NULL) {
        next = node->next;
        if (free_data) { free(node->data); }
        free(node);
        node = next;
    }
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
}

int linkedlist_size(const linkedlist_t *list) {
    return list->size;
}

void *linkedlist_get(const linkedlist_t *list, int i) {
    if (i < 0 || i >= list->size) { return NULL; }

    llnode_t *node = list->head;
    int j = 0;
    while (j < i) {
        node = node->next;
        j++;
    }
    return node->data;
}

void linkedlist_foreach(const linkedlist_t *list, llcallback_t f) {
    if (f == NULL) { return; }

    llnode_t *node = list->head;
    int i = 0;

    while (node != NULL) {
        if (f(i, node->data)) {
            return;
        }

        node = node->next;
        i++;
    }
}

void *linkedlist_find(const linkedlist_t *list, llpredicate_t match_fn, const void *criteria) {
    if (match_fn == NULL) { return NULL; }

    llnode_t *node = list->head;
    int i = 0;
    
    while (node != NULL) {
        if (match_fn(i, node->data, criteria)) {
            return node->data;
        }

        node = node->next;
        i++;
    }

    return NULL;
}