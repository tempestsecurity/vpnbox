#include <string.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#define _GNU_SOURCE
#include <linux/fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>

#define write_str(fd,str)    write((fd),(str),strlen(str))
#define write_cstr(fd,str)   write((fd),(str),sizeof(str)-1)

#define BUF_SIZE	4096
#define MAX_KEY_SIZE	512

void xor_func(unsigned char *buf, int size, unsigned char *key, int ksize)
{
    int i=0;
    while (--size >= 0) {
        buf[size] ^= key[i];
        if (++i==ksize) i=0;
    }
}

void parse_key(unsigned char *k, int *ksize, char *str)
{
    unsigned char i, c=2, *key = k;

    *ksize = 0;

    if (memcmp(str, "0x", 2)==0) str += 2;

    while (*str) {
        for (c = 2, i = 0 ; c-- ; str++) {
            i <<= 4;
            if (*str >= '0' && *str <= '9') {
                i |= *str - '0';
            } else if (*str >= 'A' && *str <= 'F') {
                i |= *str - 'A' + 10;
            } else if (*str >= 'a' && *str <= 'f') {
                i |= *str - 'a' + 10;
            } else {
                return;
            }
        }
        *key++ = i;
        if (++(*ksize) == MAX_KEY_SIZE) break;
    }
}

int main(int argc, char **argv)
{
    unsigned char key[MAX_KEY_SIZE];
    int ksize = 0, c;
    char buf[BUF_SIZE];
    int fd, r, pipe_in[2], pipe_out[2];
    struct stat fd_stat;

    if (argc < 2) {
        write_cstr(STDERR_FILENO,"params missing\n");
        return -1;
    }

    while ( (c=getopt(argc,argv,"k:K:")) != -1 ) {
        switch (c) {
            case 'k':
                parse_key(key, &ksize, optarg);
                if (!ksize) {
                    write_cstr(STDERR_FILENO,"error parsing key.\n");
                    return -1;
                }
                break;
            case 'K':
                fd = open(optarg, O_RDONLY);
                if (fd < 0) {
                    write_cstr(STDERR_FILENO,"error opening key file.\n");
                    return -1;
                }
                fstat(fd, &fd_stat);
                fd_stat.st_mode &= 0x3FFF;
                if (fd_stat.st_mode != S_IRUSR) {
                    write_cstr(STDERR_FILENO,"error key file permission\n");
                    return -1;
                }
                if ((r = read (fd, buf, sizeof(buf)-1)) <= 0) {
                    write_cstr(STDERR_FILENO,"error reading key file.\n");
                    return -1;
                }
                close (fd);
                buf[r] = '\0';
                parse_key(key, &ksize, buf);
                break;
        }
    }

    argv += optind;

    if (!ksize || !argv) {
        write_cstr(STDERR_FILENO,"error invalid usage.\n");
        return -1;
    }

    if (pipe2(pipe_in,  O_DIRECT) == -1) pipe(pipe_in);
    if (pipe2(pipe_out, O_DIRECT) == -1) pipe(pipe_out);

    r = fork();
    switch(r) {
        case -1:
            write_cstr(STDERR_FILENO,"unable to fork.\n");
            return(-1);
        case 0:
            if (dup2(pipe_in[STDIN_FILENO], STDIN_FILENO) == -1) {
                write_cstr(STDERR_FILENO,"unable to move descriptor to stdin.\n");
                return(-1);
            }
            close(pipe_in[STDOUT_FILENO]);
            if (dup2(pipe_out[STDOUT_FILENO], STDOUT_FILENO) == -1) {
                write_cstr(STDERR_FILENO,"unable to move descriptor to stdout.\n");
                return(-1);
            }
            close(pipe_out[STDIN_FILENO]);
            return(execv(argv[0], argv));
    }
    close(pipe_in [STDIN_FILENO]);
    close(pipe_out[STDOUT_FILENO]);

    r = fork();
    switch(r) {
        case -1:
            write_cstr(STDERR_FILENO,"unable to fork.\n");
            return(-1);
        case 0:
            close(pipe_out[STDIN_FILENO]);
            close(STDOUT_FILENO);
            for(;;) {
                r = read(STDIN_FILENO, buf, BUF_SIZE);
                if (r > 0) {
                    xor_func(buf, r, key, ksize);
                    write(pipe_in[STDOUT_FILENO], buf, r);
                } else {
                    break;
                }
            }
            return 0;
    }
    close(pipe_in[STDOUT_FILENO]);
    close(STDIN_FILENO);

    for(;;) {
        r = read(pipe_out[STDIN_FILENO], buf, BUF_SIZE);
        if (r > 0) {
            xor_func(buf, r, key, ksize);
            write(STDOUT_FILENO, buf, r);
        } else {
            break;
        }
    }
    return 0;
}
