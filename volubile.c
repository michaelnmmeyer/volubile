#line 1 "api.c"
#include <string.h>
#line 1 "api.h"
#ifndef VOLUBILE_H
#define VOLUBILE_H

#define VB_VERSION "0.1"

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/* Defined in "mini.h". See https://github.com/michaelnmmeyer/mini */
struct mini;

/* Error codes. */
enum {
   VB_OK,         /* No error. */
   VB_E2LONG,     /* Query string too long. */
   VB_EPAGE,      /* Page size too large. */
   VB_EQUTF8,     /* Query string is not valid UTF-8. */
   VB_ELUTF8,     /* Lexicon contains an invalid UTF-8 string. */
   VB_EFSA,       /* Lexicon is not a numbered automaton. */
};

/* Returns a string describing an error code. */
const char *vb_strerror(int err);

enum vb_match_mode {
   VB_AUTO,          /* Select the matching mode automatically. */
   VB_EXACT,         /* Match a literal string, no magic character. */
   VB_PREFIX,        /* Prefix matching. */
   VB_SUBSTR,        /* Substring matching. */
   VB_SUFFIX,        /* Suffix matching. */
   VB_GLOB,          /* Glob matching. */
   VB_LEVENSHTEIN,   /* Levenshtein distance. */
   VB_DAMERAU,       /* Damerau-Levenshtein distance. */
   VB_LCSUBSTR,      /* Longest common substring. */
   VB_LCSUBSEQ,      /* Longest common subsequence. */
   
   VB_MODES_NR
};

#define VB_MAX_PAGE_SIZE 30   /* Maximum allowed number of words per page. */

struct vb_query {
   const char *query;         /* Query string and its length. */
   size_t len;

   enum vb_match_mode mode;   /* Matching mode to use. */
   size_t page_size;          /* Maximum number of words to return page. */

   /* Maximum allowed edit distance. Only taken into account when the matching
    * mode is VB_LEVENSHTEIN or VB_DAMERAU.
    */
   int32_t max_dist;
   
   /* Length, in code points, of the prefix that must be shared between the
    * query word and some given word in the lexicon for the second to be
    * considered a potential match. This concerns fuzzy matching search
    * modes, at the exception of VB_LCSUBSTR.
    * Increasing this value accelerates fuzzy matching, but decreases recall.
    * 1 or 2 is fine; higher values are likely to be harmful.
    */
   size_t prefix_len;

   /* State data for paginating matching words. Must be filled with zeroes the
    * first time vb_match() is called. After a call, these values are updated
    * in such a manner that, if vb_match() is called again with this same
    * structure, it will fetch the next results page, if any remain.
    */
   struct vb_pagination {
      bool last_page;         /* Whether we just returned the last page. */

      uint32_t last_pos;      /* Position of the last seen word. */
      int32_t last_weight;    /* Lowest rank among the returned words. */

   } pagination;
};

/* Sample initializer for the above structure. */
#define VB_QUERY_INIT {                                                        \
   .query = "",                                                                \
   .mode = VB_AUTO,                                                            \
   .page_size = 10,                                                            \
   .max_dist = 3,                                                              \
   .prefix_len = 1,                                                            \
}

/* Searches a lexicon.
 * The lexicon must be a numbered automaton.
 * The provided callback function will be called for each matching word. It will
 * be passed the current matching word. On success, returns VB_OK, otherwise an
 * error code.
 */
int vb_match(const struct mini *lexicon, struct vb_query *,
             void (*handler)(void *arg, const char *token, size_t len),
             void *arg);

#endif
#line 3 "api.c"
#line 1 "priv.h"
#ifndef VB_PRIV_H
#define VB_PRIV_H

#include <uchar.h>
#line 1 "mini.h"
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
#line 7 "priv.h"

struct vb_match_ctx {
   struct vb_query *query;

   /* The query string after simplification (magic characters removed, as well
    * as leading and trailing wildcards, if applicable).
    */
   enum vb_match_mode mode;
   const char *str;
   size_t len;

   void (*handler)(void *arg, const char *token, size_t len);
   void *arg;
};

extern int (*const vb_match_funcs[VB_MODES_NR])(const struct mini *,
                                                struct vb_match_ctx *);

void vb_parse_query(struct vb_match_ctx *, char [static MN_MAX_WORD_LEN + 1]);

