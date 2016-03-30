#define _GNU_SOURCE
#include <fcntl.h>
#include <unistd.h>
#include "strannex.h"

int pipe_handle(int *pipefd)
{
    int r;
    if ((r = pipe2(pipefd,  O_DIRECT)) == -1) {
        write_cstr(STDERR_FILENO,": Warning, pipe2 failed, using pipe.\n");
        r = pipe(pipefd);
    }
    return r;
}
