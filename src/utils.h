#ifndef UTILS_H
#define UTILS_H

#include <string.h>
#include <errno.h>
#include <stdbool.h>

/** Smaller of a and b */
#define MIN(a, b) ((a) > (b) ? (b) : (a))
/** Larger of a nd b */
#define MAX(a, b) ((a) > (b) ? (a) : (b))


/** Printf to stdout with function name and trailing newline. */
#define log(fmt, ...) printf("[%s] " fmt "\n", __func__, ##__VA_ARGS__)
/** Printf to stderr with function name and trailing newline. */
#define elog(fmt, ...) fprintf(stderr, "[%s] " fmt "\n", __func__, ##__VA_ARGS__)
/** Printf to stderr with the current errno message. */
#define pelog(fmt, ...) elog(fmt ": %s", ##__VA_ARGS__, strerror(errno))

extern bool VERBOSE;
/** Printf to stdout if VERBOSE is true. */
#define vlog(...) do { if (VERBOSE) log(__VA_ARGS__); } while (0)
/** Printf to stderr if VERBOSE is true. */
#define evlog(...) do { if (VERBOSE) elog(__VA_ARGS__); } while (0)
/** Printf to stderr with the current errno message if VERBOSE is true */
#define pevlog(...) do { if (VERBOSE) pelog(__VA_ARGS__); } while (0)

#define HEXDUMP_COL_WIDTH 24

typedef bool (*callback_t)(int i, void *data, void *ctx);
typedef bool (*predicate_t)(int i, const void *data, const void *criteria);


// Linked list

typedef struct _Node llnode_t;
struct _Node {
    void *data;
    llnode_t *next;
};

typedef struct {
    llnode_t *head;
    llnode_t *tail;
    int size;
} linkedlist_t;

/** Initialize a new empty linked list. */
void linkedlist_init(linkedlist_t *list);

/** Push data to the linked list */
void linkedlist_push(linkedlist_t *list, void *data);

/** Returns the data at the end of the list or NULL if the list is empty */
void *linkedlist_peek(const linkedlist_t *list);

/** Pops and returns the data at the start of the list or NULL if the list is empty */
void *linkedlist_popleft(linkedlist_t *list);

/** Free linked list memory */
void linkedlist_free(linkedlist_t *list);

/**
 * Calls `free()` on each element of the linked list. Make sure to call *before*
 * `linkedlist_free()`.
 */
void linkedlist_free_each(linkedlist_t *list);

/** Returns the size of a linked list */
int linkedlist_size(const linkedlist_t *list);

/** Get the data in a linked list at index i. Returns NULL if i is out of bounds. O(n) */
void *linkedlist_get(const linkedlist_t *list, int i);

/**
 * Iterate a linked list and call f(i, data, ctx) for each element. Stops iteration if the function
 * returns true.
 */
void linkedlist_foreach(const linkedlist_t *list, callback_t f, void *ctx);

/**
 * Iterate a linked list and returns the first element where match_fn(data, critera) returns
 * true. Returns NULL if none matched the criteria.
 */
void *linkedlist_find(const linkedlist_t *list, predicate_t match_fn, const void *criteria);


// Vec (dynamic array)

typedef struct {
    int size;
    int capacity;
    void **array;
} vec_t;

/** Initialize a new vec */
void vec_init(vec_t *vec);

/** Expand the capacity of a vec. Does nothing if new capacity is less than current capacity. */
void vec_expand(vec_t *vec, int new_capacity);

/** Push data to the vec */
void vec_push(vec_t *vec, void *data);

/** Pop and return the rightmost element of the vec, or NULL if the vec is empty */
void *vec_pop(vec_t *vec);

/** Clears all elements of a vec */
void vec_clear(vec_t *vec);

/** Free vec memory */
void vec_free(vec_t *vec);

/** Call `free()` on each element of the vec. Make sure to call *before* `vec_free()`. */
void vec_free_each(vec_t *vec);

/** Returns the size of a vec */
int vec_size(const vec_t *vec);

/** Returns the capacity of a vec */
int vec_capacity(const vec_t *vec);

/** Get the data in a vec at index i. Returns NULL if i is out of bounds. */
void *vec_get(const vec_t *vec, int i);

/** Sets the data at index i to value. Does nothing if i is out of bounds. */
void vec_set(vec_t *vec, int i, void *value);

/**
 * Iterate a vec and call f(i, data, ctx) for each element. Stops iteration if the function returns
 * true.
 */
void vec_foreach(const vec_t *vec, callback_t f, void *ctx);

/**
 * Iterate a vec and returns the first element where match_fn(data, critera) returns true.
 * Returns NULL if none matched the criteria.
 */
void *vec_find(const vec_t *vec, predicate_t match_fn, const void *criteria);

/**
 * Iterate a vec and returns the index of the first element where match_fn(data, critera) returns
 * true. Returns -1 if none matched the criteria.
 */
int vec_indexof(const vec_t *vec, predicate_t match_fn, const void *criteria);

