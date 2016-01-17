#line 1 "glob.c"
#line 1 "api.h"
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
#line 2 "glob.c"

/* Could be refactored to remove recursion altogether. */
bool fc_glob(const char32_t *pat, const char32_t *str)
{
   for (;;) {
      switch (*pat) {
      case U'\0':
         return !*str;
         break;
      case U'?':
         if (!*str++)
            return false;
         pat++;
         break;
      case U'*':
         /* Could skip following trailing stars, don't think it's that useful.
          */
         if (!*++pat)
            return true;
         do {
            if (fc_glob(pat, str))
               return true;
         } while (*str++);
         return false;
         break;
      case U'[': {
         bool invert = false;
         pat++;
         if (*pat == U'^') {
            invert = true;
            pat++;
         }
         while ((*pat == *str) == invert) {
            if (!*pat || *pat == U']')
               return false;
            pat++;
         }
         do {
            if (!*pat++)
               return false;
         } while (*pat != U']');
         pat++;
         str++;
         break;
      }
      default:
         if (*str++ != *pat++)
            return false;
         break;
      }
   }
   return false;
}
#line 1 "mem.c"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#line 1 "mem.h"
#ifndef FC_MEM_H
#define FC_MEM_H

#include <stdlib.h>
#include <stdarg.h>
#include <stdnoreturn.h>

noreturn void fc_fatal(const char *msg, ...);

void *fc_malloc(size_t size)
#ifdef ___GNUC__
   __attribute__((malloc))
#endif
   ;

#define fc_free free

#endif
#line 5 "mem.c"

noreturn void fc_fatal(const char *msg, ...)
{
   va_list ap;

   fputs("faconde: ", stderr);
   va_start(ap, msg);
   vfprintf(stderr, msg, ap);
   va_end(ap);
   putc('\n', stderr);
   fflush(stderr);

   abort();
}

void *fc_malloc(size_t size)
{
   assert(size);
   void *mem = malloc(size);
   if (!mem)
      fc_fatal("out of memory");
   return mem;
}
#line 1 "metric.c"
#include <limits.h>
#include <string.h>
#line 1 "macro.h"
#ifndef FC_MACRO_H
#define FC_MACRO_H

#define FC_ARRAY_SIZE(a) (sizeof(a) / sizeof (a)[0])

#define FC_MIN(a, b) ((a) < (b) ? (a) : (b))
#define FC_MIN3(a, b, c) FC_MIN(a, FC_MIN(b, c))

#define FC_MAX(a, b) ((a) > (b) ? (a) : (b))
#define FC_MAX3(a, b, c) FC_MAX(a, FC_MAX(b, c))

#define FC_SWAP(T, a, b) do {                                                  \
   T tmp = a;                                                                  \
   a = b;                                                                      \
   b = tmp;                                                                    \
} while (0)

/* a, b, c = b, c, a */
#define FC_SWAP3(T, a, b, c) do {                                              \
   T tmp = a;                                                                  \
   a = b;                                                                      \
   b = c;                                                                      \
   c = tmp;                                                                    \
} while (0)

#endif
#line 6 "metric.c"

/* Default length of a column in a matrix of edit operations.
 * If one of the sequences to compare is longer than this, or if
 * the edit operations matrix doesn't fit, memory is allocated dynamically.
 */
#ifdef NDEBUG
   #define FC_DEFAULT_COLUMN_LEN 256
#else
   #define FC_DEFAULT_COLUMN_LEN 1
#endif

/* Strip common prefixes and suffixes of two sequences.
 * "seq1" must be longer than "seq2", or have the same length than "seq2",
 * for this to be valid.
 */
#define STRIP(seq1, seq2, len1, len2) do {                                     \
   assert(len1 >= len2);                                                       \
   while (len2 && *seq1 == *seq2) {                                            \
      seq1++;                                                                  \
      seq2++;                                                                  \
      len1--;                                                                  \
      len2--;                                                                  \
   }                                                                           \
   while (len2 && seq1[len1 - 1] == seq2[len2 - 1]) {                          \
      len1--;                                                                  \
      len2--;                                                                  \
   }                                                                           \
} while (0)

