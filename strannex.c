#include <stdio.h>
#include "strannex.h"

char *_strannex(char *dst, char *src, char *dstend)
{
    if (!dst || !src || !dstend || dst>dstend) return NULL;
    // skip to the end of the string
    while (*dst) {
         if (++dst>dstend) return NULL;
    }
    // Append, breaking if past end
    while (*src) {
       if (dst>=dstend) break;
       *dst++ = *src++;
    }
    // add terminator
    *dst = 0;
    // return where it last touched
    return dst;
}

char *_strannex_uint(char *dst, unsigned long value, char *dstend)
{
char *p, *src, buf[10];
unsigned size;

    src = buf;
    p=utoa_radix_nz(value,src,10,-10);
    *p = 0;

    if (!dst || !src || !dstend || dst>dstend) return NULL;
    // skip to the end of the string
    while (*dst) {
         if (++dst>dstend) return NULL;
    }
    // Append, breaking if past end
    while (*src) {
       if (dst>=dstend) break;
       *dst++ = *src++;
    }
    // add terminator
    *dst = 0;
    // return where it last touched
    return dst;
}

char *utoa_radix_nz(unsigned long value, char *out, int radix, int ndigits)
{
    if (!out || radix>36) return NULL;
    char *p=out;
    if (value) {
        while (value) {
            char d = value % radix;
            value /= radix;
            d += (d>9) ? 'A'-10 : '0';
            *p++ = d;
            if (ndigits<0) ndigits++;
            if (ndigits>0) ndigits--;
            if (!ndigits) break;
        }
    } else {
        if (ndigits<1) ndigits=1;
    }
    while (ndigits-->0) *p++='0';
    char *end=p;
    p--;
    while (out<p) {
        char tmp=*out;
        *out++=*p;
        *p--=tmp;
    }
    return end;
}

void write_int(int fd, int value)
{
char *p, str[11];
unsigned size;

    p=str;
    if (value < 0) {
        *p++ = '-';
        value *= -1;
    }
    p=utoa_radix_nz((unsigned long)value,p,10,-10);
    size = p - str;
    write(fd,str,size);
}

void write_uint(int fd, unsigned long value)
{
char *p, str[10];
unsigned size;

    p=utoa_radix_nz(value,str,10,-10);
    size = p - str;
    write(fd,str,size);
}
