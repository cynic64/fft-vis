
CC=gcc
FLAGS=-shared -Wall -pedantic -O3 -lm -fPIC -lfftw3f -g

all: fft-vis.c
	$(CC) $(FLAGS) fft-vis.c -o fft-vis.so 

install: fft-vis.so
	cp fft-vis.so /usr/lib64/ladspa/fft-vis.so

uninstall: /usr/lib64/ladspa/fft-vis.so
	rm -f /usr/lib64/ladspa/fft-vis.so

clean:
	rm -f fft-vis.so
