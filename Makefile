all:
	c++ -o main main.cpp `pkg-config --libs --cflags gtk+-3.0`
