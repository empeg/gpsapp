/*
 * Copyright (c) 2002 Jan Harkes <jaharkes(at)cs.cmu.edu>
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU General Public License Version 2.
 */

/*
 * Basically we want to modify the init binary so that it doesn't start
 * /empeg/bin/player directly, but starts an intermediate program that
 * execs the player with the -s- flag.
 *
 * We do this by binary editing any references to /empeg/bin/player into
 * /empeg/bin/play-s.
 *
 * If this application is started as /empeg/bin/play-s it execs the real player
 * binary with an additional -s- flag (but only when we're in Car/DC mode).
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

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

static void exec_player(int argc, char **argv)
{
    char **nargv = malloc(argc * sizeof(char *));
    int i, nargc = 1;

    if (nargv) {
	nargv[0] = "/empeg/bin/player";

	if (InCar()) {
	    puts("Starting player with -s- option\n");
	    nargv[nargc++] = "-s-";
	}

	for (i = 1; i < argc; i++)
	    nargv[nargc++] = argv[i];
	nargv[nargc++] = NULL;

	execv("/empeg/bin/player", nargv);
    }
}

static int copy_file(char *from, char *to)
{
    char buf[4096];
    int in, out, n_in, n_out, ret = -1;

    in = open(from, O_RDONLY);
    if (in == -1) return -1;

    out = open(to, O_CREAT | O_TRUNC | O_WRONLY, 0755);
    if (out == -1) {
	close(in);
	return -1;
    }

    while (1) {
	n_in  = read(in, buf, 4096);
	if (n_in == 0) break;
	if (n_in < 0) goto err_exit;

	n_out = write(out, buf, n_in);
	if (n_out != n_in) goto err_exit;
    }
    /* make sure changes have a good change of hitting the disk */
    if (fsync(out) == -1)
	goto err_exit;

    ret = 0;

err_exit:
    close(out);
    close(in);

    return ret;
}

int modify(char *path)
{
    char buf[18], *match = "empeg/bin/player";
    int fd, n, ret = -1;
    
    fd = open(path, O_RDWR);
    if (fd == -1)
	return -1;

    while(1) {
	n = read(fd, buf, 1);
	if (n != 1) {
	    puts("String not found, was init already modified?\n");
	    goto fail;
	}

	if (buf[0] != '/')
	    continue;

	n = read(fd, buf, 17);

	if (n != 17 || memcmp(buf, match, 17) != 0)
	    continue;

	/* skip back over 'er\0' */
	if (lseek(fd, -3, SEEK_CUR) == -1)
	    goto fail;

	if (write(fd, "-s", 2) != 2)
	    goto fail;

	break;
    }
    if (fsync(fd) == -1)
	goto fail;

    ret = 0;

fail:
    close(fd);
    return ret;
}

int main(int argc, char **argv)
{
    /* are we used as a replacement player? */
    if (strcmp(argv[0], "/empeg/bin/play-s") == 0) {
	exec_player(argc, argv);
	goto fail; /* We should never get here! */
    }
    
    /* Copy ourself (relatively safely) to /empeg/bin/play-s.new */
    puts("Copying myself to /empeg/bin/play-s.new\n");
    if (copy_file(argv[0], "/empeg/bin/play-s.new") == -1)
	goto fail;

    puts("Renaming the new copy to /empeg/bin/play-s\n");
    if (rename("/empeg/bin/play-s.new", "/empeg/bin/play-s") == -1)
	goto fail;

    /* Make a copy of /bin/init */
    puts("Copying /bin/init to /bin/init.new\n");
    if (copy_file("/bin/init", "/bin/init.new") == -1)
	goto fail;

    puts("Modifying /bin/init.new\n");
    if (modify("/bin/init.new") == -1)
	goto fail;

    /* Make a backup of /bin/init */
    puts("Backing up /bin/init as /bin/init.orig\n");
    if (copy_file("/bin/init", "/bin/init.orig") == -1)
	goto fail;

    puts("Moving the modified init binary to /bin/init\n");
    if (rename("/bin/init.new", "/bin/init") == -1)
	goto fail;

    puts("All done, remount the drive as readonly ('ro'), cross your fingers and reboot\n");
    exit(0);

fail:
    perror("Failed");
    exit(-1);
}