#define TRANSPOSED(seq1, seq2, i, j)                                           \
   (i > 1 && j > 1 && seq1[i - 2] == seq2[j - 1] && seq1[i - 1] == seq2[j - 2])

#define IN_RANGE(len) (len >= 0 && len <= FC_MAX_SEQ_LEN)

static_assert(sizeof(size_t) >= sizeof(int32_t), "");

/*******************************************************************************
 * Absolute Levenshtein distance
 ******************************************************************************/

/* Like fc_levenshtein() but with a provided buffer for holding the matrix.
 * "seq1" is also expected to be longer than "seq2", or have the same length.
 * The provided buffer should be big enough to hold len2 + 1 int32_ts.
 */
static int32_t fc_levenshtein0(int32_t *column, const char32_t *seq1, int32_t len1,
                           const char32_t *seq2, int32_t len2)
{
   assert(IN_RANGE(len1) && IN_RANGE(len2) && len1 >= len2);

   STRIP(seq1, seq2, len1, len2);

   if (len2 == 0)
      return len1;

   for (int32_t j = 1 ; j <= len2; j++)
      column[j] = j;

   for (int32_t i = 1 ; i <= len1; i++) {
      *column = i;
      int32_t last = i - 1;

      for (int32_t j = 1; j <= len2; j++) {
         const int32_t old = column[j];
         if (seq1[i - 1] == seq2[j - 1])
            column[j] = last;
         else {
            const int32_t ic = column[j - 1] + 1;
            const int32_t dc = column[j] + 1;
            const int32_t rc = last + 1;
            column[j] = FC_MIN3(ic, dc, rc);
         }
         last = old;
      }
   }

   return column[len2];
}

int32_t fc_levenshtein(const char32_t *seq1, int32_t len1,
                   const char32_t *seq2, int32_t len2)
{
   assert(IN_RANGE(len1) && IN_RANGE(len2));

   if (len1 < len2) {
      FC_SWAP(const char32_t *, seq1, seq2);
      FC_SWAP(int32_t, len1, len2);
   }

   int32_t column[FC_DEFAULT_COLUMN_LEN], *columnp = column;

   if (len2 >= (int32_t)FC_ARRAY_SIZE(column))
      columnp = fc_malloc((len2 + 1) * sizeof *columnp);

   int32_t dist = fc_levenshtein0(columnp, seq1, len1, seq2, len2);

   if (columnp != column)
      fc_free(columnp);

   return dist;
}


/*******************************************************************************
 * Normalized Levenshtein distance
 ******************************************************************************/

/* Like fc_nlevenshtein() but with a provided buffer for holding the matrix.
 * "seq1" is also expected to be longer than "seq2", or have the same length.
 * The provided buffer should be big enough to hold len2 + 1 doubles.
 */
static double fc_nlevenshtein0(int32_t *column, enum fc_norm_method method,
                               const char32_t *seq1, int32_t len1,
                               const char32_t *seq2, int32_t len2)
{
   assert(IN_RANGE(len1) && IN_RANGE(len2) && len1 >= len2);

   if (len2 == 0)
      return len1 == 0 ? 0.0 : 1.0;

   if (method == FC_NORM_LSEQ)
      return fc_levenshtein0(column, seq1, len1, seq2, len2) / (double)len1;

   assert(method == FC_NORM_LALIGN);

   int32_t *length = &column[len2 + 1];

   for (int32_t j = 1 ; j <= len2; j++)
      column[j] = length[j] = j;

   for (int32_t i = 1 ; i <= len1; i++) {
      *column = *length = i;
      int32_t last = i - 1, llast = i - 1;

      for (int32_t j = 1; j <= len2; j++) {

         const int32_t old = column[j];
         const int32_t ic = column[j - 1] + 1;
         const int32_t dc = column[j] + 1;
         const int32_t rc = last + (seq1[i - 1] != seq2[j - 1]);
         column[j] = FC_MIN3(ic, dc, rc);
         last = old;

         const int32_t lold = length[j];
         const int32_t lic = ic == column[j] ? length[j - 1] + 1 : 0;
         const int32_t ldc = dc == column[j] ? length[j] + 1 : 0;
         const int32_t lrc = rc == column[j] ? llast + 1 : 0;
         length[j] = FC_MAX3(lic, ldc, lrc);
         llast = lold;
      }
   }

   return column[len2] / (double)length[len2];
}