/**
 * Prints a vec as `["a", "b", ...]`. Assumes the data contained in vec are null terminated
 * `char*`s. Don't use otherwise.
 */
void print_vec(const vec_t *vec);


// String functions

/**
 * Strips trailing whitespace. Must be null-terminated. Modifies the string.
 * 
 * @param str String pointer
 * @return new length of the string
 */
int rstrip(char *str);

/**
 * Strips trailing whitespace. Does not modify the string. Considers only `len` characters.
 * 
 * @param str String pointer
 * @param len Length of the string
 * @return new length of the string
 */
int rnstrip(const char *str, int len);

/**
 * Strips leading whitespace. Does not modify the string. Returns a pointer to the new start of the
 * string and outputs the number of stripped characters.
 * 
 * @param str String pointer
 * @param out_num_stripped Output param for the number of characters stripped. Pass NULL to discard.
 * @return New pointer to the string
 */
const char* lstrip(const char *str, int *out_num_stripped);

/**
 * Strips leading whitespace. Does not modify the string. Considers only `len` characters of the
 * string. Returns a pointer to the new start of the string and outputs the number of stripped
 * characters.
 * 
 * @param str String pointer
 * @param len Length of the string
 * @param out_num_stripped Output param for the number of characters stripped. Pass NULL to discard.
 * @return New pointer to the string
 */
const char* lnstrip(const char *str, int len, int *out_num_stripped);

/**
 * Strips leading and trailing whitespace. Must be null-terminated. Modifies the string. Returns a
 * pointer to the new start of the string and outputs the new length.
 * 
 * @param str String pointer
 * @param out_new_len Output param for the new length of the string. Pass NULL to discard
 * @return New pointer to the string
 */
const char *strip(char *str, int *out_new_len);

/**
 * Strips leading and trailing whitespace. Does not modify the string. Considers only `len`
 * characters of the string. Returns a pointer to the new start of the string and outputs the new
 * length.
 * 
 * @param str String pointer
 * @param len Length of the string
 * @param out_new_len New length of the string. Pass NULL to discard
 * @return New pointer to the string 
 */
const char *nstrip(const char *str, int len, int *out_new_len);

/**
 * Returns true if s1 and s2 are equal. Both must be null terminated.
 * NULL strings are considered not equal.
 */
bool streq(const char *s1, const char *s2);

/**
 * Returns true if the first `n` characters of s1 and s2 are equal.
 * NULL strings are considered not equal.
 */
bool strneq(const char *s1, const char *s2, int n);

/**
 * Returns true if s1 and s2 are equal, ignoring case. Both must be null terminated.
 * NULL strings are considered not equal.
 */
bool streq_icase(const char *s1, const char *s2);

/** Returns true if s starts with prefix */
bool strstartswith(const char *s, const char *prefix);

/**
 * Finds the first instance of `c` in string `s`, searching at most `n` chars. Returns the index
 * of the character, or -1 if it is not found.
 */
int strnfind(const char *s, char c, int n);

/**
 * Returns true if string `s` contains only characters in `chset`.
 * Returns false if either are NULL.
 */
bool is_str_allin(const char *s, size_t len, const char *chset);

/** Converts a string to an integer. Returns false on error. */
bool strtoint(const char *s, int *out, int *out_len, bool allow_trailing);

/**
 * Converts an int into a string and writes it into `buffer`. Buffer size must be large enough to
 * hold the string. Use `intstrlen`.
 */
bool inttostr(int n, char *buffer);

/** 
 * Returns the number of characters required to convert an int into a string (null terminator not 
 * included.) 
 */
int intstrlen(int n);

/** Copies a string into the heap */
char *strclone(const char *s);

/** Copies N characters of a string into the heap */
char *strnclone(const char *s, int len);


/** Concatenates strings, returning a new heap-allocated string. */
#define strconcat(...) _strconcat_impl(__VA_ARGS__, NULL);
char *_strconcat_impl(const char *first, ...);

/**
 * Splits a string by a delimiter and returns a vec of substrings. Each substring will be a heap-
 * allocated.
 */
vec_t strsplit(const char *s, const char *delim);


// Other

/** Initialize RNG */
void init_rand();

/** Get a random double between 0.0 (inclusive) and 1.0 (exclusive) */
double get_rand();

/** Min of a and b */
int min(int a, int b);

/** Max of a and b */
int max(int a, int b);

/** Clamp val to between min and max */
int clamp(int val, int min, int max);

/** 
 * Shuffle integers between start (inclusive) and end (exclusive). `arr` should be an array with
 * at least `(end-start)*sizeof(int)` allocated. Only positive integers, pretty please.
 */
void fill_with_shuffled_range(int *arr, int start, int end);

/**
 * Print bytes of `data` as Hex and ASCII. If a character isn't valid ASCII, prints a dot. Hex will
 * be on the left and ascii on the right.
 * 
 * @param data pointer to the data
 * @param data_len number of bytes to print
 */
void print_hexdump(const void *data, int data_len);

#endif