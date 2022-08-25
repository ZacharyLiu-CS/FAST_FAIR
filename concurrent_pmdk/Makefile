.PHONY: all clean
.DEFAULT_GOAL := all

LIBS=-lrt -lm -pthread -lpmemobj
INCLUDES=-I./include
CFLAGS=-O0 -std=c++11 -g

output = btree_concurrent btree_concurrent_mixed

all: main

main: src/test.cpp
	g++ ${CFLAGS} -c src/btree.cc
	ar -r libfastfair.a btree.o
	g++ $(CFLAGS) -o btree_concurrent src/test.cpp libfastfair.a $(LIBS) -DCONCURRENT
	g++ $(CFLAGS) -o btree_concurrent_mixed src/test.cpp  libfastfair.a $(LIBS) -DCONCURRENT -DMIXED
	
clean: 
	rm $(output)
	rm *.a
	rm *.o