/* Decodes a UTF-8 string.
 * "dest" should be large enough to hold (len + 1) code points.
 * "len" must be < INT32_MAX.
 * The output string will be nul-terminated.
 * Returns the length of the decoded string on success, -1 otherwise.
 * We only do a surface check, not a thorough one that involves examining each
 * byte plus the value of the decoded code points. The goal is to detect
 * unexpected binary data and encoding errors that could easily lead to memory
 * corruptions.
 */
int32_t vb_utf8_decode(char32_t *restrict dest,
                       const char *restrict str, size_t len);

/* Length, in bytes, of the first "nr" code points of "str" if it was encoded
 * as UTF-8.
 */
size_t vb_utf8_bytes(const char32_t *str, size_t nr);

#endif
#line 4 "api.c"

const char *vb_strerror(int err)
{
   static const char *const tbl[] = {
      [VB_OK] = "no error",
      [VB_E2LONG] = "query string too long",
      [VB_EPAGE] = "page size too large",
      [VB_EQUTF8] = "query string is not valid UTF-8",
      [VB_ELUTF8] = "lexicon contains an invalid UTF-8 string",
      [VB_EFSA] = "lexicon is not a numbered automaton",
   };
   
   if (err >= 0 && (size_t)err < sizeof tbl / sizeof *tbl)
      return tbl[err];
   return "unknown error";
}

int vb_match(const struct mini *lex, struct vb_query *q,
             void (*callback)(void *arg, const char *token, size_t len),
             void *arg)
{
   if (mn_type(lex) != MN_NUMBERED)
      return VB_EFSA;

   if (q->page_size > VB_MAX_PAGE_SIZE)
      return VB_EPAGE;

   /* Technically, there can be a match if the query is longer than the longest
    * possible word that can be held in an automaton. But we must define a limit
    * because a) we use stack-allocated buffers for decoding strings, and b)
    * we cannot do fuzzy matching on very long strings (hard limit defined in
    * faconde.h). This is unlikely to be a problem anyway.
    */
   if (q->len > MN_MAX_WORD_LEN)
      return VB_E2LONG;

   if (q->page_size == 0 || q->pagination.last_pos == UINT32_MAX)
      q->pagination.last_page = true;
   if (q->pagination.last_page)
      return VB_OK;

   struct vb_match_ctx c = {
      .query = q,
      .mode = q->mode,
      .str = q->query,
      .len = q->len,
      .handler = callback,
      .arg = arg,
   };
   if (c.mode >= sizeof vb_match_funcs / sizeof *vb_match_funcs)
      c.mode = VB_AUTO;

   char buf[MN_MAX_WORD_LEN + 1];
   vb_parse_query(&c, buf);

   int ret = vb_match_funcs[c.mode](lex, &c);
   if (q->pagination.last_page)
      q->pagination.last_pos = UINT32_MAX;

   return ret;
}
#line 1 "match.c"
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#line 1 "faconde.h"
#ifndef FACONDE_H
#define FACONDE_H

#define FC_VERSION "0.1"

#include <stdint.h>
#include <stdbool.h>
#include <uchar.h>

/* Maximum allowed length of a sequence. We don't check internally that this
 * limit is respected. It could be larger, but this is unlikely to be useful.
 * With this value, the worst-case allocation is 64M.
 */
#define FC_MAX_SEQ_LEN 4096

/* Check if a string matches a glob pattern.
 * Matching is case-sensitive, and is performed over the whole string. The
 * supported syntax is as follows:
 *
 *  ?         matches a single character
 *  *         matches zero or more characters
 *  [abc]     matches any of the characters a, b, or c
 *  [^abc]    matches any character but a, b, and c
 *
 * Character classes are not supported.
 *
 * The characters `[`, `?`, and `*` are interpreted as literals when in a group.
 * The character `]`, to be included in a group, must be placed in first
 * position. The character `^`, if included in a group and intended to be
 * interpreted as a literal, must not be placed at the beginning of the group.
 * The character `]`, if not preceded by `[`, is interpreted as a literal.
 *
 * If the pattern is invalid, returns false.
 */
bool fc_glob(const char32_t *pat, const char32_t *str);


/*******************************************************************************
 * Levenshtein/Damerau distance
 ******************************************************************************/

