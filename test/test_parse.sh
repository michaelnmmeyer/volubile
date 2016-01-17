#!/usr/bin/env sh

set -e

while read before _ _; do
   ./test_parse "$before";
done < parse.tsv | cmp parse.tsv;

exit 0
