#include <string.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#define _GNU_SOURCE
#include <linux/fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "strannex.h"

#define MULT		4
#define BUF_SIZE	4096

#include <stdio.h>
void signalHandler(int signal)
{
    kill(getppid(), SIGPIPE);
    exit (0);
}

int main(int argc, char **argv)
{
    int cr = 0, cs = 0;
    char buf[BUF_SIZE];
    char fifo[MULT*BUF_SIZE], *progname;
    char *head  = fifo, *tail  = fifo;
    char *head2 = NULL;
    unsigned int len2 = 0;
    int fd, r, pipe_in[2], pipe_out[2];
    struct rlimit rlim;

    if ((progname = strrchr(argv[0], '/')) == NULL) {
        progname = argv[0];
    } else {
        progname++;
    }

    if (argc < 2) {
        write_str(STDERR_FILENO, progname);
        write_cstr(STDERR_FILENO,": command missing\n");
        return -1;
    }

    if (pipe2(pipe_in,  O_DIRECT) == -1) {
        write_str(STDERR_FILENO, progname);
        write_cstr(STDERR_FILENO,": Warning, pipe2 pipe_in failed, using pipe.\n");
        pipe(pipe_in);
    }
    if (pipe2(pipe_out, O_DIRECT) == -1) {
        write_str(STDERR_FILENO, progname);
        write_cstr(STDERR_FILENO,": Warning, pipe2 pipe_out failed, using pipe.\n");
        pipe(pipe_out);
    }

    signal(SIGCHLD,signalHandler);
    signal(SIGPIPE,signalHandler);

    r = fork();
    switch(r) {
        case -1:
            write_str(STDERR_FILENO, progname);
            write_cstr(STDERR_FILENO,": unable to fork.\n");
            return(-1);
        case 0:
            r = fork();
            switch(r) {
                case -1:
                    write_str(STDERR_FILENO, progname);
                    write_cstr(STDERR_FILENO,": unable to fork.\n");
                    return(-1);
                case 0: // children to deal with encapsulation.
                    close(pipe_in [STDIN_FILENO]);
                    close(pipe_in [STDOUT_FILENO]);
                    close(pipe_out[STDOUT_FILENO]);
                    close(STDIN_FILENO);
                    memcpy(argv[0], "bundle", 6);
                    char *a = argv[0]+6;
                    while(*a) *a++ = ' ';

                    rlim.rlim_cur = 0;
                    setrlimit(RLIMIT_NOFILE, &rlim);
                    for(;;) {
                        r = read(pipe_out[STDIN_FILENO], buf+2, BUF_SIZE-6);
                        if (r > 0) {
                            unsigned short len = htons(r);
                            unsigned short *p  = (unsigned short *)buf;
                            p[0] = len;
                            write(STDOUT_FILENO, buf, r+2);
                        } else {
                            return 2;
                        }
                    }
                    kill(getppid(), SIGPIPE);
                    return 0;
            }

            // children to deal with exec.
            if (dup2(pipe_in[STDIN_FILENO], STDIN_FILENO) == -1) {
                write_str(STDERR_FILENO, progname);
                write_cstr(STDERR_FILENO,": unable to move descriptor to stdin.\n");
                return(-1);
            }
            close(pipe_in[STDOUT_FILENO]);
            if (dup2(pipe_out[STDOUT_FILENO], STDOUT_FILENO) == -1) {
                write_str(STDERR_FILENO, progname);
                write_cstr(STDERR_FILENO,": unable to move descriptor to stdout.\n");
                return(-1);
            }
            close(pipe_out[STDIN_FILENO]);
            return(execvp(argv[1], argv+1));
    }
    close(pipe_in [STDIN_FILENO]);
    close(pipe_out[STDIN_FILENO]);
    close(pipe_out[STDOUT_FILENO]);
    close(STDOUT_FILENO);

    // first loop deal with decapsulation.

    rlim.rlim_cur = 0;
    setrlimit(RLIMIT_NOFILE, &rlim);

    for(;;) {
        if (tail + BUF_SIZE >= fifo+sizeof(fifo)) {
            write_str(STDERR_FILENO, progname);
            write_cstr(2,": Might not fit!\n"); return 3;
        }
        r = read(STDIN_FILENO, tail, BUF_SIZE);
        if (r>0) {
            tail += r;
        } else {
            kill(getppid(), SIGPIPE);
            return 1;
        }
        for (;;) {
            int n = tail - head + len2;
            if (n<=2) break;
            char *p = head2 ? head2 : head;
            unsigned int len = 0xF00 & ((*p++)<<8);
            if (head2 && p>=head2+len2) p = fifo;
            len |= (*p)&0xFF;
            // FIXME: if (len==0 || len>4000) ...
            if (n<len+2) break;
            if (len2) {
                if (len2<2) break;
                const struct iovec vec[2] = {
                    { head2+2, len2-2      },
                    { head,    len-len2+2  }
                };
                writev(pipe_in[STDOUT_FILENO],vec,2);
                head += len-len2+2;
                head2 = NULL;
                len2  = 0;
            } else {
                write(pipe_in[STDOUT_FILENO],head+2,len);
                head += len+2;
            }
        }
        if (len2 == 1) continue;
        if (head == tail) {
            head = tail = fifo;
        } else {
            if (head > fifo+2*BUF_SIZE) {
                head2 = head;
                len2  = tail - head;
                head  = tail = fifo;
            }
        }
    }
    kill(getppid(), SIGPIPE);
    return 0;
}
