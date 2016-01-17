#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include <assert.h>
#include <stdbool.h>
#include <arpa/inet.h>     /* htonl(), ntohl() */

#include "mini.h"

/* Size of the states hash table. */
#define MN_HT_SIZE (1 << 18)

/* Maximum number of transitions in a single automaton. */
#define MN_MAX_SIZE (1 << 22)

/* We don't use bitfields for portability. */
#define IS_LAST(trans) ((trans) & 0x1)
#define IS_TERMINAL(state) ((state) & 0x2)
#define GET_CHAR(trans) (uint8_t)(((trans) >> 2) & 0xff)
#define GET_DEST(trans) (uint32_t)(((trans) >> 10) & ((1 << 22) - 1))

#define SET_FLAG_BIT(num, flag, mask) do {                                     \
   if (flag)                                                                   \
      num |= mask;                                                             \
   else                                                                        \
      num &= ~mask;                                                            \
} while (0)

#define SET_LAST(trans, flag) SET_FLAG_BIT(trans, flag, 0x1)
#define SET_TERMINAL(state, flag) SET_FLAG_BIT(state, flag, 0x2)
#define SET_CHAR(trans, chr) do {                                              \
   trans |= (uint32_t)(chr) << 2;                                              \
} while (0)
#define SET_DEST(trans, pos) do {                                              \
   trans |= (uint32_t)(pos) << 10;                                             \
} while (0)

static const uint32_t mn_magic = 1835626089;
static const uint32_t mn_version = 1;

static int lmemcmp(const void *restrict str1, size_t len1,
                   const void *restrict str2, size_t len2)
{
   int cmp = memcmp(str1, str2, len1 < len2 ? len1 : len2);
   if (cmp)
      return cmp;
   return len1 < len2 ? -1 : len1 > len2;
}

const char *mn_strerror(int err)
{
   static const char *const tbl[] = {
      [MN_OK] = "no error",
      [MN_EWORD] = "attempt to add the empty string or a too long word",
      [MN_EORDER] = "word added out of order",
      [MN_EMAGIC] = "magic identifier mismatch",
      [MN_EVERSION] = "version mismatch",
      [MN_ECORRUPT] = "automaton is corrupt",
      [MN_EFREEZED] = "attempt to add a word to a freezed automaton",
      [MN_E2BIG] = "automaton has grown too large",
      [MN_EIO] = "IO error",
   };

   if (err >= 0 && (size_t)err < sizeof tbl / sizeof *tbl)
      return tbl[err];
   return "unknown error";
}


/*******************************************************************************
 * Encoder
 ******************************************************************************/

/* Record type for the states hash table. */
struct mini_enc_bkt {
   unsigned nr;                  /* Number of outgoing transitions. */
   uint32_t hash;                /* Hash value. */
   uint32_t addr;                /* Position in the automaton array. */
   struct mini_enc_bkt *next;    /* Next record pointer. */
};

/* Automaton encoder. */
struct mini_enc {
   uint8_t prev[MN_MAX_WORD_LEN + 1];    /* Previous word added. */
   size_t prev_len;                          /* Length of this word. */

   /* Temporary states. */
   struct mini_state {
      uint32_t transitions[1 << 8];    /* Outgoing transitions. */
      unsigned nr;                     /* Number of outgoing transitions. */
      bool terminal;                   /* Whether terminal. */
   } states[MN_MAX_WORD_LEN + 1];

   struct mini_enc_bkt *table[MN_HT_SIZE];  /* States hash table. */

   /* Whether the automaton has been dumped at least one time, in which case
    * adding new words is not allowed anymore.
    */
   bool finished;

   uint32_t *counts;          /* Array of word counts (may be NULL). */
   uint32_t aut_size;         /* Size of the automaton array (= size of the
                               * counts array). */
   uint32_t automaton[];      /* The automaton proper. */
};

struct mini_enc *mn_enc_new(enum mn_type type)
{
   assert(type == MN_STANDARD || type == MN_NUMBERED);

   struct mini_enc *enc;
   const size_t size = offsetof(struct mini_enc, automaton);
   if (type == MN_NUMBERED) {
      enc = calloc(1, size + 2 * MN_MAX_SIZE * sizeof *enc->automaton);
      enc->counts = &enc->automaton[MN_MAX_SIZE];
   } else {
      enc = calloc(1, size + MN_MAX_SIZE * sizeof *enc->automaton);
   }
   return enc;
}

