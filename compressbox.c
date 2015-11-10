#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <errno.h>
#include <arpa/inet.h>
#include <endian.h>
#include <minilzo.h>
#include "writestr.h"

#define MULT		4
#define BUF_SIZE	4096

#include <stdio.h>
void signalHandler(int signal)
{
    exit (0);
}

int main(int argc, char **argv)
{
    char in[BUF_SIZE], out[BUF_SIZE], work[LZO1X_1_MEM_COMPRESS];
    lzo_uint ulen;
    int r, pipe_in[2], pipe_out[2];
    struct rlimit rlim;

    if (argc < 2) {
        write_cstr(STDERR_FILENO,"command missing\n");
        return -1;
    }

    if (pipe2(pipe_in,  O_DIRECT) == -1) {
        write_cstr(STDERR_FILENO,"Warning, pipe2 pipe_in failed, using pipe.\n");
        pipe(pipe_in);
    }
    if (pipe2(pipe_out, O_DIRECT) == -1) {
        write_cstr(STDERR_FILENO,"Warning, pipe2 pipe_out failed, using pipe.\n");
        pipe(pipe_out);
    }

    signal(SIGCHLD,signalHandler);
    signal(SIGPIPE,signalHandler);

    r = fork();
    switch(r) {
        case -1:
            write_cstr(STDERR_FILENO,"unable to fork.\n");
            return(-1);
        case 0:
            r = fork();
            switch(r) {
                case -1:
                    write_cstr(STDERR_FILENO,"unable to fork.\n");
                    return(-1);
                case 0: // children to deal with compression.
                    close(pipe_in [STDIN_FILENO]);
                    close(pipe_in [STDOUT_FILENO]);
                    close(pipe_out[STDOUT_FILENO]);
                    close(STDIN_FILENO);
                    rlim.rlim_cur = 0;
                    setrlimit(RLIMIT_NOFILE, &rlim);
                    if (lzo_init() != LZO_E_OK) {
                        write_cstr(STDERR_FILENO, "Erro initting lzo.\n");
                        return 1;
                    }

                    for(;;) {
                        r = read (pipe_out[STDIN_FILENO], in, sizeof(in));
                        if (r > 0) {
                            lzo1x_1_compress (in, r, out, &ulen, work);
                            write (STDOUT_FILENO, out, ulen);
                            //write_str_uint(STDERR_FILENO, "lzo1x_1_compress r=", r);
                            //writeln_str_uint(STDERR_FILENO, ", ulen=", ulen);
                        } else {
                            if (r < 0) writeln_str_uint(STDERR_FILENO, "Read error at reading: ", errno);
                            break;
                        }
                    }
                    return 0;
            }

            // children to deal with exec.
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
            return(execv(argv[1], argv+1));
    }
    close(pipe_in [STDIN_FILENO]);
    close(pipe_out[STDIN_FILENO]);
    close(pipe_out[STDOUT_FILENO]);
    close(STDOUT_FILENO);

    // first loop deal with decompression.

    rlim.rlim_cur = 0;
    setrlimit(RLIMIT_NOFILE, &rlim);

    if (lzo_init() != LZO_E_OK) {
        write (STDERR_FILENO, "Erro initting lzo.\n", 19);
        return 1;
    }

    for(;;) {
        r = read (STDIN_FILENO, in, sizeof(in));
        if (r > 0) {
            if (lzo1x_decompress (in, r, out, &ulen, work)==LZO_E_OK) {
                write (pipe_in[STDOUT_FILENO], out, ulen);
                //write_str_uint(STDERR_FILENO, "lzo1x_decompress r=", r);
                //writeln_str_uint(STDERR_FILENO, ", ulen=", ulen);
            } else {
                write_str_uint(STDERR_FILENO, "ERROR lzo1x_decompress r=", r);
                writeln_str_uint(STDERR_FILENO, ", ulen=", ulen);
            }
        } else {
            if (r < 0) write_str_uint(STDERR_FILENO, "Read error at decompression: errno=", errno);
            break;
        }
    }
    return 0;
}
