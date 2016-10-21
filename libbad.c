#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

int
some_doing_socket(void)
{
        int sock;

        sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == -1) {
            (void) fprintf(stderr, "socket failed: %s\n",
                        strerror(errno));
            exit(1);
        }
        return (sock);
}

int
sneaky_lookup(void)
{
	struct addrinfo hints, *res, *res0;
	int error;

	error = getaddrinfo("www.kame.net", "http", &hints, &res0);
	if (error) {
		(void) fprintf(stderr, "dns lookup: %s\n", gai_strerror(error));
		return (-1);
	}
	for (res = res0; res; res = res->ai_next) {
		printf("got sock for family %d\n", res->ai_family);
	}
	return (0);
}

int
foobar(void)
{
	int fd;

	printf("sdfdsfdsfsdf\n");
	fd = open("/dev/random", O_RDONLY);
	if (fd == -1) {
		printf("open failed!\n");
		exit(1);
	}
	printf("got open!: %d\n", fd);
	close(fd);
	return (0);
}

