#ifndef MINI_H
#define MINI_H

#define MN_VERSION "0.1"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* Maximum length of a word, in bytes, discounting the terminal nul byte. */
#define MN_MAX_WORD_LEN 333

/* Error codes.
 * All functions below that return an int (except mn_contains()) return one of
 * these.
 */
enum {
   MN_OK,         /* No error. */
   MN_EWORD,      /* Attempt to add the empty string or a too long word. */
   MN_EORDER,     /* Word added out of order. */
   MN_EMAGIC,     /* Magic identifier mismatch. */
   MN_EVERSION,   /* Version mismatch. */
   MN_ECORRUPT,   /* Automaton is corrupt. */
   MN_EFREEZED,   /* Attempt to add a word to a freezed automaton. */
   MN_E2BIG,      /* Automaton has grown too large. */
   MN_EIO,        /* IO error. */
};

/* Returns a string describing an error code. */
const char *mn_strerror(int err);

/*******************************************************************************
 * Encoder
 ******************************************************************************/

enum mn_type {
   MN_STANDARD = 0,     /* Classic automaton. */
   MN_NUMBERED = 1,     /* Numbered automaton. */
};

struct mini_enc;

/* Allocates a new automaton encoder.
 * If MN_NUMBERED is chosen as the automaton type, a numbered automaton is
 * created instead of a classical one. This makes possible retrieving a word
 * given its ordinal, and retrieving a word ordinal given the word itself. On
 * the other hand, this doubles the automaton size.
 */
struct mini_enc *mn_enc_new(enum mn_type type);

/* Destructor. */
void mn_enc_free(struct mini_enc *);

/* Encodes a new word.
 * Words must be added in lexicographical order, must be sorted byte-wise,
 * must be unique, and their length must be greater than zero and not exceed
 * MN_MAX_WORD_LEN. On error, no new words should be added, and the encoder
 * should be cleared before anything else is done with it.
 */
int mn_enc_add(struct mini_enc *, const void *word, size_t len);

/* Dumps an encoded automaton.
 * The provided callback will be called several times for writing the automaton
 * to some file or memory location. It must return zero on success, non-zero on
 * error. If it returns non-zero, this function will return MN_EIO.
 *
 * The automaton is freezed after this function is called, so no new words
 * should be added afterwards, unless mn_enc_clear() is called first.
 * This function can be called multiple times, unless it returns MN_E2BIG, which
 * indicates that the encoder should be reset with mn_enc_clear() before
 * anything else is done with it.
 */
int mn_enc_dump(struct mini_enc *,
                int (*write)(void *arg, const void *data, size_t size),
                void *arg);

/* Dumps an encoded automaton to a file.
 * The provided file must be opened in binary mode, for writing. The underlying
 * file descriptor is not synced with the device, but the file handle is
 * flushed.
 */
int mn_enc_dump_file(struct mini_enc *, FILE *);

/* Clears the internal structures. After this is called, the encoder object can
 * be used again to encode a new set of words.
 */
void mn_enc_clear(struct mini_enc *);


/*******************************************************************************
 * Reader
 ******************************************************************************/

struct mini;

/* Loads an automaton.
 * The provided callback will be called several times for reading the automaton.
 * It should return zero on success, non-zero on failure. A short read must be
 * considered as an error.
 * On success, makes the provided struct pointer point to the allocated
 * automaton. On failure, makes it point to NULL.
 */
int mn_load(struct mini **,
            int (*read)(void *arg, void *buf, size_t size),
            void *arg);

/* Loads an automaton from a file.
 * The provided file must be opened in binary mode, for reading.
 */
int mn_load_file(struct mini **, FILE *);

/* Destructor. */
void mn_free(struct mini *);

/* Returns the type of an automaton. */
enum mn_type mn_type(const struct mini *);

/* Returns the number of words in an automaton.
 * If the automaton is numbered, this is a constant time operation. Otherwise,
 * this requires traversing the entire automaton.
 */
uint32_t mn_size(const struct mini *);

/* Checks if an automaton contains a word.
 * Returns 1 if so, 0 otherwise.
 */
int mn_contains(const struct mini *, const void *word, size_t len);

/* Returns the ordinal of a word.
 * If the automaton is numbered and the word is present in the automaton,
 * return a strictly positive integer indicating its position (starting at 1),
 * otherwise 0.
 */
uint32_t mn_locate(const struct mini *, const void *word, size_t len);

/* Extracts a word given its ordinal.
 * The size of the provided buffer must be at least MN_MAX_WORD_LEN + 1.
 * Returns the length of the corresponding word. If the automaton isn't numbered
 * or the provided index is invalid, adds a nul character to the provided
 * buffer, and returns 0.
 */
size_t mn_extract(const struct mini *, uint32_t pos, void *word);


/*******************************************************************************
 * Iterator
 ******************************************************************************/

/* Automaton iterator. */
struct mini_iter {
   const struct mini *fsa;                    /* Attached automaton. */
   size_t root;                               /* Root depth. */
   size_t depth;                              /* Current stack depth. */
   uint32_t positions[MN_MAX_WORD_LEN + 1];   /* Offsets stack. */
   char word[MN_MAX_WORD_LEN + 1];            /* Current word. */
};

/* Returns an iterator over an automaton, starting at the very first word.
 * The iterator is initialized in such a manner that it will iterate
 * over the whole automaton.
 * Returns 1 if there are words to iterate on, 0 otherwise.
 */
uint32_t mn_iter_init(struct mini_iter *, const struct mini *);

/* Returns an iterator over all words of an automaton that start with a given
 * prefix, the prefix itself included if it exists as a full word in the
 * automaton.
 * If there are matching words, returns the position of the first matching
 * word if the automaton is numbered, 1 if it isn't. If there are no matching
 * words, returns 0.
 */
uint32_t mn_iter_initp(struct mini_iter *, const struct mini *,
                       const void *prefix, size_t len);

/* Returns an iterator over an automaton, starting at a word, and including
 * that word if it is part of the lexicon.
 * This differs from mn_iter_initp() in that the lexicon isn't required to
 * contain at least one word starting with the provided string, and in that the
 * iterator is initialized in such a manner that it will yield all remaining
 * words in the lexicon.
 * If there are matching words, returns the position of the first matching
 * word if the automaton is numbered, 1 if it isn't. If there are no matching
 * words, returns 0.
 */
uint32_t mn_iter_inits(struct mini_iter *, const struct mini *,
                       const void *word, size_t len);

/* Returns an iterator over an automaton, starting at a given ordinal, and
 * including it. Iteration will stop after the last word of the automaton has
 * been fetched.
 * Returns the position given at argument if it is valid and the automaton is
 * numbered, otherwise 0.
 */
uint32_t mn_iter_initn(struct mini_iter *, const struct mini *, uint32_t pos);

/* Fetches the next word from an iterator.
 * If "len" is not NULL, it will be assigned the length of the returned word.
 * If there are no remaining words, "len" is set to 0 if it is not NULL, and
 * NULL is returned.
 */
const char *mn_iter_next(struct mini_iter *, size_t *len);


/*******************************************************************************
 * Debugging.
 ******************************************************************************/

/* Output formats. */
enum mn_dump_format {
   MN_DUMP_TXT,  /* One word per line. */
   MN_DUMP_TSV,  /* TSV file, one line per transition, the first one giving
                  * the field names. */
   MN_DUMP_DOT,  /* DOT file, for visualization with Graphviz. */
};

/* Writes a description of an automaton to a file. */
int mn_dump(const struct mini *, FILE *, enum mn_dump_format);

#endif