static void mn_clear_table(struct mini_enc *enc)
{
   for (size_t i = 0; i < sizeof enc->table / sizeof *enc->table; i++) {
      struct mini_enc_bkt *r = enc->table[i];
      while (r) {
         struct mini_enc_bkt *next = r->next;
         free(r);
         r = next;
      }
   }
   memset(enc->table, 0, sizeof enc->table);
}

void mn_enc_free(struct mini_enc *enc)
{
   mn_clear_table(enc);
   free(enc);
}

void mn_enc_clear(struct mini_enc *enc)
{
   enc->prev_len = 0;
   enc->aut_size = 0;
   memset(enc->states, 0, sizeof enc->states);
   enc->finished = false;
   mn_clear_table(enc);
}

static uint32_t hash_state(const struct mini_state *const state)
{
   uint32_t hash = 0;
   for (unsigned i = 0; i < state->nr; i++)
      hash += state->transitions[i];
   return (hash * 324027) >> 13;
}

static uint32_t mkstate(struct mini_enc *enc, struct mini_state *state)
{
   if (!state->nr)
      state->transitions[state->nr++] = 0;
   SET_LAST(state->transitions[state->nr - 1], true);

   const uint32_t hash = hash_state(state);
   const uint32_t pos = hash & (MN_HT_SIZE - 1);

   struct mini_enc_bkt *bkt;
   for (bkt = enc->table[pos]; bkt; bkt = bkt->next) {
      if (bkt->hash == hash && bkt->nr == state->nr &&
         !memcmp(&enc->automaton[bkt->addr], state->transitions, state->nr * sizeof *state->transitions))
         return bkt->addr;
   }

   if (enc->aut_size + state->nr >= MN_MAX_SIZE)
      return UINT32_MAX;

   bkt = malloc(sizeof *bkt);
   *bkt = (struct mini_enc_bkt){
      .hash = hash,
      .addr = enc->aut_size,
      .nr = state->nr,
      .next = enc->table[pos],
   };
   enc->table[pos] = bkt;

   memcpy(&enc->automaton[enc->aut_size], state->transitions, state->nr * sizeof *state->transitions);
   enc->aut_size += state->nr;

   return bkt->addr;
}

static int minimize(struct mini_enc *enc, size_t lim)
{
   while (enc->prev_len > lim) {
      const uint32_t dest = mkstate(enc, &enc->states[enc->prev_len]);
      if (dest == UINT32_MAX)
         return MN_E2BIG;

      uint32_t state = 0;
      SET_DEST(state, dest);
      SET_TERMINAL(state, enc->states[enc->prev_len].terminal);
      SET_CHAR(state, enc->prev[--enc->prev_len]);
      enc->states[enc->prev_len].transitions[enc->states[enc->prev_len].nr++] = state;
   }
   return MN_OK;
}

static int add_word(struct mini_enc *enc, const uint8_t *word, size_t len)
{
   size_t pref_len = 0;
   size_t min_len = len < enc->prev_len ? len : enc->prev_len;
   while (pref_len < min_len && word[pref_len] == enc->prev[pref_len])
      pref_len++;

   int ret = minimize(enc, pref_len);
   if (ret)
      return ret;

   while (enc->prev_len < len) {
      enc->prev[enc->prev_len] = word[enc->prev_len];
      enc->states[++enc->prev_len].terminal = false;
      enc->states[enc->prev_len].nr = 0;
   }
   enc->prev[enc->prev_len] = '\0';
   enc->states[enc->prev_len].terminal = true;

   return MN_OK;
}

int mn_enc_add(struct mini_enc *enc, const void *word, size_t len)
{
   if (enc->finished)
      return MN_EFREEZED;

   if (len == 0 || len > MN_MAX_WORD_LEN)
      return MN_EWORD;

   if (lmemcmp(word, len, enc->prev, enc->prev_len) <= 0)
      return MN_EORDER;

   return add_word(enc, word, len);
}

static uint32_t number_states(struct mini_enc *enc, uint32_t pos)
{
   uint32_t count = 0;

   if (!pos)
      return count;
   do {
      uint32_t new_count = number_states(enc, GET_DEST(enc->automaton[pos]));
      if (IS_TERMINAL(enc->automaton[pos]))
         new_count++;
      enc->counts[pos] = new_count;
      count += new_count;
   } while (!IS_LAST(enc->automaton[pos++]));

   return count;
}