double fc_nlevenshtein(enum fc_norm_method method,
                       const char32_t *seq1, int32_t len1,
                       const char32_t *seq2, int32_t len2)
{
   assert(IN_RANGE(len1) && IN_RANGE(len2));

   if (len1 < len2) {
      FC_SWAP(const char32_t *, seq1, seq2);
      FC_SWAP(int32_t, len1, len2);
   }

   int32_t column[FC_DEFAULT_COLUMN_LEN * 2], *columnp = column;

   if (2 * (len2 + 1) > (int32_t)FC_ARRAY_SIZE(column))
      columnp = fc_malloc(2 * (len2 + 1) * sizeof *columnp);

   double dist = fc_nlevenshtein0(columnp, method, seq1, len1, seq2, len2);

   if (columnp != column)
      fc_free(columnp);

   return dist;
}

/*******************************************************************************
 * Absolute Damerau distance
 ******************************************************************************/

/* Like fc_damerau() but with a provided buffer for holding the matrix.
 * "seq1" is also expected to be longer than "seq2", or have the same length.
 * The provided buffer should be big enough to hold 3 * (len2 + 1) int32_ts.
 */
static int32_t fc_damerau0(int32_t *matrix, const char32_t *seq1, int32_t len1,
                       const char32_t *seq2, int32_t len2)
{
   assert(IN_RANGE(len1) && IN_RANGE(len2) && len1 >= len2);

   STRIP(seq1, seq2, len1, len2);

   if (len2 == 0)
      return len1;

   int32_t *transpos = matrix;
   int32_t *previous = &transpos[len2 + 1];
   int32_t *current = &previous[len2 + 1];

   for (int32_t j = 0 ; j <= len2; j++)
      previous[j] = j;

   for (int32_t i = 1 ; i <= len1; i++) {
      current[0] = i;

      for (int32_t j = 1; j <= len2; j++) {
         if (seq1[i - 1] == seq2[j - 1]) {
            current[j] = previous[j - 1];
         } else {
            const int32_t ic = current[j - 1] + 1;
            const int32_t dc = previous[j] + 1;
            const int32_t rc = previous[j - 1] + 1;
            current[j] = FC_MIN3(ic, dc, rc);

            if (TRANSPOSED(seq1, seq2, i, j)) {
               const int32_t tc = transpos[j - 2] + 1;
               current[j] = FC_MIN(tc, current[j]);
            }
         }
      }
      FC_SWAP3(int32_t *, transpos, previous, current);
   }

   return previous[len2];
}

int32_t fc_damerau(const char32_t *seq1, int32_t len1,
                   const char32_t *seq2, int32_t len2)
{
   assert(IN_RANGE(len1) && IN_RANGE(len2));

   if (len1 < len2) {
      FC_SWAP(const char32_t *, seq1, seq2);
      FC_SWAP(int32_t, len1, len2);
   }

   int32_t column[FC_DEFAULT_COLUMN_LEN * 3], *columnp = column;

   if (3 * (len2 + 1) > (int32_t)FC_ARRAY_SIZE(column))
      columnp = fc_malloc(3 * (len2 + 1) * sizeof *columnp);

   int32_t dist = fc_damerau0(columnp, seq1, len1, seq2, len2);

   if (columnp != column)
      fc_free(columnp);

   return dist;
}


/*******************************************************************************
 * Normalized Damerau distance
 ******************************************************************************/

/* Like fc_ndamerau() but with a provided buffer for holding the matrix.
 * "seq1" is also expected to be longer than "seq2", or have the same length.
 * The provided buffer should be big enough to hold 6 * (len2 + 1) items.
 */