/* Normalization strategies for Levenshtein and Damerau.
 * FC_NORM_LSEQ   Normalize by the length longest sequence.
 * FC_NORM_LALIGN Normalize by the longest alignement between the two input
 *                sequences. This is more expensive (both in terms of space and
 *                time) than FC_NORM_LSEQ, but (arguably) more accurate. For
 *                details, see Heeringa, "Measuring Dialect Pronunciation
 *                Differences using Levenshtein Distance".
 */
enum fc_norm_method {
   FC_NORM_LSEQ,
   FC_NORM_LALIGN,
};

/* Computes the absolute Levenshtein distance between two sequences. */
int32_t fc_levenshtein(const char32_t *seq1, int32_t len1,
                       const char32_t *seq2, int32_t len2);

/* Computes a normalized Levenshtein distance between two sequences. */
double fc_nlevenshtein(enum fc_norm_method method,
                       const char32_t *seq1, int32_t len1,
                       const char32_t *seq2, int32_t len2);

/* Computes the absolute Damerau distance between two sequences. */
int32_t fc_damerau(const char32_t *seq1, int32_t len1,
                   const char32_t *seq2, int32_t len2);

/* Computes a normalized Damerau distance between two sequences. */
double fc_ndamerau(enum fc_norm_method method,
                   const char32_t *seq1, int32_t len1,
                   const char32_t *seq2, int32_t len2);

/* Computes the distance between the provided sequences upto a maximum value
 * of 1. If the distance between the sequences is larger than that, a value
 * larger than 1 is returned.
 */
int32_t fc_lev_bounded1(const char32_t *seq1, int32_t len1,
                        const char32_t *seq2, int32_t len2);

/* Same as "fc_lev_bounded1()", but for distance 2. */
int32_t fc_lev_bounded2(const char32_t *seq1, int32_t len1,
                        const char32_t *seq2, int32_t len2);

/* Table of pointers to the above functions.
 * The function at index 0 is a dummy one that compares sequences for equality.
 */
extern int32_t (*const fc_lev_bounded[3])(const char32_t *, int32_t,
                                          const char32_t *, int32_t);

/* Computes the jaro distance between two sequences.
 * Contrary to the canonical implementation, this returns 0 for identity, and
 * 1 to indicate absolute difference, instead of the reverse.
 */
double fc_jaro(const char32_t *seq1, int32_t len1,
               const char32_t *seq2, int32_t len2);


/*******************************************************************************
 * Longest Common Substring and Subsequence
 ******************************************************************************/

/* Computes the length of the longest common substring between two sequences. */
int32_t fc_lcsubstr(const char32_t *seq1, int32_t len1,
                    const char32_t *seq2, int32_t len2);

/* Like fc_lcsubstr(), but also makes possible the extraction of a longest
 * common substring. If "pos" is not NULL, it is made to point to the leftmost
 * longest common substring in "seq1". If the length of the longest common
 * substring is zero, "pos", if not NULL, is made to point to the end of "seq1".
 */
int32_t fc_lcsubstr_extract(const char32_t *seq1, int32_t len1,
                            const char32_t *seq2, int32_t len2,
                            const char32_t **pos);

/* Computes the length of the longest common subsequence between two
 * sequences.
 */
int32_t fc_lcsubseq(const char32_t *seq1, int32_t len1,
                    const char32_t *seq2, int32_t len2);

/* Normalized version of fc_lcsubseq(). */
double fc_nlcsubseq(const char32_t *seq1, int32_t len1,
                    const char32_t *seq2, int32_t len2);


/*******************************************************************************
 * Memoized string metrics.
 ******************************************************************************/

enum fc_metric {
   FC_LEVENSHTEIN,
   FC_DAMERAU,
   FC_LCSUBSTR,
   FC_LCSUBSEQ,

   FC_METRIC_NR,
};

struct fc_memo {
   int32_t (*compute)(struct fc_memo *, const char32_t *, int32_t);
   void *matrix;           /* Similarity matrix. */
   int32_t mdim;           /* Matrix dimension. */
   const char32_t *seq1;   /* Reference sequence. */
   int32_t len1;           /* Length of the reference sequence. */
   char32_t *seq2;         /* Previous sequence seen. */
   int32_t len2;           /* Length of this sequence. */
   int32_t max_dist;       /* Maximum allowed distance (for Levenshtein). */
};