static int finish(struct mini_enc *enc)
{
   int ret = minimize(enc, 0);
   if (ret)
      return ret;

   uint32_t start_state = mkstate(enc, &enc->states[0]);
   if (start_state == UINT32_MAX)
      return MN_E2BIG;

   SET_DEST(enc->automaton[0], start_state);
   if (enc->counts)
      enc->counts[0] = number_states(enc, start_state);

   return MN_OK;
}

static void swap_aut(struct mini_enc *enc)
{
   for (uint32_t i = 0; i < enc->aut_size; i++)
      enc->automaton[i] = htonl(enc->automaton[i]);

   if (enc->counts)
      for (uint32_t i = 0; i < enc->aut_size; i++)
         enc->counts[i] = htonl(enc->counts[i]);
}

int mn_enc_dump(struct mini_enc *enc,
                int (*write)(void *arg, const void *data, size_t size),
                void *arg)
{
   if (!enc->finished) {
      int ret = finish(enc);
      if (ret)
         return ret;
      swap_aut(enc);
      enc->finished = true;
   }

   uint32_t size = enc->counts ? MN_NUMBERED : MN_STANDARD;
   size |= enc->aut_size << 8;
   uint32_t header[3] = {
      htonl(mn_magic),
      htonl(mn_version),
      htonl(size),
   };
   if (write(arg, header, sizeof header))
      return MN_EIO;

   size_t to_write = enc->aut_size * sizeof *enc->automaton;
   if (write(arg, enc->automaton, to_write) || (enc->counts && write(arg, enc->counts, to_write)))
      return MN_EIO;
   return MN_OK;
}

static int mn_write(void *fp, const void *data, size_t size)
{
   if (fwrite(data, 1, size, fp) == size)
      return 0;
   return -1;
}

int mn_enc_dump_file(struct mini_enc *enc, FILE *fp)
{
   int ret = mn_enc_dump(enc, mn_write, fp);
   if (ret)
      return ret;

   return fflush(fp) ? MN_EIO : MN_OK;
}


/*******************************************************************************
 * Decoder
 ******************************************************************************/

struct mini {
   const uint32_t *counts;
   uint32_t nr;               /* Number of transitions. */
   uint32_t transitions[];
};

int mn_load(struct mini **fsap,
            int (*read)(void *arg, void *buf, size_t size),
            void *arg)
{
   *fsap = NULL;

   uint32_t header[3];
   if (read(arg, header, sizeof header))
      return MN_EIO;
   for (size_t i = 0; i < sizeof header / sizeof *header; i++)
      header[i] = ntohl(header[i]);

   if (header[0] != mn_magic)
      return MN_EMAGIC;
   if (header[1] != mn_version)
      return MN_EVERSION;

   uint32_t nr = header[2] >> 8;
   if (nr < 1 || nr >= MN_MAX_SIZE)
      return MN_ECORRUPT;

   int type = header[2] & 0xff;
   size_t to_read;
   if (type == MN_STANDARD)
      to_read = sizeof(uint32_t[nr]);
   else if (type == MN_NUMBERED)
      to_read = sizeof(uint32_t[2][nr]);
   else
      return MN_ECORRUPT;

   struct mini *fsa = malloc(offsetof(struct mini, transitions) + to_read);
   if (read(arg, fsa->transitions, to_read)) {
      free(fsa);
      return MN_EIO;
   }

   to_read /= sizeof *fsa->transitions;
   for (size_t i = 0; i < to_read; i++)
      fsa->transitions[i] = ntohl(fsa->transitions[i]);

   fsa->counts = (type == MN_NUMBERED) ? &fsa->transitions[nr] : NULL;
   fsa->nr = nr;
   *fsap = fsa;
   return MN_OK;
}

static int mn_read(void *fp, void *buf, size_t size)
{
   if (fread(buf, 1, size, fp) == size)
      return 0;
   return -1;
}

int mn_load_file(struct mini **fsa, FILE *fp)
{
   int ret = mn_load(fsa, mn_read, fp);
   if (ret)
      return ret;
   
   return ferror(fp) ? MN_EIO : MN_OK;
}

