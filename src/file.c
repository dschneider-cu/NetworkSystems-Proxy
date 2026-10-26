#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "file.h"
#include "utils.h"


char *get_filepath(const char *dir, const char *filename, int filename_len) {
    // Allocate memory for filename combined with the directory (+1 for the \0)
    int dir_len = strlen(dir);
    filename_len = strnlen(filename, filename_len);
    char *filepath = (char *)malloc(dir_len + filename_len + 1);
    if (filepath == NULL) {
        elog("Failed to allocate memory: %u bytes", dir_len + filename_len + 1);
        return NULL;
    }

    // Concatenate dir with filename
    memcpy(filepath, dir, dir_len);
    memcpy(filepath + dir_len, filename, filename_len);
    filepath[dir_len + filename_len] = '\0';

    return filepath;
}

FILE *open_file(const char *filepath, const char *mode) {
    if (filepath == NULL) {
        return NULL;
    }

    FILE *file = fopen(filepath, mode);
    if (file == NULL) {
        pelog("Error opening file: %s", filepath);
        return NULL;
    }
    
    return file;
}

void close_file(FILE *file) {
    fclose(file);
}

long read_file_size(FILE *file) {
    // Move to the end of the file
    if (fseek(file, 0, SEEK_END) != 0) {
        pelog("Error seeking to end of file");
        return -1;
    }

    long file_size = ftell(file);
    if (file_size < 0) {
        pelog("Error determining file size");
        return -1;
    }

    // Rewind back to the beginning to start reading
    rewind(file);
    return file_size;
}

bool read_file_content(FILE *file, uint8_t *buffer, long file_size) {
    if (file == NULL || buffer == NULL || file_size <= 0) {
        return false;
    }

    // Read the binary data into the buffer
    size_t bytes_read = fread(buffer, sizeof(uint8_t), file_size, file);
    if (ferror(file)) {
        pelog("Error reading file data");
        return false;
    }
    if (bytes_read != file_size) {
        elog("Didn't read expected number of bytes from file: %ld expected, %ld actual", file_size, bytes_read);
        return false;
    }

    return true;
}

uint8_t *read_file(const char *filepath, long *out_file_size) {
    if (filepath == NULL || out_file_size == NULL) {
        return NULL;
    }

    FILE *file = open_file(filepath, "rb");
    if (file == NULL) {
        return NULL;
    }

    long file_size = read_file_size(file);
    if (file_size <= 0) {
        fclose(file);
        return NULL;
    }

    // Allocate memory for the file content
    uint8_t* buffer = (uint8_t *)malloc(file_size);
    if (buffer == NULL) {
        elog("Failed to allocate memory: %ld bytes", file_size);
        fclose(file);
        return NULL;
    }

    if (!read_file_content(file, buffer, file_size)) {
        fclose(file);
        free(buffer);
        return NULL;
    }

    *out_file_size = file_size;
    fclose(file);
    return buffer;
}

bool create_file(const char *filepath, const void *content, size_t content_len) {
    if (filepath == NULL || (content == NULL && content_len > 0)) { return false; }

    // Open the file
    FILE *file = open_file(filepath, "wb");
    if (file == NULL) {
        pelog("Failed to open file: \"%s\"", filepath);
        return false;
    }

    // Write content to the file
    if (content_len > 0) {
        size_t bytes_written = fwrite(content, 1, content_len, file);
        if (bytes_written < content_len) {
            pelog("Failed to write all bytes to file");
            close_file(file);
            return false;
        }
    }

    close_file(file);
    return true;
}

bool delete_file(const char *filepath) {
    if (filepath == NULL) { return false; }

    // Delete the file
    bool success = true;
    if (remove(filepath) != 0) {
        pelog("Failed to delete file");
        return false;
    }

    return true;
}

char *list_files(const char *dir, int *out_len, bool incl_subdir) {
    if (out_len != NULL) { *out_len = -1; }
    if (dir == NULL) { return NULL; }

    DIR *d = opendir(dir);
    if (d == NULL) {
        pelog("Failed to open directory: \"%s\"", dir);
        return NULL;
    }

    // Buffer that holds all filenames
    char *buffer = NULL;
    // Track allocated memory capacity
    size_t buffer_cap = 0;
    // Track current bytes used
    size_t buffer_len = 0;

    // Iterate files in directory
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        // Skip "." and ".."
        if (streq(entry->d_name, ".") || streq(entry->d_name, "..")) {
            continue;
        }

        // Skip subdirectories if we want to
        if (!incl_subdir && entry->d_type != DT_REG) {
            continue;
        }

        size_t name_len = strlen(entry->d_name);
        size_t needed = name_len + 1; // +1 for \0

        // Reallocate the buffer if we dont have enough capacity
        if (buffer_len + needed > buffer_cap) {
            // Arbitrary starting capacity of 256
            size_t new_cap = buffer_cap == 0 ? 256 : buffer_cap * 2;
            while (buffer_len + needed > new_cap) {
                new_cap *= 2;
            }

            char *new_buf = (char *)realloc(buffer, new_cap);
            if (new_buf == NULL) {
                pelog("Failed to reallocate directory buffer");
                free(buffer);
                closedir(d);
                return NULL;
            }
            buffer = new_buf;
            buffer_cap = new_cap;
        }

        // Copy the string into the end of the buffer, set null terminator
        memcpy(buffer + buffer_len, entry->d_name, name_len);
        buffer[buffer_len + name_len] = '\0';
        buffer_len += needed;
    }

    closedir(d);
    
    // Directory is empty
    if (buffer_len == 0) {
        *out_len = 0;
        free(buffer);
        return NULL;
    }

    *out_len = buffer_len;
    return buffer;
}

void print_filenames(const char *buffer, int buffer_len) {
    if (buffer == NULL || buffer_len == 0) {
        printf("[ Directory is empty ]\n");
        return;
    }

    const char *p = buffer;
    const char *end = buffer + buffer_len;
    while (p < end) {
        printf("%s\n", p);

        // Move the pointer to the next string, including the \0
        p += strlen(p) + 1;
    }
}

/** Safe characters for a filename */
const char *fn_safe_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890_-.";

bool is_filename_safe(const char *name, int max_len) {
    // Check null or empty string
    if (name == NULL || name[0] == '\0') {
        return false;
    }

    size_t len = strnlen(name, max_len);

    // Verify characters
    if (!is_str_allin(name, len, fn_safe_chars)) {
        return false;
    }

    return true;
}