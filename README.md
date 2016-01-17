# volubile

A dictionary search engine


## Purpose

This is a kind of search engine for dictionaries. It allows searching words in a
dictionary with approximate matching (aka fuzzy matching) and pattern matching.
In response to a query, it returns a list of matching words, sorted by
decreasing similarity to the query. Informations necessary for paginating
results efficiently are also provided to the caller.

Several fuzzy matching metrics are supported:
* Levenshtein distance
* Damerau-Levenshtein distance
* Longest common substring
* Longest common subsequence

## Building

The library is available in source form, as an amalgamation. Compile
[`volubile.c`](https://raw.githubusercontent.com/michaelnmmeyer/volubile/master/volubile.c)
together with your source code, and use the interface described in
[`volubile.h`](https://raw.githubusercontent.com/michaelnmmeyer/volubile/master/volubile.h).
The following must also be compiled with your code:
* [`mini.c`](https://raw.githubusercontent.com/michaelnmmeyer/mini/master/mini.c)
* [`faconde.c`](https://raw.githubusercontent.com/michaelnmmeyer/faconde/master/faconde.c)

These are C11 source files, so you must use a modern C compiler, which means
either GCC or CLang on Unix.

## Query syntax

### Matching mode selectors

A few characters have a special meaning when in leading position:

    #apple   find words that have "apple" as a substring; equivalent to the
             pattern *apple*
    +apple   find words that have the longest possible substring in common with
             "apple"
    @apple   find words that resemble "apple" (using the Damerau-Levenshtein
             distance as metric)

The Levenshtein distance and longest common subsequence metrics can only be
selected programmatically.

### Glob matching

Glob matching is case-sensitive, and is performed over whole words. The
supported syntax is as follows:

    ?         matches a single character
    *         matches zero or more characters
    [abc]     matches any of the characters a, b, or c
    [^abc]    matches any character but a, b, and c

Character classes are not supported.

The characters `[`, `?`, and `*` are interpreted as literals when in a group.
The character `]`, to be used in a group, must be placed in first position. The
character `^`, if included in a group and intended to be interpreted as a
literal, must not be placed at the beginning of the group. The character `]`, if
not preceded by `[`, is interpreted as a literal.
