-- Find words similar to "useful" in the accompanying sample lexicon. Iterate
-- over all results pages, one by one.

local vb = require("volubile")

local lexicon = vb.load("example_lexicon.dat")

local query = "@useful"
local params = {}

local page_no = 0

while true do
   local matches = lexicon:match(query, params)
   if #matches > 0 then
      page_no = page_no + 1
      print(string.format("=> PAGE %d", page_no))
      for _, word in ipairs(matches) do
         print(word)
      end
   end
   if matches.last_page then break end
   params.last_pos, params.last_weight = matches.last_pos, matches.last_weight
end
