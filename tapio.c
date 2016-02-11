#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>
#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#include <netdb.h>

#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_tun.h>

#include <sys/time.h>
#include <sys/resource.h>

#include <stdlib.h>
#include <signal.h>

#include "strannex.h"

#ifndef POLLRDHUP
#define POLLRDHUP       0x2000
#endif

#define BUF_SIZE 4096

int tap_alloc(char *dev, char *name, int tun)
{
    struct ifreq ifr;
    int fd, err;

    /* Arguments taken by the function:
     *
     * char *dev: the name of an interface (or '\0'). MUST have enough
     *   space to hold the interface name if '\0' is passed
     * int flags: interface flags (eg, IFF_TUN etc.)
     */

    /* open the clone device */

    if( (fd = open(dev, O_RDWR | O_EXCL)) < 0 ) {
        return fd;
    }

    /* preparation of the struct ifr, of type "struct ifreq" */
    memset(&ifr, 0, sizeof(ifr));

    ifr.ifr_flags = tun | IFF_NO_PI;   /* IFF_TUN or IFF_TAP, plus maybe IFF_NO_PI */

    if (*dev) {
        /* if a device name was specified, put it in the structure; otherwise,
         * the kernel will try to allocate the "next" device of the
         * specified type */
        strncpy(ifr.ifr_name, name, IFNAMSIZ);
    }

    /* try to create the device */
    if ((err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0) {
        close(fd);
        return err;
    }

    if ((err = ioctl(fd, TUNSETPERSIST, 1)) < 0) {
        close(fd);
        return err;
    }

    /* if the operation was successful, write back the name of the
     * interface to the variable "dev", so the caller can know
     * it. Note that the caller MUST reserve space in *dev (see calling
     * code below) */
    strcpy(dev, ifr.ifr_name);

    /* this is the special file descriptor that the caller will use to talk
     * with the virtual interface */
    return fd;
}

void signalHandler(int signal)
{
    kill(getppid(), SIGPIPE);
    exit (0);
}

int main(int argc, char **argv)
{
    char buf[BUF_SIZE], dev[IFNAMSIZ]="/dev/net/tun", tapdev[10], *progname;
    int tap, r, tun = IFF_TUN;
    int c, verbose = 0, interval = 0, keep = 0, keep_count = 0;
    time_t last_keep;
    struct timeval tv;
    struct rlimit rlim;
    fd_set rfd;

    while ( (c=getopt(argc,argv,"vi:k:")) != -1 ) {
        switch (c) {
            case 'v':
                verbose++;
                break;
            case 'i':
                interval = atoi(optarg);
                break;
            case 'k':
                keep = atoi(optarg);
                break;
        }
    }

    if ((progname = strrchr(argv[0], '/')) == NULL) {
        progname = argv[0];
    } else {
        progname++;
    }

    if (keep && !interval) {
        write_str(STDERR_FILENO, progname);
        write_cstr(STDERR_FILENO, ": keep alive need interval value.\n");
        return 1;
    }

    signal(SIGPIPE,signalHandler);

    if (strcmp("tunio", progname)) tun = IFF_TAP;

    if (argc>optind) {
        if (!strncmp(argv[optind],"/dev/",5)) argv[optind] += 5;
        if (verbose) {
            write_str(STDERR_FILENO, progname);
            write_cstr(STDERR_FILENO, ": Forced to ");
            write_str(STDERR_FILENO,argv[optind]);
            write_cstr(STDERR_FILENO, "\n");
        }
        strstart(tapdev);
        strarray(tapdev);
        strannex(tapdev,argv[optind]);
        tap = tap_alloc (dev, tapdev, tun);
    } else {
        strstart(tapdev);
        for (r=0 ; r<1000 ; r++) {
            strarray(tapdev);
            if (tun == IFF_TUN) {
                strannex(tapdev, "tun");
            } else {
                strannex(tapdev, "tap");
            }
            strannex_uint(tapdev, r);
            tap = tap_alloc (dev, tapdev, tun);
            if (tap > 0) break; // success
        }
    }
    if (tap <= 0) {
        write_str(STDERR_FILENO, progname);
        if (tun == IFF_TUN) {
            write_cstr(STDERR_FILENO, ": Cannot open TUN\n");
        } else {
            write_cstr(STDERR_FILENO, ": Cannot open TAP\n");
        }
        return 1;
    }

    gettimeofday(&tv, NULL);
    last_keep = tv.tv_sec;

    rlim.rlim_cur = 0;
    setrlimit(RLIMIT_NOFILE, &rlim);

    /* ------------ main loop without length ---------- */
    for (;;) {
        FD_ZERO(&rfd);
        FD_SET(STDIN_FILENO, &rfd);
        FD_SET(tap,          &rfd);
        tv.tv_sec  = 1;
        tv.tv_usec = 0;
        r = select(tap+1,&rfd,NULL,NULL,&tv);
        if(r == -1) {
            write_str(STDERR_FILENO, progname);
            write_cstr(STDERR_FILENO, ": select() error.\n");
            return errno;
        }
        if (interval) {
            gettimeofday(&tv, NULL);
            if (verbose && (tv.tv_sec % 10)==0) {
                write_str(STDERR_FILENO, progname);
                write_str_uint(STDERR_FILENO, ": keep count: ", keep_count);
                writeln_str_uint(STDERR_FILENO, ", keep: ", keep);
            }
            if (tv.tv_sec >= (last_keep+interval)) {
                write(STDOUT_FILENO, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 20);
                if (verbose) {
                    write_str(STDERR_FILENO, progname);
                    write_cstr(STDERR_FILENO, ": Sent a keep alive.\n");
                }
                last_keep = tv.tv_sec;
            }
        }
        if (keep) {
            keep_count++;
            if (verbose) {
                write_str(STDERR_FILENO, progname);
                writeln_str_uint(STDERR_FILENO, ": keep count increased: ", keep_count);
            }
        }

        if (FD_ISSET(STDIN_FILENO,&rfd)) {
            r = read(STDIN_FILENO, buf, BUF_SIZE);
            if (r > 0 && keep) { // reset count on read.
                keep_count = 0;
                if (verbose) {
                    write_str(STDERR_FILENO, progname);
                    write_cstr(STDERR_FILENO, ": data received, keep count reseted.\n");
                }
            }
            if (r > 20) { // ignore packet inferior to 21 bytes, keep alive
                write(tap, buf, r);
            } else if (r <= 0) {
                return 1;
            }
        }
        if (FD_ISSET(tap,&rfd)) {
            r = read(tap, buf, BUF_SIZE);
            if (r > 0) {
                write(STDOUT_FILENO, buf, r);
            } else if (r <= 0) {
                return 2;
            }
        }
        if (keep && keep_count >= keep) {
            write_str(STDERR_FILENO, progname);
            writeln_str_uint(STDERR_FILENO, ": Keep count exceeded ", keep_count);
            kill(getppid(), SIGPIPE);
            return 0;
        }
    }
    return 0;
}
