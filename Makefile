CC=gcc
CFLAGS=-march=native -funroll-loops -fno-schedule-insns -O3 -fomit-frame-pointer -Wno-unused-result -s

all:
	$(CC) $(CFLAGS) -o tapio tapio.c strannex.c
	$(CC) $(CFLAGS) -o xorbox xorbox.c
	$(CC) $(CFLAGS) -I libsodium-1.0.3/src/libsodium/include/sodium -o secretbox secretbox.c libsodium-1.0.3/src/libsodium/.libs/libsodium.a

clean:
	rm -fv tapio xorbox secretbox
