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
