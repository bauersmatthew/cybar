all:
	g++ -std=c++14 -lX11 -lXft -lasound -lpthread -I/usr/include/freetype2 -O2 -o bin/cybar src/cybar.cpp
.PHONY: all

debug:
	g++ -std=c++14 -lX11 -lXft -lasound -lpthread -I/usr/include/freetype2 -g -D DEBUG -o bin/cybar src/cybar.cpp
.PHONY: all

install: bin/cybar
	cp bin/cybar /usr/bin/
.PHONY: install