static double fc_ndamerau0(int32_t *matrix, enum fc_norm_method method,
                           const char32_t *seq1, int32_t len1,
                           const char32_t *seq2, int32_t len2)
{
   assert(IN_RANGE(len1) && IN_RANGE(len2) && len1 >= len2);

   if (len2 == 0)
      return len1 == 0 ? 0.0 : 1.0;

   if (method == FC_NORM_LSEQ)
      return (double)fc_damerau0(matrix, seq1, len1, seq2, len2) / len1;

   assert(method == FC_NORM_LALIGN);

   int32_t *ltranspos = matrix;
   int32_t *lprevious = &ltranspos[len2 + 1];
   int32_t *lcurrent = &lprevious[len2 + 1];
   int32_t *transpos = &lcurrent[len2 + 1];
   int32_t *previous = &transpos[len2 + 1];
   int32_t *current = &previous[len2 + 1];

   for (int32_t j = 0 ; j <= len2; j++)
      previous[j] = lprevious[j] = j;

   for (int32_t i = 1 ; i <= len1; i++) {
      *current = *lcurrent = i;

      for (int32_t j = 1; j <= len2; j++) {
         const bool transposed = TRANSPOSED(seq1, seq2, i, j);

         const int32_t ic = current[j - 1] + 1;
         const int32_t dc = previous[j] + 1;
         const int32_t rc = previous[j - 1] + (seq1[i - 1] != seq2[j - 1]);
         current[j] = FC_MIN3(ic, dc, rc);

         int32_t tc;
         if (transposed) {
            tc = transpos[j - 2] + 1;
            current[j] = FC_MIN(current[j], tc);
         }

         const int32_t lic = ic == current[j] ? lcurrent[j - 1] + 1 : 0;
         const int32_t ldc = dc == current[j] ? lprevious[j] + 1 : 0;
         const int32_t lrc = rc == current[j] ? lprevious[j - 1] + 1 : 0;
         lcurrent[j] = FC_MAX3(lic, ldc, lrc);

         if (transposed) {
            const int32_t ltc = tc == current[j] ? ltranspos[j - 2] + 1 : 0;
            lcurrent[j] = FC_MAX(lcurrent[j], ltc);
         }
      }

      FC_SWAP3(int32_t *, transpos, previous, current);
      FC_SWAP3(int32_t *, ltranspos, lprevious, lcurrent);
   }

   return previous[len2] / (double)lprevious[len2];
}

double fc_ndamerau(enum fc_norm_method method,
                   const char32_t *seq1, int32_t len1,
                   const char32_t *seq2, int32_t len2)
{
   assert(IN_RANGE(len1) && IN_RANGE(len2));

   if (len1 < len2) {
      FC_SWAP(const char32_t *, seq1, seq2);
      FC_SWAP(int32_t, len1, len2);
   }

   int32_t column[FC_DEFAULT_COLUMN_LEN * 6], *columnp = column;
   if (6 * (len2 + 1) > (int32_t)FC_ARRAY_SIZE(column))
      columnp = fc_malloc(6 * (len2 + 1) * sizeof *columnp);

   double dist = fc_ndamerau0(columnp, method, seq1, len1, seq2, len2);

   if (columnp != column)
      fc_free(columnp);

   return dist;
}


/*******************************************************************************
 * Bounded Levenshtein distance computation
 ******************************************************************************/

static int32_t fc_lev_bounded0(const char32_t *seq1, int32_t len1,
                           const char32_t *seq2, int32_t len2)
{
   assert(IN_RANGE(len1) && IN_RANGE(len2));

   if (len1 == len2)
      return memcmp(seq1, seq2, len1 * sizeof *seq1) != 0;
   return INT32_MAX;
}

int32_t fc_lev_bounded1(const char32_t *seq1, int32_t len1,
                    const char32_t *seq2, int32_t len2)
{
   assert(IN_RANGE(len1) && IN_RANGE(len2));

   if (len1 < len2) {
      FC_SWAP(int32_t, len1, len2);
      FC_SWAP(const char32_t *, seq1, seq2);
   }

   STRIP(seq1, seq2, len1, len2);
   return len1;
}

/* C adaptation of:
 * http://writingarchives.sakura.ne.jp/fastcomp/#algorithm
 * This is both efficient and cheap in implementation complexity.
 * i, d, r -> insert, delete, replace.
 */