/* Initializer.
 * metric: the metric to use.
 * max_len: the maximum possible length of a sequence (or higher). The
 * internal matrix is never reallocated.
 * max_dist: the maximum allowed edit distance (the lower, the faster). This
 * parameter is only used if the chosen metric is Levenshtein or Damerau.
 */
void fc_memo_init(struct fc_memo *, enum fc_metric metric,
                  int32_t max_len, int32_t max_dist);

/* Destructor. */
void fc_memo_fini(struct fc_memo *);

/* Returns the chosen metric. */
enum fc_metric fc_memo_metric(const struct fc_memo *);

/* Sets the reference sequence.
 * It is not copied internally, and should then be available until either a new
 * reference sequence is set, or this object is deinitialized. The reference
 * sequence can be changed several times without deinitializing this object
 * first.
 */
void fc_memo_set_ref(struct fc_memo *, const char32_t *seq1, int32_t len1);

/* Compares the reference sequence to a new one. */
static inline int32_t fc_memo_compute(struct fc_memo *m,
                                      const char32_t *seq2, int32_t len2)
{
   return m->compute(m, seq2, len2);
}

/* Concrete prototypes for the memoized string metrics functions.
 * The function called must match the chosen metric.
 */
int32_t fc_memo_levenshtein(struct fc_memo *, const char32_t *, int32_t);
int32_t fc_memo_damerau(struct fc_memo *, const char32_t *, int32_t);
int32_t fc_memo_lcsubstr(struct fc_memo *, const char32_t *, int32_t);
int32_t fc_memo_lcsubseq(struct fc_memo *, const char32_t *, int32_t);

#endif
#line 5 "match.c"
#line 1 "heap.h"
#ifndef VB_HEAP_H
#define VB_HEAP_H

#include <stddef.h>
#include <stdlib.h>
#include <assert.h>

#define VB_HEAP_INIT(array, max_elems) {.data = array, .max = max_elems}

#define VB_HEAP_DECLARE(NAME, T, COMPAR)                                       \
struct NAME {                                                                  \
   T *data;                                                                    \
   size_t size;                                                                \
   size_t max;                                                                 \
};                                                                             \
                                                                               \
static void NAME##_push(struct NAME *heap, T item)                             \
{                                                                              \
   assert(heap->max > 0);                                                      \
   T *restrict data = heap->data;                                              \
   const size_t max = heap->max;                                               \
                                                                               \
   if (heap->size < max) {                                                     \
      size_t cur = heap->size;                                                 \
      while (cur) {                                                            \
         const size_t par = (cur - 1) >> 1;                                    \
         if (COMPAR(data[par], item) >= 0)                                     \
            break;                                                             \
         data[cur] = data[par];                                                \
         cur = par;                                                            \
      }                                                                        \
      data[cur] = item;                                                        \
      heap->size++;                                                            \
   } else if (COMPAR(item, data[0]) < 0) {                                     \
      size_t cur = 0;                                                          \
      const size_t lim = max >> 1;                                             \
      while (cur < lim) {                                                      \
         size_t child = (cur << 1) + 1;                                        \
         if (child < max - 1 && COMPAR(data[child], data[child + 1]) < 0)      \
            child++;                                                           \
         if (COMPAR(item, data[child]) >= 0)                                   \
            break;                                                             \
         data[cur] = data[child];                                              \
         cur = child;                                                          \
      }                                                                        \
      data[cur] = item;                                                        \
   }                                                                           \
}                                                                              \
                                                                               \
