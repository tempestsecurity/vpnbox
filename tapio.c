#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
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

#include "strannex.h"

#ifndef POLLRDHUP
#define POLLRDHUP       0x2000
#endif

#define BUF_SIZE 4096

int tap_alloc(char *dev, char *name, int owner)
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

    ifr.ifr_flags = IFF_TAP|IFF_NO_PI;   /* IFF_TUN or IFF_TAP, plus maybe IFF_NO_PI */

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

    if ((err = ioctl(fd, TUNSETOWNER, owner)) < 0) {
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

int main(int argc, char **argv)
{
    char buf[BUF_SIZE], dev[IFNAMSIZ], tapdev[10];
    struct pollfd event[2];
    int tap, r;

    strcpy(dev, "/dev/net/tun");
    if (argc == 2) {
        write(STDERR_FILENO, "Forced to ", 10);
        write(STDERR_FILENO, argv[1], strlen(argv[1]));
        write(STDERR_FILENO, "\n", 1);
        strcpy(tapdev, argv[1]);
        tap = tap_alloc (dev, tapdev, 1000);
    } else {
        strstart(tapdev);
        for (r=0 ; r<1000 ; r++) {
            strarray(tapdev);
            strannex(tapdev, "tap");
            strannex_uint(tapdev, r);
            tap = tap_alloc (dev, tapdev, 1000);
            if (tap > 0) break; // success
        }
    }
    if (tap <= 0) {
        write(STDERR_FILENO, "Cannot open TAP\n", 16);
        return tap;
    }

    memset(&event, 0, sizeof(struct pollfd));
    event[0].fd = STDIN_FILENO;
    event[0].events = POLLIN | POLLERR | POLLRDHUP | POLLHUP | POLLNVAL;
    event[1].fd = tap;
    event[1].events = POLLIN | POLLERR | POLLRDHUP | POLLHUP | POLLNVAL;

    for(;;) {
        r = poll(event, 2, -1);
        if(r == -1) {
            write(STDERR_FILENO, "poll() error.\n", 14);
            return -1;
        }
        if (event[0].revents & (POLLERR | POLLRDHUP | POLLHUP | POLLNVAL)) {
            return 0;
        }
        if (event[0].revents & POLLIN) {
            r = read(STDIN_FILENO, buf, BUF_SIZE);
            if (r > 0) {
                write(tap, buf, r);
            } else if (r < 0) {
                return 1;
            }
        }
        if (event[1].revents & POLLIN) {
            r = read(tap, buf, BUF_SIZE);
            if (r > 0) {
                write(STDOUT_FILENO, buf, r);
            } else if (r < 0) {
                return 2;
            }
        }
    }
    return 0;
}
