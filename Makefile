CC ?= gcc
ifeq ($(CC), cc)
	CC := gcc
endif

CFLAGS ?= -march=native -funroll-loops -fno-schedule-insns -O3 -fomit-frame-pointer -Wno-unused-result -s

UNAME := $(shell uname -m)
COMP  := none
ifeq ($(CC), gcc)
	ifeq ($(UNAME), x86_64)
		COMP:=x86_64
	endif
endif

LIBSODIUM += -I libsodium/include
LIBSODIUM += libsodium/sodium/core.c
LIBSODIUM += libsodium/sodium/utils.c
LIBSODIUM += libsodium/sodium/runtime.c
LIBSODIUM += libsodium/randombytes/randombytes.c
LIBSODIUM += libsodium/randombytes/sysrandom/randombytes_sysrandom.c
LIBSODIUM += libsodium/crypto_secretbox/xsalsa20poly1305/ref/box_xsalsa20poly1305.c
LIBSODIUM += libsodium/crypto_onetimeauth/poly1305/onetimeauth_poly1305_try.c
LIBSODIUM += libsodium/crypto_onetimeauth/poly1305/donna/auth_poly1305_donna.c
LIBSODIUM += libsodium/crypto_onetimeauth/poly1305/donna/verify_poly1305_donna.c
LIBSODIUM += libsodium/crypto_onetimeauth/poly1305/onetimeauth_poly1305.c
LIBSODIUM += libsodium/crypto_verify/16/ref/verify_16.c
LIBSODIUM += libsodium/crypto_stream/xsalsa20/ref/xor_xsalsa20.c
LIBSODIUM += libsodium/crypto_stream/xsalsa20/ref/stream_xsalsa20.c
LIBSODIUM += libsodium/crypto_core/hsalsa20/ref2/core_hsalsa20.c
ifeq ($(COMP), x86_64)
	LIBSODIUM += libsodium/crypto_stream/salsa20/amd64_xmm6/stream_salsa20_amd64_xmm6.S
else
	LIBSODIUM += libsodium/crypto_stream/salsa20/ref/stream_salsa20_ref.c
	LIBSODIUM += libsodium/crypto_stream/salsa20/ref/xor_salsa20_ref.c
	LIBSODIUM += libsodium/crypto_core/salsa20/ref/core_salsa20.c
endif

all: tapio xorbox secretbox

tapio: tapio.c strannex.h writestr.h strannex.c
	$(CC) $(CFLAGS) -o tapio tapio.c strannex.c
	ln -sf tapio tunio

xorbox: xorbox.c writestr.h
	$(CC) $(CFLAGS) -o xorbox xorbox.c writestr.c

secretbox: secretbox.c writestr.c strannex.c writestr.h strannex.h
	$(CC) $(CFLAGS) $(LIBSODIUM) -o secretbox secretbox.c writestr.c 

clean:
	rm -fv tapio tunio xorbox secretbox