static int NAME##_qsort_cmp_(const void *a, const void *b)                     \
{                                                                              \
   return COMPAR(*(const T *)a, *(const T *)b);                                \
}                                                                              \
static void NAME##_finish(struct NAME *heap)                                   \
{                                                                              \
   qsort(heap->data, heap->size, sizeof *heap->data, NAME##_qsort_cmp_);       \
}

#endif
#line 8 "match.c"

static const char glob_chars[] = "*?[]";

/*******************************************************************************
 * Pattern matching.
 ******************************************************************************/

static int match_exact(const struct mini *lex, struct vb_match_ctx *c)
{
   if (mn_contains(lex, c->str, c->len))
      c->handler(c->arg, c->str, c->len);
   c->query->pagination.last_page = true;
   return VB_OK;
}

static int match_prefix(const struct mini *lex, struct vb_match_ctx *c)
{
   bool first_page;
   struct mini_iter it;
   uint32_t pos = c->query->pagination.last_pos;

   if (pos) {
      first_page = false;
      mn_iter_initn(&it, lex, pos);
   } else {
      first_page = true;
      pos = mn_iter_initp(&it, lex, c->str, c->len);
   }

   const char *term;
   size_t len;
   size_t page_size = c->query->page_size;
   while ((term = mn_iter_next(&it, &len))) {
      if (!first_page && (len < c->len || memcmp(c->str, term, c->len)))
         break;
      if (!page_size--) {
         c->query->pagination.last_pos = pos;
         return VB_OK;
      } else {
         c->handler(c->arg, term, len);
      }
      pos++;
   }
   c->query->pagination.last_page = true;
   return VB_OK;
}

static int match_substr(const struct mini *lex, struct vb_match_ctx *c)
{
   struct mini_iter it;
   uint32_t pos = c->query->pagination.last_pos;
   if (pos) {
      mn_iter_initn(&it, lex, pos);
   } else {
      pos = mn_iter_init(&it, lex);
   }

   const char *term;
   size_t len;
   size_t page_size = c->query->page_size;
   while ((term = mn_iter_next(&it, &len))) {
      if (len >= c->len && strstr(term, c->str)) {
         if (!page_size--) {
            c->query->pagination.last_pos = pos;
            return VB_OK;
         } else {
            c->handler(c->arg, term, len);
         }
      }
      pos++;
   }
   c->query->pagination.last_page = true;
   return VB_OK;
}

static int match_suffix(const struct mini *lex, struct vb_match_ctx *c)
{
   uint32_t pos = c->query->pagination.last_pos;
   struct mini_iter it;
   if (pos) {
      mn_iter_initn(&it, lex, pos);
   } else {
      pos = mn_iter_init(&it, lex);
   }

   const char *term;
   size_t len;
   size_t page_size = c->query->page_size;
   while ((term = mn_iter_next(&it, &len))) {
      if (len >= c->len && !memcmp(c->str, &term[len - c->len], c->len)) {
         if (!page_size--) {
            c->query->pagination.last_pos = pos;
            return VB_OK;
         } else {
            c->handler(c->arg, term, len);
         }
      }
      pos++;
   }
   c->query->pagination.last_page = true;
   return VB_OK;
}

static int match_glob(const struct mini *lex, struct vb_match_ctx *c)
{
   struct mini_iter it;
   uint32_t pos = c->query->pagination.last_pos;
   size_t pfx_len = strcspn(c->str, glob_chars);
   if (pos) {
      mn_iter_initn(&it, lex, pos);
   } else {
      pos = mn_iter_initp(&it, lex, c->str, pfx_len);
   }

   int ret = VB_OK;
   char32_t upat[MN_MAX_WORD_LEN + 1];
   if (vb_utf8_decode(upat, &c->str[pfx_len], c->len - pfx_len) < 0) {
      ret = VB_EQUTF8;
      goto fini;
   }

   const char *term;
   size_t len;
   size_t page_size = c->query->page_size;
   while ((term = mn_iter_next(&it, &len))) {
      if (pfx_len && (len < pfx_len || memcmp(term, c->str, pfx_len)))
         break;
      char32_t uterm[MN_MAX_WORD_LEN + 1];
      if (vb_utf8_decode(uterm, &term[pfx_len], len - pfx_len) < 0) {
         ret = VB_ELUTF8;
         goto fini;
      }
      if (fc_glob(upat, uterm)) {
         if (!page_size--) {
            c->query->pagination.last_pos = pos;
            return VB_OK;
         } else {
            c->handler(c->arg, term, len);
         }
      }
      pos++;
   }

fini:
   c->query->pagination.last_page = true;
   return ret;
}


/*******************************************************************************
 * Fuzzy matching.
 ******************************************************************************/

static int32_t levenshtein_compute(struct fc_memo *m, const char32_t *seq, int32_t len)
{
   int32_t ret = fc_memo_levenshtein(m, seq, len);
   return ret > m->max_dist ? INT32_MAX : ret;
}

static int32_t damerau_compute(struct fc_memo *m, const char32_t *seq, int32_t len)
{
   int32_t ret = fc_memo_damerau(m, seq, len);
   return ret > m->max_dist ? INT32_MAX : ret;
}

static int32_t lcsubstr_compute(struct fc_memo *m, const char32_t *seq, int32_t len)
{
   return -fc_memo_lcsubstr(m, seq, len);
}

static int32_t lcsubseq_compute(struct fc_memo *m, const char32_t *seq, int32_t len)
{
   return -2. * fc_memo_lcsubseq(m, seq, len) / (double)(m->len1 + len) * 1000.;
}

struct vb_match_infos {
   uint32_t pos;
   int32_t weight;
};

static int vb_match_infos_cmp(const struct vb_match_infos a, const struct vb_match_infos b)
{
   if (a.weight == b.weight)
      return a.pos < b.pos ? -1 : a.pos > b.pos;
   return a.weight < b.weight ? -1 : a.weight > b.weight;
}

VB_HEAP_DECLARE(vb_heap, struct vb_match_infos, vb_match_infos_cmp)

static int match_fuzzy(const struct mini *lex, struct vb_match_ctx *c)
{
   static const int metrics[] = {
      [VB_LEVENSHTEIN] = FC_LEVENSHTEIN,
      [VB_DAMERAU] = FC_DAMERAU,
      [VB_LCSUBSTR] = FC_LCSUBSTR,
      [VB_LCSUBSEQ] = FC_LCSUBSEQ,
   };

   static int32_t (*const compute_fns[])(struct fc_memo *, const char32_t *, int32_t) = {
      [FC_LEVENSHTEIN] = levenshtein_compute,
      [FC_DAMERAU] = damerau_compute,
      [FC_LCSUBSTR] = lcsubstr_compute,
      [FC_LCSUBSEQ] = lcsubseq_compute,
   };

   char32_t seq1[MN_MAX_WORD_LEN + 1];
   int32_t len1 = vb_utf8_decode(seq1, c->str, c->len);
   if (len1 < 0) {
      c->query->pagination.last_page = true;
      return VB_EQUTF8;
   }

   struct mini_iter it;
   uint32_t pos;
   if (c->query->prefix_len && !c->query->pagination.last_pos && c->mode != VB_LCSUBSTR) {
      size_t pfx_len;
      /* If the required common prefix length is longer than the reference word,
       * there could still be an exact match.
       */
      if (c->query->prefix_len > len1)
         return match_exact(lex, c);
      pfx_len = vb_utf8_bytes(seq1, c->query->prefix_len);
      pos = mn_iter_initp(&it, lex, c->str, pfx_len);
   } else {
      pos = mn_iter_init(&it, lex);
   }

   enum fc_metric metric = metrics[c->mode];
   struct fc_memo m;
   fc_memo_init(&m, metric, MN_MAX_WORD_LEN, c->query->max_dist);
   fc_memo_set_ref(&m, seq1, len1);
   m.compute = compute_fns[metric];

   const char *term;
   size_t len;
   size_t count = 0;
   bool first_page = c->query->pagination.last_pos == 0;

   struct vb_match_infos cands[VB_MAX_PAGE_SIZE];
   struct vb_heap heap = VB_HEAP_INIT(cands, c->query->page_size);
   
   struct vb_match_infos last_min = {
      .pos = c->query->pagination.last_pos,
      .weight = c->query->pagination.last_weight,
   };

   while ((term = mn_iter_next(&it, &len))) {
      char32_t seq2[MN_MAX_WORD_LEN + 1];
      int32_t len2 = vb_utf8_decode(seq2, term, len);
      if (len2 < 0) {
         c->query->pagination.last_page = true;
         return VB_ELUTF8;
      }
      struct vb_match_infos x = {
         .pos = pos,
         .weight = fc_memo_compute(&m, seq2, len2),
      };
      if (x.weight != INT32_MAX && (first_page || vb_match_infos_cmp(x, last_min) > 0)) {
         count++;
         vb_heap_push(&heap, x);
      }
      pos++;
   }

   fc_memo_fini(&m);
   vb_heap_finish(&heap);

   for (size_t i = 0; i < heap.size; i++) {
      len = mn_extract(lex, heap.data[i].pos, seq1);
      c->handler(c->arg, (const char *)seq1, len);
   }
   if (heap.size) {
      c->query->pagination.last_pos = heap.data[heap.size - 1].pos;
      c->query->pagination.last_weight = heap.data[heap.size - 1].weight;
   }
   if (count <= heap.size)
      c->query->pagination.last_page = true;
   return VB_OK;
}

int (*const vb_match_funcs[])(const struct mini *, struct vb_match_ctx *) = {
   [VB_EXACT] = match_exact,
   [VB_PREFIX] = match_prefix,
   [VB_SUBSTR] = match_substr,
   [VB_SUFFIX] = match_suffix,
   [VB_GLOB] = match_glob,
   [VB_LEVENSHTEIN] = match_fuzzy,
   [VB_DAMERAU] = match_fuzzy,
   [VB_LCSUBSTR] = match_fuzzy,
   [VB_LCSUBSEQ] = match_fuzzy,
};
#line 1 "parse.c"

#line 1 "src/parse.rl"
#include <string.h>


#line 43 "src/parse.rl"



#line 12 "src/parse.c"
static const char _vb_simplify_glob_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	3, 1, 4, 1, 5
};

