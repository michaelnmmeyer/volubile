#include <lua.h>
#include <lauxlib.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include "../volubile.h"
#include "../src/lib/mini.h"
#include "../src/lib/faconde.h"

/* See the Lua binding of mini. */
#define MN_MT "mini"

struct mini_lua {
   struct mini *fsa;
   int lua_ref;
   int ref_cnt;
};

static int vb_lua_load(lua_State *lua)
{
   const char *path = luaL_checkstring(lua, 1);
   struct mini_lua *fsa = lua_newuserdata(lua, sizeof *fsa);

   FILE *fp = fopen(path, "rb");
   if (!fp) {
      lua_pushnil(lua);
      lua_pushstring(lua, strerror(errno));
      return 2;
   }
   
   int ret = mn_load_file(&fsa->fsa, fp);
   fclose(fp);
   if (ret) {
      lua_pushnil(lua);
      lua_pushstring(lua, mn_strerror(ret));
      return 2;
   }

   fsa->lua_ref = LUA_NOREF;
   fsa->ref_cnt = 0;
   luaL_getmetatable(lua, MN_MT);
   lua_setmetatable(lua, -2);
   return 1;
}

static int vb_lua_free(lua_State *lua)
{
   struct mini_lua *fsa = luaL_checkudata(lua, 1, MN_MT);
   mn_free(fsa->fsa);
   return 0;
}

struct vb_lua_match_ctx {
   int count;
   lua_State *lua;
};

static void vb_lua_match_handle(void *arg, const char *term, size_t len)
{
   struct vb_lua_match_ctx *ctx = arg;

   lua_pushlstring(ctx->lua, term, len);
   lua_rawseti(ctx->lua, -2, ++ctx->count);
}

static int vb_lua_match_mode(const char *name)
{
   static const char *const modes[] = {
      [VB_AUTO] = "auto",
      [VB_EXACT] = "exact",
      [VB_PREFIX] = "prefix",
      [VB_SUBSTR] = "substr",
      [VB_SUFFIX] = "suffix",
      [VB_GLOB] = "glob",
      [VB_LEVENSHTEIN] = "levenshtein",
      [VB_DAMERAU] = "damerau",
      [VB_LCSUBSTR] = "lcsubstr",
      [VB_LCSUBSEQ] = "lcsubseq",
   };
   
   for (size_t i = 0; i < sizeof modes / sizeof *modes; i++)
      if (!strcmp(modes[i], name))
         return i;
   return -1;
}

static void map_params(lua_State *lua, int idx, struct vb_query *query)
{
   /* Don't bother about overflow. */
#define _(field, name) do {                                                    \
   lua_getfield(lua, idx, name);                                               \
   if (!lua_isnil(lua, -1)) {                                                  \
      query->field = lua_tonumber(lua, -1);                                    \
      lua_pop(lua, 1);                                                         \
   }                                                                           \
} while (0)
   _(pagination.last_pos, "last_pos");
   _(pagination.last_weight, "last_weight");
   _(page_size, "page_size");
   _(max_dist, "max_dist");
   _(prefix_len, "prefix_len");
#undef _

   lua_getfield(lua, idx, "mode");
   if (!lua_isnil(lua, -1)) {
      const char *name = lua_tostring(lua, -1);
      if (!name)
         luaL_error(lua, "matching mode must be a string");
      int mode = vb_lua_match_mode(name);
      if (mode < 0)
         luaL_error(lua, "invalid matching mode: '%s'", name);
      query->mode = mode;
   }
   lua_pop(lua, 1);
}

static int vb_lua_match(lua_State *lua)
{
   struct mini_lua *lexicon = luaL_checkudata(lua, 1, MN_MT);
   
   struct vb_query query = VB_QUERY_INIT;
   query.query = luaL_checklstring(lua, 2, &query.len);
   
   if (!lua_isnoneornil(lua, 3)) {
      luaL_checktype(lua, 3, LUA_TTABLE);
      map_params(lua, 3, &query);
   }
   
   lua_createtable(lua, query.page_size, 3);
   struct vb_lua_match_ctx ctx = {.lua = lua};

   int ret = vb_match(lexicon->fsa, &query, vb_lua_match_handle, &ctx);
   if (ret)
      return luaL_error(lua, "%s", vb_strerror(ret));

   lua_pushboolean(lua, query.pagination.last_page);
   lua_setfield(lua, -2, "last_page");
   lua_pushnumber(lua, query.pagination.last_pos);
   lua_setfield(lua, -2, "last_pos");
   lua_pushnumber(lua, query.pagination.last_weight);
   lua_setfield(lua, -2, "last_weight");
   return 1;
}

#ifdef __GNUC__
__attribute__((visibility("default")))
#endif
int luaopen_volubile(lua_State *lua)
{
   if (luaL_newmetatable(lua, MN_MT)) {
      lua_pushcfunction(lua, vb_lua_free);
      lua_setfield(lua, -2, "__gc");
      lua_pushvalue(lua, -1);
      lua_setfield(lua, -2, "__index");
   }
   lua_pushcfunction(lua, vb_lua_match);
   lua_setfield(lua, -2, "match");

   const luaL_Reg lib[] = {
      {"load", vb_lua_load},
      {"match", vb_lua_match},
      {NULL, NULL},
   };
   luaL_newlib(lua, lib);

   lua_pushstring(lua, VB_VERSION);
   lua_setfield(lua, -2, "VERSION");
   lua_pushnumber(lua, VB_MAX_PAGE_SIZE);
   lua_setfield(lua, -2, "MAX_PAGE_SIZE");
   return 1;
   
   (void)fc_memo_compute;  /* Shut up compiler. */
}
