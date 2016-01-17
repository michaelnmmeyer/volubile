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

## Usage

### Example

There is a concrete usage example in the file
[`example.c`](https://raw.githubusercontent.com/michaelnmmeyer/volubile/master/example.c)
in this directory. It searches the lexicon encoded in `example_lexicon.dat`.
Compile the example program with `make`, and use it like this:

    $ ./example '@redundant'   # find words similar to "redundant"
    redundant
    redundancy
    redundantly
    redounding
    refunding
    => [76155 3]

Only the five words most similar to "`redundant`" are shown, plus two numbers
that encode informations necessary to access the next results page. We can
display the next five words by issuing the same query together with these
numbers:

    ./example '@redundant' 76155 3
    reluctant
    remnant
    repentant
    repugnant
    resonant
    => [77312 3]

This process can be repeated again to access the remaining matching words.

### Creating a lexicon

To search inside a lexicon, you must first encode it as a numbered automaton in
the [`mini`](https://github.com/michaelnmmeyer/mini) format. This can be done
with the `mini` command-line tool, or through the corresponding programmatic
interface. With the command-line tool, you can create a lexicon with a command
like the following:

    $ mini create -t numbered lexicon.dat < /usr/share/dict/words

Note the use of the `-t` switch.

## Query syntax

### Matching mode selectors

Several string matching modes are available. When the selected one is `VB_AUTO`,
a few characters have a special meaning when they appear at the beginning of a
query:

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

Glob characters are only given a special meaning when the selected matching made
is `VB_AUTO` or `VB_GLOB`. Otherwise, they are interpreted as literals.

The characters `[`, `?`, and `*` are interpreted as literals when in a group.
The character `]`, to be used in a group, must be placed in first position. The
character `^`, if included in a group and intended to be interpreted as a
literal, must not be placed at the beginning of the group. The character `]`, if
not preceded by `[`, is interpreted as a literal.
