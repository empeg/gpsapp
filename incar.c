/*
 * Copyright (c) 2002 Jan Harkes <jaharkes(at)cs.cmu.edu>
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU General Public License Version 2.
 */

/*
 * returns 0 when we're on DC (in the car)
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

static int InCar(void)
{
    int n, fd;
    char buf[17], *match = "1 (Battery Power)";

    fd = open("/proc/empeg_power", O_RDONLY);
    if (fd == -1)
	return 0;

    n = read(fd, buf, 17);
    close(fd);

    /* first line should be "1 (Battery Power)" */
    return (n == 17 && memcmp(buf, match, 17) == 0);
}

int main(int argc, char **argv)
{
    exit(!InCar());
}

