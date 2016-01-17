#include <assert.h>
#include <stdint.h>
#include "priv.h"

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