int32_t fc_lev_bounded2(const char32_t *seq1, int32_t len1,
                    const char32_t *seq2, int32_t len2)
{
   assert(IN_RANGE(len1) && IN_RANGE(len2));

   static const char *const models[3][4] = {
      {"id", "di", "rr", NULL},
      {"dr", "rd", NULL},
      {"dd", NULL},
   };
   int32_t dist = 3;

   if (len1 < len2) {
      FC_SWAP(const char32_t *, seq1, seq2);
      FC_SWAP(int32_t, len1, len2);
   }
   STRIP(seq1, seq2, len1, len2);

   const int32_t diff = len1 - len2;
   if (diff > 2)
      return INT32_MAX;
   if (len2 == 0)
      return len1;

   for (const char *const *model = models[diff]; *model; model++) {
      int32_t i = 0, j = 0, cost = 0;

      while (i < len1 && j < len2) {
         if (seq1[i] == seq2[j]) {
            i++;
            j++;
         } else {
            cost++;
            if (cost > 2)
               break;
            switch ((*model)[cost - 1]) {
            case 'd':
               i++;
               break;
            case 'i':
               j++;
               break;
            default:
               i++;
               j++;
               break;
            }
         }
      }

      if (cost <= 2) {
         if (i < len1)
            cost += len1 - i;
         else if (j < len2)
            cost += len2 - j;
         if (cost < dist)
            dist = cost;
      }
   }

   return dist;
}

int32_t (*const fc_lev_bounded[3])(const char32_t *, int32_t, const char32_t *, int32_t) = {
   fc_lev_bounded0,
   fc_lev_bounded1,
   fc_lev_bounded2,
};


/*******************************************************************************
 * Longest common substring
 ******************************************************************************/

static int32_t fc_lcsubstr0(int32_t *column, const char32_t *seq1, int32_t len1,
                        const char32_t *seq2, int32_t len2, const char32_t **pos)
{
   assert(IN_RANGE(len1) && IN_RANGE(len2));

   memset(column, 0, len2 * sizeof *column);

   int32_t max_len = 0;
   int32_t my_pos = INT32_MAX;

   for (int32_t i = 0; i < len1; i++) {
      int32_t last = 0;
      for (int32_t j = 0; j < len2; j++) {
         const int32_t old = column[j];
         if (seq1[i] == seq2[j]) {
            column[j] = last + 1;
            if (max_len < column[j]) {
               max_len = column[j];
               my_pos = i;
            }
         } else {
            column[j] = 0;
         }
         last = old;
      }
   }

   if (pos)
      *pos = max_len ? &seq1[my_pos - max_len + 1] : &seq1[len1];
   return max_len;
}

int32_t fc_lcsubstr_extract(const char32_t *seq1, int32_t len1,
                            const char32_t *seq2, int32_t len2,
                            const char32_t **pos)
{
   assert(IN_RANGE(len1) && IN_RANGE(len2));

   /* We don't swap the sequences here to not mess up the value assigned to
    * the pointer *pos. This might result in a larger allocation.
    */
   int32_t column[FC_DEFAULT_COLUMN_LEN], *columnp = column;
   if (len2 >= (int32_t)FC_ARRAY_SIZE(column))
      columnp = fc_malloc(len2 * sizeof *columnp);

   int32_t dist = fc_lcsubstr0(columnp, seq1, len1, seq2, len2, pos);

   if (columnp != column)
      fc_free(columnp);

   return dist;
}

int32_t fc_lcsubstr(const char32_t *seq1, int32_t len1, const char32_t *seq2, int32_t len2)
{
   return fc_lcsubstr_extract(seq1, len1, seq2, len2, NULL);
}


/*******************************************************************************
 * Longest common subsequence
 ******************************************************************************/

static int32_t fc_lcsubseq0(int32_t *column, const char32_t *seq1, int32_t len1,
                        const char32_t *seq2, int32_t len2)
{
   assert(IN_RANGE(len1) && IN_RANGE(len2) && len1 >= len2);

   if (len2 == 0)
      return 0;

   memset(column, 0, (len2 + 1) * sizeof *column);

   for (int32_t i = 1; i <= len1; i++) {
      int32_t last = 0;
      for (int32_t j = 1; j <= len2; j++) {
         const int32_t old = column[j];
         if (seq1[i - 1] == seq2[j - 1])
            column[j] = last + 1;
         else if (column[j] < column[j - 1])
            column[j] = column[j - 1];
         last = old;
      }
   }

   return column[len2];
}

