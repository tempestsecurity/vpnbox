#include <stdio.h>
#include <string.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#define _GNU_SOURCE
#include <linux/fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "crypto_secretbox.h"
#include <stdint.h>
#include <stdlib.h>
#include <signal.h>
#include "strannex.h"

#define BUF_SIZE	4096
#define MAX_KEY_SIZE	crypto_secretbox_KEYBYTES

#define SIZE_OF_INTEGER       (8*sizeof(unsigned int)) /** 32-bit microprocessor */
#define REDUNDANT_BIT_SHIFTS  5
#define REDUNDANT_BITS        (1<<REDUNDANT_BIT_SHIFTS)
#define BITMAP_LOC_MASK       (REDUNDANT_BITS-1)

struct replay_s {
    int replaywin_size;
    uint64_t replaywin_lastseq;
    uint64_t sequence_number;
    unsigned int bitmap_index_mask;
    unsigned int bitmap_len;
    unsigned int *replaywin_bitmap;
};

void parse_key(unsigned char *k, int *ksize, char *str);

void hexdump(int fd, char *buf, int size);

int check_replay_window (struct replay_s *rep, uint64_t sequence_number);

int update_replay_window (struct replay_s *rep, uint64_t sequence_number);

void signalHandler(int signal)
{
    kill(getppid(), SIGPIPE);
    exit (0);
}