static const char _vb_simplify_glob_key_offsets[] = {
	0, 4, 8, 9, 10, 11, 15, 19
};

static const char _vb_simplify_glob_trans_keys[] = {
	42, 63, 91, 93, 42, 63, 91, 93, 
	42, 42, 42, 42, 63, 91, 93, 42, 
	63, 91, 93, 42, 0
};

static const char _vb_simplify_glob_single_lengths[] = {
	4, 4, 1, 1, 1, 4, 4, 1
};

static const char _vb_simplify_glob_range_lengths[] = {
	0, 0, 0, 0, 0, 0, 0, 0
};

static const char _vb_simplify_glob_index_offsets[] = {
	0, 5, 10, 12, 14, 16, 21, 26
};

static const char _vb_simplify_glob_indicies[] = {
	1, 2, 2, 2, 0, 3, 2, 2, 
	2, 0, 4, 2, 5, 2, 6, 2, 
	8, 2, 2, 2, 7, 9, 2, 2, 
	2, 7, 10, 2, 0
};

static const char _vb_simplify_glob_trans_targs[] = {
	1, 5, 3, 2, 2, 4, 4, 6, 
	5, 7, 7
};

static const char _vb_simplify_glob_trans_actions[] = {
	1, 1, 1, 1, 0, 1, 0, 1, 
	0, 1, 0
};

