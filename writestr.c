#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "writestr.h"

void writeln_uint(int fd, unsigned long value)
{
    write_uint(fd,value);
    write_cstr(fd,"\n");
}

void write_str_uint(int fd, char *str, unsigned long value)
{
    if (str) write_str(fd,str);
    write_uint(fd,value);
}

void writeln_str_uint(int fd, char *str, unsigned long value)
{
    if (str) write_str(fd,str);
    writeln_uint(fd,value);
}

void write_uint(int fd, unsigned long value)
{
    char buf[10],*end;
    end=utoa_nz(value,buf,-10);
    write(fd,buf,end-buf)!=0;
}

char *utoa_nz(unsigned long value, char *out, int ndigits)
{
    return utoa_radix_nz(value,out,10,ndigits);
}

char *utoa_radix(unsigned long value, char *out, int radix, int ndigits)
{
    char *p=utoa_radix_nz(value,out,radix,ndigits);
    *p=0;
    return p;
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
