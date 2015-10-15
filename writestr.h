#ifndef WRITESTR_H
#define WRITESTR_H
    void writeln_uint(int fd, unsigned long value);
    void write_str_uint(int fd, char *str, unsigned long value);
    void writeln_str_uint(int fd, char *str, unsigned long value);
    void write_uint(int fd, unsigned long value);
    char *utoa_nz(unsigned long value, char *out, int ndigits);
    char *utoa_radix(unsigned long value, char *out, int radix, int ndigits);
    char *utoa_radix_nz(unsigned long value, char *out, int radix, int ndigits);

    #define write_cstr(fd,str)              write((fd),(str),sizeof(str)-1)!=0
    #define write_cstr_uint(fd,str,value)   write_cstr(fd,str); write_uint(fd,value);
    #define writeln_cstr_uint(fd,str,value) write_cstr_uint(fd,str,value); write(fd,"\n",1)!=0
    #define writeln_cstr(fd,str)            write_cstr((fd),(str)); write(fd,"\n",1)!=0
    #define write_str(fd,str)               write((fd),(str),strlen(str))!=0
    #define writeln_str(fd,str)             write_str((fd),(str)); write(fd,"\n",1)!=0
#endif
