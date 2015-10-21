CC=gcc
CFLAGS=-march=native -funroll-loops -fno-schedule-insns -O3 -fomit-frame-pointer -Wno-unused-result -s

all: tapio xorbox secretbox

tapio: tapio.c strannex.h writestr.h strannex.c
	$(CC) $(CFLAGS) -o tapio tapio.c strannex.c
	ln -sf tapio tunio
	
xorbox: xorbox.c writestr.h
	$(CC) $(CFLAGS) -o xorbox xorbox.c writestr.c
	
secretbox: secretbox.c writestr.c strannex.c writestr.h strannex.h
	$(CC) $(CFLAGS) -I libsodium-1.0.3/src/libsodium/include/sodium -o secretbox secretbox.c writestr.c libsodium-1.0.3/src/libsodium/.libs/libsodium.a

clean:
	rm -fv tapio tunio xorbox secretbox