int32_t fc_lcsubseq(const char32_t *seq1, int32_t len1, const char32_t *seq2, int32_t len2)
{
   assert(IN_RANGE(len1) && IN_RANGE(len2));

   if (len1 < len2) {
      FC_SWAP(const char32_t *, seq1, seq2);
      FC_SWAP(int32_t, len1, len2);
   }

   int32_t column[FC_DEFAULT_COLUMN_LEN], *columnp = column;
   if (len2 >= (int32_t)FC_ARRAY_SIZE(column))
      columnp = fc_malloc((len2 + 1) * sizeof *columnp);

   int32_t res = fc_lcsubseq0(columnp, seq1, len1, seq2, len2);

   if (columnp != column)
      fc_free(columnp);

   return res;
}

double fc_nlcsubseq(const char32_t *seq1, int32_t len1,
                    const char32_t *seq2, int32_t len2)
{
   assert(IN_RANGE(len1) && IN_RANGE(len2));

   if (len1 == 0 && len2 == 0)
      return 1.;

   const int32_t lcs = fc_lcsubseq(seq1, len1, seq2, len2);
   return 1. - (2. * lcs) / (double)(len1 + len2);
}


/*******************************************************************************
 * Jaro
 ******************************************************************************/

/* Could be made to use less space by allocating an array of chars, (like
 * Winkler_1994.c does) and only allocating the space needed to hold the longest
 * sequence. Sequences would be made distinguishable by the use of a special
 * mask. But it's not worth it. Better stay consistent with the rest of the
 * implementation and allocate an array of int32_ts. Also, the space overhead is
 * negligible anyway (in comparison to the metrics that use a full matrix), and
 * the maximum length of a sequence is expected to be low.
 */
static double fc_jaro0(int32_t *matched1, const char32_t *seq1, int32_t len1,
                       const char32_t *seq2, int32_t len2)
{
   assert(IN_RANGE(len1) && IN_RANGE(len2));

   memset(matched1, 0, (len1 + len2) * sizeof *matched1);

   int32_t window = (FC_MAX(len1, len2) >> 1) - 1;
   if (window < 0)
      window = 0;

   int32_t matches = 0;
   int32_t *matched2 = &matched1[len1];

   for (int32_t i = 0; i < len1; i++) {
      int32_t bot = i - window;
      if (bot < 0)
         bot = 0;
      int32_t top = i + window + 1;
      if (top > len2)
         top = len2;

      for (int32_t j = bot; j < top; j++) {
         if (!matched2[j] && seq1[i] == seq2[j]) {
            matched1[i] = matched2[j] = 1;
            matches++;
            break;
         }
      }
   }
   if (!matches)
      return 0.;

   int32_t transpos = 0, k = 0;

   for (int32_t i = 0; i < len1; i++) {
      if (matched1[i]) {
         int32_t j;
         for (j = k; j < len2; j++) {
            if (matched2[j]) {
               k = j + 1;
               break;
            }
         }
         if (seq1[i] != seq2[j])
            transpos++;
      }
   }

   transpos >>= 1;
   return 1. - (1. / 3. * (  matches / (double)len1
                           + matches / (double)len2
                           + (matches - transpos) / (double)matches));
}

double fc_jaro(const char32_t *seq1, int32_t len1, const char32_t *seq2, int32_t len2)
{
   assert(IN_RANGE(len1) && IN_RANGE(len2));

   int32_t buf[FC_DEFAULT_COLUMN_LEN * 2], *bufp = buf;

   if (len1 + len2 > (int32_t)FC_ARRAY_SIZE(buf))
      bufp = fc_malloc((len1 + len2) * sizeof *bufp);

   double dist = fc_jaro0(bufp, seq1, len1, seq2, len2);

   if (bufp != buf)
      fc_free(bufp);

   return dist;
}


/*******************************************************************************
 * Memoized string metrics.
 ******************************************************************************/

static void memo_calloc(struct fc_memo *ctx, size_t max_len, size_t mat_size)
{
   ctx->seq2 = fc_malloc(max_len * sizeof *ctx->seq2 + mat_size);
   ctx->matrix = ctx->seq2 + max_len;
   memset(ctx->matrix, 0, mat_size);
}

