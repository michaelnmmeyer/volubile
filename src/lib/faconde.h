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
