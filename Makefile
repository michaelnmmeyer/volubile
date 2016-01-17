CFLAGS = -std=c11 -g -Wall -Werror -DNDEBUG -Wno-unused-function
AMALG = volubile.h volubile.c

VALGRIND = valgrind --leak-check=full --error-exitcode=1

#--------------------------------------
# Abstract targets
#--------------------------------------

all: $(AMALG) example

check: lua/volubile.so test/test_parse test/test_heap
	cd test && $(VALGRIND) bash ./test_parse.sh
	cd test && $(VALGRIND) lua test_lib.lua
	cd test && $(VALGRIND) ./test_heap

clean:
	rm -f example lua/volubile.so test/test_parse test/test_heap

.PHONY: all check clean


#--------------------------------------
# Concrete targets
#--------------------------------------

volubile.h: src/api.h
	cp $< $@

volubile.c: $(wildcard src/*.h src/*.c)
	src/mkamalg.py src/*.c > $@

src/parse.c: src/parse.rl
	ragel -e $< -o $@

lua/volubile.so: $(AMALG) lua/volubile.c
	$(MAKE) -C lua

example: example.c $(AMALG) src/lib/faconde.c src/lib/mini.c
	$(CC) $(CFLAGS) $< volubile.c src/lib/faconde.c src/lib/mini.c -o $@

test/%: test/%.c $(AMALG) 
	$(CC) $(CFLAGS) $< src/parse.c -o $@
