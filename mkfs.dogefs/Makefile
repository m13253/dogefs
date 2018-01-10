.PHONY: all clean

CXX = clang++
CXXFLAGS = -g -std=gnu++11 -Wall -D_FILE_OFFSET_BITS=64

all: mkfs.dogefs

clean:
	rm -f mkfs.dogefs

mkfs.dogefs: main.cpp ../common/types.h ../common/utils.h
	$(CXX) $(CXXFLAGS) -o $@ $< $(LIBS)

bootsect.bin: bootsect.s
	nasm -f bin -o $@ $<
