main.o: main.cpp stb_image.h
	g++ main.cpp -I. -g -Wall -Wextra

all: main.o

.PHONY: clean
clean:
	rm -rf a.out
	rm -f ./imagesOut/*
	rm -f ./assignment/*