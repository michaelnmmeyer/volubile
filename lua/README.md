# volubile-lua

Lua binding of `volubile`.

## Building

Check the value of the variable `LUA_VERSION` in the makefile in this directory.
Then invoke the usual:

    $ make && sudo make install

You can also pass in the correct version number on the command-line:

    $ make LUA_VERSION=5.3 && sudo make install LUA_VERSION=5.3


## Documentation

There is a concrete usage example in the file `example.lua` in this directory.
The following is a more formal description.

### Constants

`volubile.VERSION`  
The library version.

`volubile.MAX_PAGE_SIZE`  
Maximum allowed page size.

### Functions

`volubile.load(lexicon_path)`  
Loads a lexicon from a file. Returns a lexicon object on success, `nil` plus an
error message otherwise. The returned lexicon object is compatible with the
lexicon type used by the [`mini`
library](https://github.com/michaelnmmeyer/mini/tree/master/lua). Lexica can be
created only with this last library, not with `volubile`.

`volubile.match(lexicon, query_string[, params])`  
Query a lexicon.  
The `params` argument, if given, must be a table. The following fields will be
checked:
* `mode`. Matching mode to use. This must be a string. The following are
  available:
   * `auto`. Detect the matching mode automatically.
   * `exact`. Exact matching.
   * `prefix`. Prefix matching
   * `substr`. Substring matching.
   * `suffix`. Suffix matching
   * `glob`. Glob pattern matching.
   * `levenshtein`. Levenshtein distance
   * `damerau`. Damerau-Levenshtein distance.
   * `lcsubstr`. Longest common substring.
   * `lcsubseq`. Longest common subsequence.
  Defaults to `auto`.
* `page_size`. Number of words to return per page. Defaults to 10.
* `max_dist`. Maximum allowed edit distance. This only applies to the
  `levenshtein` and `damerau` matching modes. Defaults to 3.
* `prefix_len`: Length, in code points, of the prefix that must be shared
  between the query word and some given word in the lexicon for the second to be
  considered a potential match. This concerns fuzzy matching search modes, at
  the exception of `lcsubstr`. Increasing this value accelerates fuzzy
  matching, but decreases recall. 1 or 2 is fine; higher values are likely to be
  harmful. Defaults to 1.
* `last_pos`, `last_weight`. Pagination informations, encoded as two numbers.

Returns a table containing the matching words. This table also contains three
fields in its hash part:
* `last_page`. Whether the last results page was just returned. If `true`,
   calling `volubile.match` again won't yield new results.
* `last_pos`, `last_weight`. Two numbers that encode informations necessary for
  paginating results. To obtain the next results page for a given query, set
  these field in the `params` table and issue the same query again, with
  otherwise identical parameters.
