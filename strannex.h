#ifndef __STRANNEX_H__
    #define __STRANNEX_H__
    char *_strannex(char *dst, char *src, char *dstend);
    char *_strannex_uint(char *dst, unsigned long value, char *dstend);

    #define strstart(dst)      char *dst##__p, *dst##__q
    #define strlimit(dst,max)  dst##__p = (dst); dst##__q = (dst) + (max) - 1; *dst##__p = 0
    #define strarray(dst)      strlimit(dst,sizeof(dst))
    #define strannex(dst,src)  dst##__p = _strannex(dst##__p,(src),dst##__q)
    #define strannex_uint(dst,src)  dst##__p = _strannex_uint(dst##__p,(src),dst##__q)

    #ifdef __DIETREFDEF_H__
        #ifdef __x86_64__
            #define rawmemchr(s,c) memchr(s, c, 0x7FFFFFFFFFFFFFFF)
        #else  // __x86_64__
            #define rawmemchr(s,c) memchr(s, c, 0x7FFFFFF)
        #endif // __x86_64__
    #else  // __DIETREFDEF_H__
        #ifdef __i386__
            void *rawmemchr(const void *s, int c);
        #else  // __i386__ not intel, maybe cross compiling
            #define rawmemchr(s,c) memchr(s, c, 0x7FFFFFF)
        #endif // __i386__
    #endif // __DIETREFDEF_H__

    #ifndef my_strlen
    #define my_strlen(a)                   (((unsigned long long)rawmemchr(a, '\0')) - ((unsigned long long)a))
    #endif

    #define write_cstr(fd,str)             write((fd),(str),sizeof(str)-1)
    #define writeln_cstr(fd,str)           write((fd),(str),sizeof(str)-1); write(fd,"\n",1)
    #define write_str(fd,str)              write((fd),(str),my_strlen(str))
    #define writeln_str(fd,str)            write_str((fd),(str)); write(fd,"\n",1)

    #define writeln_str_int(fd,str,value)  write_str((fd),(str)); write_int(fd,value); write(fd,"\n",1)
    #define writeln_int(fd,value)          write_int(fd,value); write(fd,"\n",1)
    #define write_str_int(fd,str,value)    write_str((fd),(str)); write_int(fd,value)

    #define writeln_str_uint(fd,str,value) write_str((fd),(str)); write_uint(fd,value); write(fd,"\n",1)
    #define writeln_uint(fd,value)         write_uint(fd,value); write(fd,"\n",1)
    #define write_str_uint(fd,str,value)   write_str((fd),(str)); write_uint(fd,value)

    char *utoa_radix_nz(unsigned long value, char *out, int radix, int ndigits);
    void write_int(int fd, int value);
    void write_uint(int fd, unsigned long value);
#endif // __STRANNEX_H__