static const char _vb_simplify_glob_eof_actions[] = {
	9, 9, 3, 11, 11, 11, 7, 5
};

static const int vb_simplify_glob_start = 0;

static const int vb_simplify_glob_en_main = 0;


#line 46 "src/parse.rl"

static void vb_simplify_glob(struct vb_match_ctx *c, char *buf)
{
   int cs;
   const char *p = c->str;
   const char *pe = &p[c->len];
   const char *const eof = pe;

   size_t len = 0;
   
#line 77 "src/parse.c"
	{
	cs = vb_simplify_glob_start;
	}

#line 56 "src/parse.rl"
   
#line 84 "src/parse.c"
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const char *_keys;

	if ( p == pe )
		goto _test_eof;
_resume:
	_keys = _vb_simplify_glob_trans_keys + _vb_simplify_glob_key_offsets[cs];
	_trans = _vb_simplify_glob_index_offsets[cs];

	_klen = _vb_simplify_glob_single_lengths[cs];
	if ( _klen > 0 ) {
		const char *_lower = _keys;
		const char *_mid;
		const char *_upper = _keys + _klen - 1;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + ((_upper-_lower) >> 1);
			if ( (*p) < *_mid )
				_upper = _mid - 1;
			else if ( (*p) > *_mid )
				_lower = _mid + 1;
			else {
				_trans += (unsigned int)(_mid - _keys);
				goto _match;
			}
		}
		_keys += _klen;
		_trans += _klen;
	}

	_klen = _vb_simplify_glob_range_lengths[cs];
	if ( _klen > 0 ) {
		const char *_lower = _keys;
		const char *_mid;
		const char *_upper = _keys + (_klen<<1) - 2;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + (((_upper-_lower) >> 1) & ~1);
			if ( (*p) < _mid[0] )
				_upper = _mid - 2;
			else if ( (*p) > _mid[1] )
				_lower = _mid + 2;
			else {
				_trans += (unsigned int)((_mid - _keys)>>1);
				goto _match;
			}
		}
		_trans += _klen;
	}

