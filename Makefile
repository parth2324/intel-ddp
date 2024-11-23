CC=gcc
# CFLAGS=-std=gnu99 #Uncomment if using GCC versions older than 5 (where it may otherwise throw some errors)

all: latency_test augrey_test

util.o: util.c util.h
	$(CC) $(CFLAGS) -c $<

%.o: %.c util.h
	$(CC) $(CFLAGS)  -c $<

latency_test: latency_test.o util.o
	$(CC) $(CFLAGS) $^ -o $@

augrey_test: augrey_test.o util.o
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f *.o *~ OUT_*.txt latency_test augrey_test
