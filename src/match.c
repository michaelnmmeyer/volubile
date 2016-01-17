#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include "lib/faconde.h"
#include "lib/mini.h"
#include "api.h"
#include "heap.h"
#include "priv.h"

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