_match:
	_trans = _vb_simplify_glob_indicies[_trans];
	cs = _vb_simplify_glob_trans_targs[_trans];

	if ( _vb_simplify_glob_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _vb_simplify_glob_actions + _vb_simplify_glob_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 0:
#line 8 "src/parse.rl"
	{
   buf[len++] = (*p);
}
	break;
#line 162 "src/parse.c"
		}
	}

_again:
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	const char *__acts = _vb_simplify_glob_actions + _vb_simplify_glob_eof_actions[cs];
	unsigned int __nacts = (unsigned int) *__acts++;
	while ( __nacts-- > 0 ) {
		switch ( *__acts++ ) {
	case 1:
#line 16 "src/parse.rl"
	{
   c->mode = VB_PREFIX;
   c->str = buf;
   c->len = len - 1;
}
	break;
	case 2:
#line 21 "src/parse.rl"
	{
   buf[len - 1] = '\0';    /* We use strstr() in the matching function. */
   c->mode = VB_SUBSTR;
   c->str = buf + 1;
   c->len = len - 2;
}
	break;
	case 3:
#line 27 "src/parse.rl"
	{
   c->mode = VB_SUFFIX;
   c->str = buf + 1;
   c->len = len - 1;
}
	break;
	case 4:
#line 32 "src/parse.rl"
	{
   c->mode = VB_EXACT;
}
	break;
	case 5:
#line 35 "src/parse.rl"
	{
   c->mode = VB_GLOB;
   c->str = buf;
   c->len = len;
}
	break;
#line 215 "src/parse.c"
		}
	}
	}

	}

#line 57 "src/parse.rl"

   (void)vb_simplify_glob_en_main;
}

static void vb_set_match_mode(struct vb_match_ctx *c)
{
   if (c->len == 0) {
      c->mode = VB_EXACT;
      return;
   }

   switch (*c->str) {
   #define _(C, NAME)                                                          \
   case C:                                                                     \
      c->mode = VB_##NAME;                                                     \
      c->str++;                                                                \
      c->len--;                                                                \
      break;
   _('+', LCSUBSTR)
   _('@', DAMERAU)
   _('#', SUBSTR)
   #undef _
   default:
      c->mode = VB_GLOB;
      break;
   }
}

void vb_parse_query(struct vb_match_ctx *c, char *buf)
{
   if (c->mode == VB_AUTO)
      vb_set_match_mode(c);

   if (c->mode == VB_SUBSTR) { /* We use strstr() in the matching function. */
      memcpy(buf, c->str, c->len);
      buf[c->len] = '\0';
      c->str = buf;
   } else if (c->mode == VB_GLOB) {
      vb_simplify_glob(c, buf);
   }
}
#line 1 "utf8.c"
#include <assert.h>
#include <stdint.h>

static const unsigned char vb_len_table[256] = {
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
   2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
   3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
   4, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0
};
#define vb_char_len(c) vb_len_table[(unsigned char)(c)]

static char32_t vb_decode_char(const unsigned char *str, size_t clen)
{
   switch (clen) {
   case 1:
      return str[0];
   case 2:
      return ((str[0] & 0x1F) << 6) | (str[1] & 0x3F);
   case 3:
      return ((str[0] & 0x0f) << 12) | ((str[1] & 0x3f) << 6) | (str[2] & 0x3f);
   default:
      assert(clen == 4);
      return ((str[0] & 0x07) << 18) | ((str[1] & 0x3f) << 12) | ((str[2] & 0x3f) << 6) | (str[3] & 0x3f);
   }
}
#define vb_decode_char(str, len) vb_decode_char((const unsigned char *)(str), len)

int32_t vb_utf8_decode(char32_t *restrict dest,
                       const char *restrict str, size_t len)
{
   assert(len < INT32_MAX);

   int32_t ulen = 0;

   for (size_t i = 0; i < len; ) {
      size_t clen = vb_char_len(str[i]);
      if (clen == 0 || i + clen > len)
         return -1;
      dest[ulen++] = vb_decode_char(&str[i], clen);
      i += clen;
   }

   dest[ulen] = U'\0';
   return ulen;
}

size_t vb_utf8_bytes(const char32_t *str, size_t nr)
{
   size_t pfx = 0;

   for (size_t i = 0; i < nr; i++) {
      if (str[i] < 0x80)
         pfx++;
      else if (str[i] < 0x800)
         pfx += 2;
      else if (str[i] < 0x10000)
         pfx += 3;
      else
         pfx += 4;
   }
   return pfx;
}
