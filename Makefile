CC ?= gcc
ifeq ($(CC), cc)
	CC := gcc
endif

CFLAGS ?= -funroll-loops -fno-schedule-insns -O3 -fomit-frame-pointer -Wno-unused-result -s

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

all: tapio xorbox secretbox unbundle compressbox
	tar cvzf vpnbox.tgz --owner=root --group=root tapio tunio xorbox secretbox unbundle compressbox

tapio: tapio.c strannex.h strannex.c
	$(CC) $(CFLAGS) -o tapio tapio.c strannex.c
	ln -sf tapio tunio

xorbox: xorbox.c strannex.h pipe_handle.o
	$(CC) $(CFLAGS) -o xorbox xorbox.c strannex.c pipe_handle.o

secretbox: secretbox.c strannex.c strannex.h pipe_handle.o
	$(CC) $(CFLAGS) $(LIBSODIUM) -o secretbox secretbox.c strannex.c pipe_handle.o

unbundle: unbundle.c pipe_handle.o
	$(CC) $(CFLAGS) -o unbundle unbundle.c pipe_handle.o

compressbox: compressbox.c /usr/share/lzo/minilzo/minilzo.c strannex.h strannex.c pipe_handle.o
	$(CC) $(CFLAGS) -I /usr/share/lzo/minilzo -o compressbox compressbox.c /usr/share/lzo/minilzo/minilzo.c strannex.c pipe_handle.o

pipe_handle.o: pipe_handle.c pipe_handle.h
	$(CC) $(CFLAGS) -c pipe_handle.c

clean:
	rm -fv vpnbox.tgz tapio tunio xorbox secretbox unbundle compressbox pipe_handle.o
