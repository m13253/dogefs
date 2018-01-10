.PHONY: all clean

CXX = clang++
CXXFLAGS = -g -std=gnu++11 -Wall -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=29 $(shell pkg-config fuse --cflags)
LIBS = $(shell pkg-config fuse --libs)

all: mount.dogefs

clean:
	rm -f mount.dogefs

mount.dogefs: main.cpp ../common/types.h ../common/utils.h
	$(CXX) $(CXXFLAGS) -o $@ $< $(LIBS)

bootsect.bin: bootsect.s
	nasm -f bin -o $@ $<
