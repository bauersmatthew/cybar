all:
	g++ -std=c++14 -lX11 -lXft -lasound -lpthread -I/usr/include/freetype2 -O2 -o bin/cybar src/cybar.cpp

