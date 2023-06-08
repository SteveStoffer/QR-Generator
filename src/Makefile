CC=g++
CFLAGS=-std=c++17 -g
PROGRAMS=qr_generator


qr_generator: qr_generator.o qr.o
	$(CC) qr_generator.o qr.o -o $(PROGRAMS) $(CFLAGS)

qr_generator.o: qr_generator.cc
	$(CC) -c qr_generator.cc $(CFLAGS)

qr.o: qr.cc
	$(CC) -c qr.cc $(CFLAGS)

clean:
	rm -r $(PROGRAMS) *.o