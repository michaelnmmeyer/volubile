#include "priv.h"
#include <string.h>

%%{

machine vb_simplify_glob;

action putchar {
   buf[len++] = fc;
}

glob_char = ("?" | "[" | "]") > putchar;
wildcard = "*"+ > putchar;
other = (any - glob_char - "*") > putchar;

prefix = other+ wildcard % {
   c->mode = VB_PREFIX;
   c->str = buf;
   c->len = len - 1;
};
infix = wildcard other+ wildcard % {
   buf[len - 1] = '\0';    /* We use strstr() in the matching function. */
   c->mode = VB_SUBSTR;
   c->str = buf + 1;
   c->len = len - 2;
};
suffix = wildcard other+ % {
   c->mode = VB_SUFFIX;
   c->str = buf + 1;
   c->len = len - 1;
};
exact = other* % {
   c->mode = VB_EXACT;
};
glob = ((other | wildcard | glob_char)** - prefix - infix - suffix - exact) % {
   c->mode = VB_GLOB;
   c->str = buf;
   c->len = len;
};

main := prefix | infix | suffix | exact | glob;

}%%

%% write data noerror nofinal;

static void vb_simplify_glob(struct vb_match_ctx *c, char *buf)
{
   int cs;
   const char *p = c->str;
   const char *pe = &p[c->len];
   const char *const eof = pe;

   size_t len = 0;
   %% write init;
   %% write exec;

   (void)vb_simplify_glob_en_main;
}

static void vb_set_match_mode(struct vb_match_ctx *c)
{
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