void fc_memo_init(struct fc_memo *ctx, enum fc_metric metric, int32_t max_len,
                  int32_t max_dist)
{
   assert(IN_RANGE(max_len));

   ctx->mdim = max_len + 1;
   ctx->max_dist = max_dist;

   ctx->seq1 = NULL;
   ctx->len1 = 0;

   switch (metric) {

   case FC_LEVENSHTEIN: case FC_DAMERAU: {
      if (metric == FC_LEVENSHTEIN)
         ctx->compute = fc_memo_levenshtein;
      else
         ctx->compute = fc_memo_damerau;
      /* Full matrix. */
      ctx->seq2 = fc_malloc(max_len * sizeof *ctx->seq2 + sizeof(int32_t[ctx->mdim][ctx->mdim]));
      ctx->matrix = ctx->seq2 + max_len;
      int32_t (*matrix)[ctx->mdim] = ctx->matrix;
      for (int32_t i = 0; i < ctx->mdim; i++)
         matrix[i][0] = i;
      for (int32_t j = 1; j < ctx->mdim; j++)
         matrix[0][j] = j;
      break;
   }
   case FC_LCSUBSTR: {
      ctx->compute = fc_memo_lcsubstr;
      /* We add one additional column on the right of the matrix for storing
       * the length of the longest common substring found so far, for each row.
       * This is necessary because the last row doesn't necessarily contain it.
       */
      memo_calloc(ctx, max_len, sizeof(int32_t[ctx->mdim + 1][ctx->mdim]));
      break;
   }
   case FC_LCSUBSEQ: {
      ctx->compute = fc_memo_lcsubseq;
      /* Full matrix. */
      memo_calloc(ctx, max_len, sizeof(int32_t[ctx->mdim][ctx->mdim]));
      break;
   }
   default: {
      fc_fatal("invalid metric: %d", metric);
   }
   }
}

enum fc_metric fc_memo_metric(const struct fc_memo *ctx)
{
   int32_t (*const funcs[])(struct fc_memo *, const char32_t *, int32_t) = {
   #define _(metric, METRIC) [FC_##METRIC] = fc_memo_##metric,
      _(levenshtein, LEVENSHTEIN)
      _(damerau, DAMERAU)
      _(lcsubstr, LCSUBSTR)
      _(lcsubseq, LCSUBSEQ)
   #undef _
   };

   for (size_t i = 0; i < FC_ARRAY_SIZE(funcs); i++)
      if (funcs[i] == ctx->compute)
         return i;

   fc_fatal("object not properly initialized");
}

void fc_memo_set_ref(struct fc_memo *ctx,
                            const char32_t *seq1, int32_t len1)
{
   ctx->seq1 = seq1;
   ctx->len1 = len1;
   ctx->len2 = 0;
}

int32_t fc_memo_lcsubstr(struct fc_memo *ctx, const char32_t *seq2, int32_t len2)
{
   assert(ctx->seq1 && len2 >= 0 && len2 < ctx->mdim && ctx->compute == fc_memo_lcsubstr);

   const char32_t *seq1 = ctx->seq1;
   const int32_t len1 = ctx->len1;
   char32_t *old_seq2 = ctx->seq2;
   int32_t (*matrix)[ctx->mdim] = ctx->matrix;
   const int32_t max_lens = ctx->mdim;

   int32_t skip = 0, min_len2 = FC_MIN(ctx->len2, len2);
   while (skip < min_len2 && old_seq2[skip] == seq2[skip])
      skip++;
   memcpy(&old_seq2[skip], &seq2[skip], (len2 - skip) * sizeof *seq2);
   ctx->len2 = len2;

   int32_t max_len = matrix[max_lens][skip];
   for (int32_t i = skip + 1; i <= len2; i++) {
      for (int32_t j = 1; j <= len1; j++) {
         if (seq1[j - 1] == seq2[i - 1]) {
            int32_t up_left = matrix[i - 1][j - 1] + 1;
            matrix[i][j] = up_left;
            if (max_len < up_left)
               max_len = up_left;
         } else {
            matrix[i][j] = 0;
         }
      }
      matrix[max_lens][i] = max_len;
   }

   return max_len;
}