enum mn_type mn_type(const struct mini *fsa)
{
   return fsa->counts ? MN_NUMBERED : MN_STANDARD;
}

void mn_free(struct mini *fsa)
{
   free(fsa);
}

int mn_contains(const struct mini *fsa, const void *word, size_t len)
{
   const uint32_t *transitions = fsa->transitions;
   uint32_t pos = 0;

   for (size_t i = 0; i < len; i++) {
      pos = GET_DEST(transitions[pos]);
      if (!pos)
         return 0;
      while (GET_CHAR(transitions[pos]) != ((const uint8_t *)word)[i]) {
         if (IS_LAST(transitions[pos++]))
            return 0;
      }
   }
   return IS_TERMINAL(transitions[pos]);
}

static uint32_t count_words(const uint32_t *transitions, uint32_t pos)
{
   uint32_t count = 0;

   if (!pos)
      return 0;
   do {
      if (IS_TERMINAL(transitions[pos]))
         count++;
      count += count_words(transitions, GET_DEST(transitions[pos]));
   } while (!IS_LAST(transitions[pos++]));

   return count;
}

uint32_t mn_size(const struct mini *fsa)
{
   if (fsa->counts)
      return fsa->counts[0];
   return count_words(fsa->transitions, GET_DEST(fsa->transitions[0]));
}

uint32_t mn_locate(const struct mini *fsa, const void *word, size_t len)
{
   const uint32_t *transitions = fsa->transitions;
   const uint32_t *counts = fsa->counts;
   uint32_t pos = 0;
   uint32_t index = 0;

   if (!counts)
      return 0;

   for (size_t i = 0; i < len; i++) {
      pos = GET_DEST(transitions[pos]);
      if (!pos)
         return 0;
      while (GET_CHAR(transitions[pos]) != ((const uint8_t *)word)[i]) {
         if (IS_LAST(transitions[pos]))
            return 0;
         index += counts[pos++];
      }
      if (IS_TERMINAL(transitions[pos]))
         index++;
   }
   return IS_TERMINAL(transitions[pos]) ? index : 0;
}

size_t mn_extract(const struct mini *fsa, uint32_t index, void *buf)
{
   const uint32_t *transitions = fsa->transitions;
   const uint32_t *counts = fsa->counts;
   uint32_t pos = 0;
   size_t len = 0;

   if (!index || !counts || counts[0] < index) {
      ((uint8_t *)buf)[0] = '\0';
      return 0;
   }

   do {
      pos = GET_DEST(transitions[pos]);
      for (;;) {
         uint32_t cnt = counts[pos];
         if (index > cnt) {
            index -= cnt;
         } else {
            ((uint8_t *)buf)[len++] = GET_CHAR(transitions[pos]);
            if (IS_TERMINAL(transitions[pos]))
               index--;
            break;
         }
         pos++;
      }
   } while (index);

   ((uint8_t *)buf)[len] = '\0';
   return len;
}


/*******************************************************************************
 * Iterator
 ******************************************************************************/

/* Dummy iterator that will stop on the next call to "mn_iter_next()". */
static uint32_t init_none(struct mini_iter *it)
{
   it->depth = 1;
   it->root = 0;
   it->positions[0] = it->positions[1] = 0;
   return 0;
}

uint32_t mn_iter_inits_standard(struct mini_iter *it, const struct mini *fsa,
                                const uint8_t *word, size_t len)
{
   if (len == 0)
      return mn_iter_init(it, fsa);

   it->fsa = fsa;
   it->depth = it->root = 0;

   uint32_t pos = 0;
   for (size_t i = 0; i < len; i++) {
      pos = GET_DEST(fsa->transitions[pos]);
      if (!pos)
         goto find_next_word;
      int c;
      while ((c = GET_CHAR(fsa->transitions[pos])) < word[i]) {
         if (IS_LAST(fsa->transitions[pos++]))
            goto find_next_word;
      }
      it->positions[it->depth] = pos;
      it->word[it->depth++] = word[i];
      if (c > word[i])
         break;
   }

   it->depth--;
   return 1;

find_next_word:
   if (it->depth == 0)
      return init_none(it);
   while (IS_LAST(fsa->transitions[it->positions[--it->depth]])) {
      if (it->depth == 0)
         return init_none(it);
   }
   it->positions[it->depth]++;
   return 1;
}

