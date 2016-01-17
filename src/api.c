#include <string.h>
#include "api.h"
#include "priv.h"
#include "lib/mini.h"

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
