
#line 1 "src/parse.rl"
#include "priv.h"
#include <string.h>


#line 43 "src/parse.rl"



#line 12 "src/parse.c"
static const char _vb_simplify_glob_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	3, 1, 4, 1, 5
};

static const char _vb_simplify_glob_key_offsets[] = {
	0, 4, 8, 9, 10, 11, 15, 19
};

static const char _vb_simplify_glob_trans_keys[] = {
	42, 63, 91, 93, 42, 63, 91, 93, 
	42, 42, 42, 42, 63, 91, 93, 42, 
	63, 91, 93, 42, 0
};

static const char _vb_simplify_glob_single_lengths[] = {
	4, 4, 1, 1, 1, 4, 4, 1
};

static const char _vb_simplify_glob_range_lengths[] = {
	0, 0, 0, 0, 0, 0, 0, 0
};

static const char _vb_simplify_glob_index_offsets[] = {
	0, 5, 10, 12, 14, 16, 21, 26
};

static const char _vb_simplify_glob_indicies[] = {
	1, 2, 2, 2, 0, 3, 2, 2, 
	2, 0, 4, 2, 5, 2, 6, 2, 
	8, 2, 2, 2, 7, 9, 2, 2, 
	2, 7, 10, 2, 0
};

static const char _vb_simplify_glob_trans_targs[] = {
	1, 5, 3, 2, 2, 4, 4, 6, 
	5, 7, 7
};

static const char _vb_simplify_glob_trans_actions[] = {
	1, 1, 1, 1, 0, 1, 0, 1, 
	0, 1, 0
};

static const char _vb_simplify_glob_eof_actions[] = {
	9, 9, 3, 11, 11, 11, 7, 5
};

static const int vb_simplify_glob_start = 0;

static const int vb_simplify_glob_en_main = 0;


#line 46 "src/parse.rl"

static void vb_simplify_glob(struct vb_match_ctx *c, char *buf)
{
   int cs;
   const char *p = c->str;
   const char *pe = &p[c->len];
   const char *const eof = pe;

   size_t len = 0;
   
#line 77 "src/parse.c"
	{
	cs = vb_simplify_glob_start;
	}

#line 56 "src/parse.rl"
   
#line 84 "src/parse.c"
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const char *_keys;

	if ( p == pe )
		goto _test_eof;
_resume:
	_keys = _vb_simplify_glob_trans_keys + _vb_simplify_glob_key_offsets[cs];
	_trans = _vb_simplify_glob_index_offsets[cs];

	_klen = _vb_simplify_glob_single_lengths[cs];
	if ( _klen > 0 ) {
		const char *_lower = _keys;
		const char *_mid;
		const char *_upper = _keys + _klen - 1;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + ((_upper-_lower) >> 1);
			if ( (*p) < *_mid )
				_upper = _mid - 1;
			else if ( (*p) > *_mid )
				_lower = _mid + 1;
			else {
				_trans += (unsigned int)(_mid - _keys);
				goto _match;
			}
		}
		_keys += _klen;
		_trans += _klen;
	}

	_klen = _vb_simplify_glob_range_lengths[cs];
	if ( _klen > 0 ) {
		const char *_lower = _keys;
		const char *_mid;
		const char *_upper = _keys + (_klen<<1) - 2;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + (((_upper-_lower) >> 1) & ~1);
			if ( (*p) < _mid[0] )
				_upper = _mid - 2;
			else if ( (*p) > _mid[1] )
				_lower = _mid + 2;
			else {
				_trans += (unsigned int)((_mid - _keys)>>1);
				goto _match;
			}
		}
		_trans += _klen;
	}

_match:
	_trans = _vb_simplify_glob_indicies[_trans];
	cs = _vb_simplify_glob_trans_targs[_trans];

	if ( _vb_simplify_glob_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _vb_simplify_glob_actions + _vb_simplify_glob_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 0:
#line 8 "src/parse.rl"
	{
   buf[len++] = (*p);
}
	break;
#line 162 "src/parse.c"
		}
	}

_again:
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	const char *__acts = _vb_simplify_glob_actions + _vb_simplify_glob_eof_actions[cs];
	unsigned int __nacts = (unsigned int) *__acts++;
	while ( __nacts-- > 0 ) {
		switch ( *__acts++ ) {
	case 1:
#line 16 "src/parse.rl"
	{
   c->mode = VB_PREFIX;
   c->str = buf;
   c->len = len - 1;
}
	break;
	case 2:
#line 21 "src/parse.rl"
	{
   buf[len - 1] = '\0';    /* We use strstr() in the matching function. */
   c->mode = VB_SUBSTR;
   c->str = buf + 1;
   c->len = len - 2;
}
	break;
	case 3:
#line 27 "src/parse.rl"
	{
   c->mode = VB_SUFFIX;
   c->str = buf + 1;
   c->len = len - 1;
}
	break;
	case 4:
#line 32 "src/parse.rl"
	{
   c->mode = VB_EXACT;
}
	break;
	case 5:
#line 35 "src/parse.rl"
	{
   c->mode = VB_GLOB;
   c->str = buf;
   c->len = len;
}
	break;
#line 215 "src/parse.c"
		}
	}
	}

	}

#line 57 "src/parse.rl"

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
