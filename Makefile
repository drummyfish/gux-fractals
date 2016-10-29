all:
	c++ -o main -Wall -pedantic main.cpp `pkg-config --libs --cflags gtk+-3.0`
