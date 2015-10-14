all:
	gcc -march=native -funroll-loops -fno-schedule-insns -O3 -fomit-frame-pointer -Wno-unused-result -s -o xorbox xorbox.c
	gcc -march=native -funroll-loops -fno-schedule-insns -O3 -fomit-frame-pointer -I libsodium-1.0.3/src/libsodium/include/sodium -Wno-unused-result -s -o secretbox secretbox.c libsodium-1.0.3/src/libsodium/.libs/libsodium.a

clean:
	rm -fv xorbox secretbox
