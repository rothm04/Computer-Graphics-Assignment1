main.o: main.cpp stb_image.h
	g++ main.cpp -I. -g

all: main.o

.PHONY: clean
clean:
	rm -rf a.out