#ifndef FILE_H
#define FILE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>


/**
 * Concatenates path1 and path2, for example "foo/bar" and "file.txt" becomes "foo/bar/file.txt".
 * 
 * @param path1 Directory string.
 * @param filename File name.
 * @return Heap-allocated path string or NULL if the operation failed
 */
char *path_join(const char *path1, const char *path2);

/** Open a file at the given path */
FILE *open_file(const char *filepath, const char *mode);

/** Closes a file */
void close_file(FILE *file);

/**
 * Returns the size of the file in bytes
 * 
 * @param file Opened file pointer
 * @return Size of the file content in bytes. Returns -1 if an error occurs (does not close the
 *      file).
 */
long read_file_size(FILE *file);

/**
 * Reads the contents of a file as bytes and puts it in the given buffer.
 * 
 * @param file Opened file pointer
 * @param buffer Buffer to put the file content in. Must be at least `file_size` bytes
 * @param file_size Size of the file content in bytes
 * @return Whether the operation succeeded
 */
bool read_file_content(FILE *file, uint8_t *buffer, long file_size);

/**
 * Reads the contents of a file as bytes and returns a pointer to its contents, given the filepath.
 * 
 * @param filepath Path to the file to read
 * @param out_file_size Output parameter for the size of the file content in bytes
 * @return Pointer to a heap-allocated array of bytes, or NULL if the operation failed
 */
uint8_t *read_file(const char *filepath, long *out_file_size);

/**
 * Create a new file at the path. Creates an empty file if content_len is 0.
 * 
 * @param filepath Filepath of the new file
 * @param content File content
 * @param content_len Length of the content in bytes
 * @return Whether the file was successfully created
 */
bool create_file(const char *filepath, const void *content, int content_len);

/**
 * Write bytes to a file.
 * 
 * @param file Pointer to the file, use open_file
 * @param content File content
 * @param content_len Length of the content in bytes
 * @return Whether the file was sucessfully written to
 */
bool write_file(FILE *file, const void *content, int content_len);

/**
 * Delete a file at the given path
 * 
 * @param filepath Filepath of the file
 * @return Whether the file was successfully deleted
 */
bool delete_file(const char *filepath);

/**
 * List files in a directory.
 * 
 * @param dir Directory name (with or without trailing slash)
 * @param out_len Output parameter for the length of the returned buffer in bytes
 * @param incl_subdir Whether to include subdirectories (non-files) in the list
 * @return A pointer to a heap-allocated buffer containing the list of file names. The filenames
 *      will be stored in a large block separated by \0. If the operation fails, returns NULL and
 *      out_len is set to -1. If the directory is empty, returns NULL and out_len is set to 0.
 */
char *list_files(const char *dir, int *out_len, bool incl_subdir);

/**
 * Prints a list of filenames (contained in a block, as `list_files`) to stdout.
 * 
 * @param buffer Buffer containing a block of filenames separated by \0
 * @param buffer_len Length of the buffer in bytes
 */
void print_filenames(const char *buffer, int buffer_len);

/**
 * Returns true if a filename is "safe," meaning:
 *  1. It is not empty
 *  2. It does not exceed `max_len`
 *  3. It contains only allowed characters: A-Z, a-z, 0-9, underscore, hypen, dot
 * 
 * @param name Filename to verify
 * @param max_len Maximum length for the name
 * @return True if the name is safe
 */
bool is_filename_safe(const char *name, int max_len);

#endif