int main(int argc, char **argv)
{
    unsigned char key[crypto_secretbox_KEYBYTES];
    unsigned char crypt[BUF_SIZE], *send_buf;
    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    int ksize = 0, c;
    char buf[BUF_SIZE], *progname;
    int fd, r, pipe_in[2], pipe_out[2];
    struct stat fd_stat;
    uint64_t seq, init_seq = 0;
    struct timeval now_t;
    time_t last_sec;
    uint32_t rep_count, bad_count, notify;
    struct rlimit rlim;

    struct replay_s rep;
    memset(&rep, 0, sizeof(rep));

    if ((progname = strrchr(argv[0], '/')) == NULL) {
        progname = argv[0];
    } else {
        progname++;
    }

    if (argc < 2) {
        write_str(STDERR_FILENO, progname);
        write_cstr(STDERR_FILENO,": params missing\n");
        return -1;
    }

    rep.replaywin_size = 256;
    while ( (c=getopt(argc,argv,"k:K:w:")) != -1 ) {
        switch (c) {
            case 'k':
                parse_key(key, &ksize, optarg);
                if (!ksize) {
                    write_str(STDERR_FILENO, progname);
                    write_cstr(STDERR_FILENO,": error parsing key.\n");
                    return -1;
                }
                break;
            case 'K':
                fd = open(optarg, O_RDONLY);
                if (fd < 0) {
                    write_str(STDERR_FILENO, progname);
                    write_cstr(STDERR_FILENO,": error opening key file.\n");
                    return -1;
                }
                fstat(fd, &fd_stat);
                fd_stat.st_mode &= 0x3FFF;
                if (fd_stat.st_mode != S_IRUSR) {
                    write_str(STDERR_FILENO, progname);
                    write_cstr(STDERR_FILENO,": error key file permission\n");
                    return -1;
                }
                if ((r = read (fd, buf, sizeof(buf)-1)) <= 0) {
                    write_str(STDERR_FILENO, progname);
                    write_cstr(STDERR_FILENO,": error reading key file.\n");
                    return -1;
                }
                close (fd);
                buf[r] = '\0';
                parse_key(key, &ksize, buf);
                break;
            case 'w':
		rep.replaywin_size = atoi(optarg);
                if (rep.replaywin_size < SIZE_OF_INTEGER) {
                    rep.replaywin_size = SIZE_OF_INTEGER;
                }
                if (rep.replaywin_size % SIZE_OF_INTEGER) {
                    write_str(STDERR_FILENO, progname);
                    write_cstr(STDERR_FILENO,": Invalid replay window size.\n");
                    return -1;
                }
                break;
        }
    }

    argv += optind;

    if (!ksize || !argv) {
        write_str(STDERR_FILENO, progname);
        write_cstr(STDERR_FILENO,": error invalid usage.\n");
        return -1;
    }

    signal(SIGCHLD,signalHandler);
    signal(SIGPIPE,signalHandler);

    if (pipe2(pipe_in,  O_DIRECT) == -1) pipe(pipe_in);
    if (pipe2(pipe_out, O_DIRECT) == -1) pipe(pipe_out);

    r = fork();
    switch(r) {
        case -1:
            write_str(STDERR_FILENO, progname);
            write_cstr(STDERR_FILENO,": unable to fork.\n");
            return(-1);
        case 0:
            memset(buf, 0, sizeof(buf));
            memset(crypt, 0, sizeof(crypt));
            r = fork();
            switch(r) {
                case -1:
                    write_str(STDERR_FILENO, progname);
                    write_cstr(STDERR_FILENO,": unable to fork.\n");
                    return(-1);
                case 0: // child to do the cryptography.
                    close(pipe_in [STDIN_FILENO]);
                    close(pipe_in [STDOUT_FILENO]);
                    close(pipe_out[STDOUT_FILENO]);
                    close(STDIN_FILENO);
                    sodium_init();
                    randombytes_buf(nonce, sizeof(nonce));

                    rlim.rlim_cur = 0;
                    setrlimit(RLIMIT_NOFILE, &rlim);
                    for(;;) {
                        r = read(pipe_out[STDIN_FILENO], buf+crypto_secretbox_ZEROBYTES, BUF_SIZE-crypto_secretbox_ZEROBYTES);
                        if (r > 0) {
                            memset(buf, 0, crypto_secretbox_ZEROBYTES);
                            sodium_increment(nonce, sizeof(uint64_t)+sizeof(uint32_t));
                            int ret = crypto_secretbox_xsalsa20poly1305(crypt+8,buf,r+crypto_secretbox_ZEROBYTES,nonce,key);
                            if (ret == 0) {
                                memcpy(crypt, nonce, sizeof(nonce));
                                write(STDOUT_FILENO, crypt, r+8+crypto_secretbox_ZEROBYTES);
                            } else {
                                write_str(STDERR_FILENO, progname);
                                write_str_uint(STDERR_FILENO, ": Erro crypto_secretbox = ", ret);
                                write_cstr(STDERR_FILENO, "\n");
                            }
                        } else {
                            break;
                        }
                    }
                    kill(getppid(), SIGPIPE);
                    return 0;
            }
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
            return(execvp(argv[0], argv));
    }
    close(pipe_in [STDIN_FILENO]);
    close(pipe_out[STDOUT_FILENO]);
    close(pipe_out[STDIN_FILENO]);
    close(STDOUT_FILENO);

    rep.bitmap_len = rep.replaywin_size/SIZE_OF_INTEGER;
    rep.bitmap_index_mask = (rep.bitmap_len-1);
    unsigned int replaywin_bitmap[rep.bitmap_len];
    rep.replaywin_bitmap = replaywin_bitmap;

    sodium_init();
    rep_count = bad_count = notify = 0;
    gettimeofday(&now_t, NULL);
    last_sec = now_t.tv_sec;

    rlim.rlim_cur = 0;
    setrlimit(RLIMIT_NOFILE, &rlim);
    for(;;) {
        gettimeofday(&now_t, NULL);
        if (now_t.tv_sec >= (last_sec+10) && notify) {
            write_str(STDERR_FILENO, progname);
            write_cstr_uint(STDERR_FILENO, ": Replay: ", rep_count);
            writeln_cstr_uint(STDERR_FILENO, ", bad: ", bad_count);
            last_sec = now_t.tv_sec;
            notify = 0;
        }
        r = read(STDIN_FILENO, crypt, sizeof(crypt));
        if (r > 0) {
            memcpy(nonce, crypt, sizeof(nonce));
            seq   = le64toh(((uint64_t *) nonce)[0]);
            if (init_seq == 0) {
                memset(replaywin_bitmap, 0, sizeof(replaywin_bitmap));
                init_seq  = seq-1;
            }
            seq -= init_seq;
            if (check_replay_window (&rep, seq)) {
                memset(crypt+8, 0, crypto_secretbox_BOXZEROBYTES);
                int ret = crypto_secretbox_xsalsa20poly1305_open(buf,crypt+8,r-8,nonce,key);
                if (ret == 0) {
                    update_replay_window (&rep, seq);
                    write(pipe_in[STDOUT_FILENO], buf+crypto_secretbox_ZEROBYTES, r-sizeof(nonce)-crypto_secretbox_BOXZEROBYTES);
                } else {
                    bad_count++;
                    notify++;
                }
            } else {
                rep_count++;
                notify++;
            }
        } else {
            break;
        }
    }
    kill(getppid(), SIGPIPE);
    return 0;
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

void hexdump(int fd, char *buf, int size)
{
    int n;
    char c;
    for (n=0;n<size;n++) {
        if (n%16 == 0) {
           write(fd, "\n", 1);
        }
        c = '0' + ((buf[n]>>4) & 0xF);
        if (c>'9') c+=7;
        write(fd, &c, 1);

        c = '0' + (buf[n] & 0xF);
        if (c>'9') c+=7;
        write(fd, &c, 1);

        if (n%2) {
            write(fd, " ", 1);
        }
    }
    write(fd, "\n", 1);
}

int check_replay_window (struct replay_s *rep, uint64_t sequence_number)
{
    int bit_location;
    int index;

    /**
     * replay shut off
     */
    if (rep->replaywin_size == 0) {
        return 1;
    }

    /**
     * first == 0 or wrapped
     */
    if (sequence_number == 0) {
        return 0;
    }

    /**
     * first check if the sequence number is in the range
     */
    if (sequence_number > rep->replaywin_lastseq) {
        return 1;  /** larger is always good */
    }

    /**
     * The packet is too old and out of the window
     */
    if ((sequence_number + rep->replaywin_size) <
        rep->replaywin_lastseq) {
          return 0;
    }

    /**
     * The sequence is inside the sliding window
     * now check the bit in the bitmap
     * bit location only depends on the sequence number
     */
    bit_location = sequence_number&BITMAP_LOC_MASK;
    index = (sequence_number>>REDUNDANT_BIT_SHIFTS)&rep->bitmap_index_mask;

    /*
     * this packet has already been received
     */
    if (rep->replaywin_bitmap[index]&(1<<bit_location)) {
        return 0;
    }

    return 1;
}

int update_replay_window (struct replay_s *rep, uint64_t sequence_number)
{
    int bit_location;
    int index, index_cur, id;
    int diff;

    if (rep->replaywin_size == 0) {  /** replay shut off */
        return 1;
    }

    if (sequence_number == 0) {
        return 0;     /** first == 0 or wrapped */
    }

    /**
     * the packet is too old, no need to update
     */
    if ((rep->replaywin_size + sequence_number) <
        rep->replaywin_lastseq) {
           return 0;
    }

    /**
     * now update the bit
     */
    index = (sequence_number>>REDUNDANT_BIT_SHIFTS);

    /**
     * first check if the sequence number is in the range
     */
    if (sequence_number>rep->replaywin_lastseq) {
        index_cur = rep->replaywin_lastseq>>REDUNDANT_BIT_SHIFTS;
        diff = index - index_cur;
        if (diff > rep->bitmap_len) {  /* something unusual in this case */
            diff = rep->bitmap_len;
        }

        for (id = 0; id < diff; ++id) {
            rep->replaywin_bitmap[(id+index_cur+1)&rep->bitmap_index_mask]
                = 0;
        }

        rep->replaywin_lastseq = sequence_number;
    }

    index &= rep->bitmap_index_mask;
    bit_location = sequence_number&BITMAP_LOC_MASK;

    /* this packet has already been received */
    if (rep->replaywin_bitmap[index]&(1<<bit_location)) {
        return 0;
    }

    rep->replaywin_bitmap[index] |= (1<<bit_location);

    return 1;
}
