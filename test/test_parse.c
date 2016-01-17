#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "../src/priv.h"

static const char *const modes[] = {
#define _(NAME) [VB_##NAME] = #NAME,
   _(AUTO)
   _(EXACT)
   _(PREFIX)
   _(SUFFIX)
   _(SUBSTR)
   _(GLOB)
   _(LEVENSHTEIN)
   _(DAMERAU)
   _(LCSUBSTR)
   _(LCSUBSEQ)
};

int main(int argc, char **argv)
{
   if (argc != 2) {
      printf("Usage: %s <query>\n", *argv);
      exit(1);
   }

   struct vb_match_ctx c = {
      .mode = VB_AUTO,
      .str = argv[1],
      .len = strlen(argv[1]),
   };

   char buf[MN_MAX_WORD_LEN + 1];
   vb_parse_query(&c, buf);

   printf("%s\t%.*s\t%s\n", argv[1], (int)c.len, c.str, modes[c.mode]);
}
