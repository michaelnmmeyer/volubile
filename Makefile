CFLAGS = -std=c11 -g -Wall -Werror
CFLAGS += -O2 -DNDEBUG -march=native -mtune=native -fomit-frame-pointer -s

AMALG = volubile.h volubile.c

VALGRIND = valgrind --leak-check=full --error-exitcode=1

#--------------------------------------
# Abstract targets
#--------------------------------------

all: $(AMALG)

check: lua/volubile.so test/test_parse
	cd test && $(VALGRIND) bash ./test_parse.sh
	cd test && $(VALGRIND) lua test.lua

.PHONY: all check


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

test/test_parse: test/test_parse.c $(AMALG) 
	$(CC) $(CFLAGS) $< src/parse.c -o $@