uint32_t mn_iter_inits_numbered(struct mini_iter *it, const struct mini *fsa,
                                const uint8_t *word, size_t len)
{
   if (len == 0)
      return mn_iter_init(it, fsa);

   it->fsa = fsa;
   it->depth = it->root = 0;

   uint32_t pos = 0, index = 0;
   for (size_t i = 0; i < len; i++) {
      pos = GET_DEST(fsa->transitions[pos]);
      if (!pos)
         goto find_next_word;
      int c;
      while ((c = GET_CHAR(fsa->transitions[pos])) < word[i]) {
         index += fsa->counts[pos];
         if (IS_LAST(fsa->transitions[pos++]))
            goto find_next_word;
      }
      if (IS_TERMINAL(fsa->transitions[pos]))
         index++;
      it->positions[it->depth] = pos;
      it->word[it->depth++] = word[i];
      if (c > word[i])
         break;
   }

   if (!IS_TERMINAL(fsa->transitions[pos]))
      index++;
   it->depth--;
   return index;

find_next_word:
   if (it->depth == 0)
      return init_none(it);
   index++;
   while (IS_LAST(fsa->transitions[it->positions[--it->depth]])) {
      if (it->depth == 0)
         return init_none(it);
   }
   it->positions[it->depth]++;
   return index;
}

uint32_t mn_iter_inits(struct mini_iter *it, const struct mini *fsa,
                       const void *str, size_t len)
{
   return fsa->counts ?
      mn_iter_inits_numbered(it, fsa, str, len) :
      mn_iter_inits_standard(it, fsa, str, len);
}

static uint32_t mn_iter_initp_standard(struct mini_iter *it,
                                       const struct mini *fsa,
                                       const uint8_t *prefix, size_t len)
{
   if (len == 0)
      return mn_iter_init(it, fsa);

   it->fsa = fsa;
   it->depth = 0;

   uint32_t pos = 0;
   for (size_t i = 0; i < len; i++) {
      pos = GET_DEST(fsa->transitions[pos]);
      if (!pos)
         return init_none(it);
      while (GET_CHAR(fsa->transitions[pos]) != prefix[i]) {
         if (IS_LAST(fsa->transitions[pos++]))
            return init_none(it);
      }
      it->positions[it->depth] = pos;
      it->word[it->depth++] = prefix[i];
   }

   it->root = it->depth--;
   return 1;
}

static uint32_t mn_iter_initp_numbered(struct mini_iter *it,
                                       const struct mini *fsa,
                                       const uint8_t *prefix, size_t len)
{
   if (len == 0)
      return mn_iter_init(it, fsa);

   it->fsa = fsa;
   it->depth = 0;

   uint32_t pos = 0, index = 0;
   for (size_t i = 0; i < len; i++) {
      pos = GET_DEST(fsa->transitions[pos]);
      if (!pos)
         return init_none(it);
      while (GET_CHAR(fsa->transitions[pos]) != prefix[i]) {
         if (IS_LAST(fsa->transitions[pos]))
            return init_none(it);
         index += fsa->counts[pos++];
      }
      if (IS_TERMINAL(fsa->transitions[pos]))
         index++;
      it->positions[it->depth] = pos;
      it->word[it->depth++] = prefix[i];
   };

   if (!IS_TERMINAL(fsa->transitions[pos]))
       index++;

   it->root = it->depth--;
   return index;
}

uint32_t mn_iter_init(struct mini_iter *it, const struct mini *fsa)
{
   it->fsa = fsa;
   it->depth = it->root = 0;

   uint32_t pos = GET_DEST(fsa->transitions[0]);
   if (!pos)
      return init_none(it);
   it->positions[0] = pos;
   it->root = 0;
   return 1;
}

uint32_t mn_iter_initp(struct mini_iter *it, const struct mini *fsa,
                       const void *prefix, size_t len)
{
   return fsa->counts ?
      mn_iter_initp_numbered(it, fsa, prefix, len) :
      mn_iter_initp_standard(it, fsa, prefix, len);
}