int32_t fc_memo_lcsubseq(struct fc_memo *ctx,
                            const char32_t *seq2, int32_t len2)
{
   assert(ctx->seq1 && len2 >= 0 && len2 < ctx->mdim && ctx->compute == fc_memo_lcsubseq);

   const char32_t *seq1 = ctx->seq1;
   const int32_t len1 = ctx->len1;
   char32_t *old_seq2 = ctx->seq2;
   int32_t (*matrix)[ctx->mdim] = ctx->matrix;

   int32_t skip = 0, min_len2 = FC_MIN(ctx->len2, len2);
   while (skip < min_len2 && old_seq2[skip] == seq2[skip])
      skip++;
   memcpy(&old_seq2[skip], &seq2[skip], (len2 - skip) * sizeof *seq2);
   ctx->len2 = len2;

   for (int32_t i = 1; i <= len1; i++) {
      for (int32_t j = skip + 1; j <= len2; j++) {
         if (seq1[i - 1] == seq2[j - 1]) {
            matrix[i][j] = matrix[i - 1][j - 1] + 1;
         } else {
            const int32_t fst = matrix[i][j - 1];
            const int32_t snd = matrix[i - 1][j];
            matrix[i][j] = FC_MAX(fst, snd);
         }
      }
   }
   return matrix[len1][len2];
}

static int32_t fc_memo_distance(struct fc_memo *ctx,
                                const char32_t *seq2, int32_t len2,
                                bool transpos)
{
   assert(ctx->seq1 && len2 >= 0 && len2 < ctx->mdim);

   const char32_t *seq1 = ctx->seq1;
   const int32_t len1 = ctx->len1;
   char32_t *old_seq2 = ctx->seq2;
   int32_t (*matrix)[ctx->mdim] = ctx->matrix;

   if (abs(len1 - len2) > ctx->max_dist)
      return INT32_MAX;

   int32_t skip = 0, min_len2 = FC_MIN(ctx->len2, len2);
   while (skip < min_len2 && old_seq2[skip] == seq2[skip])
      skip++;

   if (skip) {
      /* We could make this check after computing each row, and possibly break
       * from the loop early if we detect that the distance can't be <= than
       * the maximum allowed distance.
       *
       * Contrary to intuition, it turns out that this is generally slower than
       * simply going on until the sequences are exhausted. This holds at least
       * for short strings, which we expect to have to deal with here.
       *
       * Probably the slowdown is due to the additional bookkeeping needed to
       * keep track of the maximum distance found so far, which involves one
       * more check per matrix cell.
       */
      int32_t min = INT32_MAX;
      for (int32_t i = 0; i <= len1; i++) {
         const int32_t val = matrix[i][skip];
         if (val < min)
            min = val;
      }
      if (min > ctx->max_dist)
         return INT32_MAX;
   }
   memcpy(&old_seq2[skip], &seq2[skip], (len2 - skip) * sizeof *seq2);
   ctx->len2 = len2;

   for (int32_t i = 1; i <= len1; i++) {
      for (int32_t j = skip + 1; j <= len2; j++) {
         if (seq1[i - 1] == seq2[j - 1]) {
            matrix[i][j] = matrix[i - 1][j - 1];
         } else {
            int32_t ic = matrix[i][j - 1] + 1;
            int32_t dc = matrix[i - 1][j] + 1;
            int32_t rc = matrix[i - 1][j - 1] + 1;
            matrix[i][j] = FC_MIN3(ic, dc, rc);
            if (transpos && TRANSPOSED(seq1, seq2, i, j)) {
               ic = matrix[i][j];
               int32_t tc = matrix[i - 2][j - 2] + 1;
               matrix[i][j] = FC_MIN(ic, tc);
            }
         }
      }
   }
   return matrix[len1][len2];
}

int32_t fc_memo_levenshtein(struct fc_memo *ctx,
                                   const char32_t *seq2, int32_t len2)
{
   assert(ctx->compute == fc_memo_levenshtein);
   return fc_memo_distance(ctx, seq2, len2, false);
}

int32_t fc_memo_damerau(struct fc_memo *ctx,
                               const char32_t *seq2, int32_t len2)
{
   assert(ctx->compute == fc_memo_damerau);
   return fc_memo_distance(ctx, seq2, len2, true);
}

void fc_memo_fini(struct fc_memo *ctx)
{
   fc_free(ctx->seq2);
}
