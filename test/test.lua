#!/usr/bin/env lua

package.cpath = "../lua/?.so"

local seed = arg[1] and tonumber(arg[1]) or os.time()
print("seed", seed)
math.randomseed(seed)

local MODES = {
	"exact",
	"prefix",
	"substr",
	"suffix",
	"glob",
	"levenshtein",
	"damerau",
	"lcsubstr",
   "lcsubseq",
}

local LEXICON_TEXT = "lexicon.txt"
local LEXICON_MINI = "lexicon.mn"
local BAD_UTF8     = "bad_utf8.mn"  -- abc, d\xfff, ghi

local vb = require("volubile")

-- Chooses a random token from the lexicon.
local function choose_tokens()
	local words = {}
	for line in io.lines(LEXICON_TEXT) do
		words[#words + 1] = line
	end
	local token = words[math.random(#words) + 1]
	-- Make sure that we get some matches.
	local tokens = {
	   exact = token,
	   prefix = token:sub(1, math.random(#token)),
	   suffix = token:sub(math.random(#token)),
	   levenshtein = token,
	   damerau = token,
	   lcsubstr = token,
	   lcsubseq = token,
	}
	local x, y = math.random(#token), math.random(#token)
	if x < y then x, y = y, x end
	tokens.substr = token:sub(x, y)
	-- Prevent glob pattern simplifications so that the glob matching function
	-- is tested.
	tokens.glob = token:sub(1, x) .. "*" .. token:sub(x + 1)
	return tokens, #words
end

-- Chooses a random page size (> 0).
local function choose_page_size()
	return math.random(vb.MAX_PAGE_SIZE)
end

local function gather_matches(lexicon, tokens, mode, page_size, num_words)
	local matches = {}
	local last_pos, last_weight
	while true do
	   local params = {
	      mode = mode,
	      page_size = page_size,
	      last_pos = last_pos,
	      last_weight = last_weight,
	   }
	   local ret = lexicon:match(tokens[mode], params)
	   for _, word in ipairs(ret) do
	      assert(not matches[word])
	      matches[word] = true
	      table.insert(matches, word)
	      assert(#matches <= num_words)
	   end
	   if ret.last_page then
	      -- The user is not supposed to try to fetch the next page if he knows
	      -- that the current one is the last one, but we should still not
	      -- return results if he does.
	      params.last_pos, params.last_weight = ret.last_pos, ret.last_weight
	      for i = 1, 10 do
	         ret = lexicon:match(tokens[mode], params)
	         assert(ret.last_page and #ret == 0)
	      end
	      break
	   end
	   last_pos, last_weight = ret.last_pos, ret.last_weight
	end
	return table.concat(matches, "\n")
end

local test = {}

function test.zero()
   local lexicon = assert(vb.load(LEXICON_MINI))
   local tokens = choose_tokens()
   for _, mode in ipairs(MODES) do
      -- Should not break when page_size = 0
      local ret = lexicon:match(tokens[mode], {mode = mode, page_size = 0})
      assert(#ret == 0 and not ret.has_next)
      -- Should not break when token = ""
      local ret = lexicon:match("", {mode = mode, page_size = 0})
      assert(#ret == 0 and not ret.has_next)
   end
end

-- Ensures pagination works as expected by using different page length
-- and checking that we obtain the same results each time.
function test.pagination()
   local lexicon = assert(vb.load(LEXICON_MINI))
	local tokens, num_words = choose_tokens()
	for _, mode in ipairs(MODES) do
		print(mode)
		local size1 = choose_page_size()
		local size2 = choose_page_size()
		local ret1 = gather_matches(lexicon, tokens, mode, size1, num_words)
		local ret2 = gather_matches(lexicon, tokens, mode, size2, num_words)
		if ret1 ~= ret2 then
			print("FAIL", token[mode], mode, size1, size2)
			print("=========\n" .. ret1)
			print("=========\n" .. ret2)
		end
		assert(ret1 == ret2)
	end
end

function test.bad_utf8()
   local lexicon = assert(vb.load(BAD_UTF8))
   assert(not pcall(lexicon.match, lexicon, "d?*"))
   assert(not pcall(lexicon.match, lexicon, "?\xff?", {mode = "glob"}))
   assert(not pcall(lexicon.match, lexicon, "d\xff?", {mode = "levenshtein"}))
end

local tmp = {}
for name in pairs(test) do table.insert(tmp, name) end
table.sort(tmp)
for _, name in ipairs(tmp) do
   print(name)
   test[name]()
end
