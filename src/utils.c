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
    
    int len = strlen(str);
    
    // Move backwards from the end of the string
    while (len > 0 && isspace((unsigned char) str[len - 1])) { len--; }
    
    // Add the new null terminator to truncate the trailing spaces
    str[len] = '\0';
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
