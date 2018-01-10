.PHONY: all clean

CXXFLAGS = -std=gnu++11 -D_FILE_OFFSET_BITS=64 -g

all: mkdogefs

clean:
	rm -f mkdogefs

mkdogefs: main.cpp ../common/types.h ../common/utils.h
	$(CXX) $(CXXFLAGS) -o $@ $< $(LIBS)
