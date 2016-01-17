PREFIX = /usr/local
LUA_VERSION = 5.2

CFLAGS = -I/usr/include/lua$(LUA_VERSION)
CFLAGS += -std=c11 -fPIC -shared -g -Wall -Werror -fvisibility=hidden
CFLAGS += -O2 -DNDEBUG -march=native -mtune=native -fomit-frame-pointer

SOURCES = volubile.c ../volubile.c ../src/lib/faconde.c ../src/lib/mini.c

LIB = volubile.so

all: $(LIB)

clean:
	rm -f $(LIB)

install: $(LIB)
	install -pm 0755 $(LIB) $(PREFIX)/lib/lua/$(LUA_VERSION)/$(LIB)

uninstall:
	rm -f $(PREFIX)/lib/lua/$(LUA_VERSION)/$(LIB)

.PHONY: all clean install uninstall

$(LIB): ../volubile.h $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o $@
