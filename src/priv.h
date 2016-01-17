#ifndef VB_PRIV_H
#define VB_PRIV_H

#include <uchar.h>
#include "api.h"
#include "lib/mini.h"

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