uint32_t mn_iter_initn(struct mini_iter *it, const struct mini *fsa,
                       uint32_t index)
{
   it->fsa = fsa;
   it->root = it->depth = 0;

   if (!fsa->counts || index == 0 || index > fsa->counts[0])
      return init_none(it);

   uint32_t pos = 0, index_copy = index;
   do {
      pos = GET_DEST(fsa->transitions[pos]);
      for (;;) {
         uint32_t cnt = fsa->counts[pos];
         if (index > cnt) {
            index -= cnt;
         } else {
            it->word[it->depth] = GET_CHAR(fsa->transitions[pos]);
            if (IS_TERMINAL(fsa->transitions[pos]))
               index--;
            it->positions[it->depth++] = pos;
            break;
         }
         pos++;
      }
   } while (index);

   it->depth--;
   return index_copy;
}

const char *mn_iter_next(struct mini_iter *it, size_t *len)
{
   const uint32_t *transitions = it->fsa->transitions;
   uint32_t *positions = it->positions;
   size_t depth = it->depth;
   char *word = it->word;

   if (!positions[depth]) {
      while (IS_LAST(transitions[positions[--depth]]))
         if (depth <= it->root)
            goto fini;
      if (depth < it->root) {
      fini:
         init_none(it);
         if (len)
            *len = 0;
         return NULL;
      }
      positions[depth]++;
   }

   uint32_t transition;
   do {
      transition = transitions[positions[depth]];
      word[depth] = GET_CHAR(transition);
      positions[++depth] = GET_DEST(transition);
   } while (!IS_TERMINAL(transition));

   word[it->depth = depth] = '\0';
   if (len)
      *len = depth;
   return word;
}


/*******************************************************************************
 * Debugging
 ******************************************************************************/

static void mn_dump_txt(const struct mini *fsa, FILE *fp)
{
   struct mini_iter c;
   const char *word;
   size_t len;

   mn_iter_init(&c, fsa);
   while ((word = mn_iter_next(&c, &len))) {
      fwrite(word, 1, len, fp);
      putc('\n', fp);
   }
}

static void mn_dump_tsv(const struct mini *fsa, FILE *fp)
{
   fputs("char\tterminal\tlast\tdest\tcount\n", fp);
   for (uint32_t pos = 0; pos < fsa->nr; pos++) {
      uint32_t trans = fsa->transitions[pos];
      uint8_t ch = GET_CHAR(trans);
      bool is_terminal = IS_TERMINAL(trans);
      bool is_last = IS_LAST(trans);
      uint32_t dest = GET_DEST(trans);
      uint32_t count = fsa->counts ? fsa->counts[pos] : 0;
      fprintf(fp, "0x%x\t%d\t%d\t%"PRIu32"\t%"PRIu32"\n", ch, is_terminal, is_last, dest, count);
   }
}

static void mn_dump_dot(const struct mini *fsa, FILE *fp)
{
   fputs("digraph FSA {\n", fp);

   /* If there is a single transition, don't output anything. */
   uint32_t i = 1;
   while (i < fsa->nr) {
      uint32_t j = i;
      do {
         uint32_t dest = GET_DEST(fsa->transitions[j]);
         unsigned char trans_char = GET_CHAR(fsa->transitions[j]);
         char label[32];
         if (isprint(trans_char) && trans_char != '"')
            snprintf(label, sizeof label, "%c", trans_char);
         else
            snprintf(label, sizeof label, "0x%02x", trans_char);
         if (fsa->counts)
            snprintf(label + strlen(label), sizeof label - strlen(label),
                     " (%"PRIu32")", fsa->counts[j]);
         fprintf(fp, "%"PRIu32" -> %"PRIu32" [label=\"%s\"]\n", i, dest, label);
         if (IS_TERMINAL(fsa->transitions[j]))
            fprintf(fp, "%"PRIu32" [style=filled];\n", dest);
      } while (!IS_LAST(fsa->transitions[j++]));
      i = j;
   }

   fputs("}\n", fp);
}

int mn_dump(const struct mini *fsa, FILE *fp, enum mn_dump_format format)
{
   static void (*const fns[])(const struct mini *, FILE *) = {
      [MN_DUMP_TXT] = mn_dump_txt,
      [MN_DUMP_TSV] = mn_dump_tsv,
      [MN_DUMP_DOT] = mn_dump_dot,
   };

   if (format >= sizeof fns / sizeof *fns)
      format = MN_DUMP_TXT;

   fns[format](fsa, fp);
   fflush(fp);
   return ferror(fp) ? MN_EIO : MN_OK;
}
