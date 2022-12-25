#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
using namespace std;
#define MYPORT "4950"    // the port users will be connecting to
#define MAXBUFLEN 100

struct packet {
	uint16_t check_sum;
	uint16_t len;
	uint32_t seqno;
	char data[500];
};

struct ack_packet {
	uint16_t check_sum;
	uint16_t len;
	uint32_t ackno;
};

void read_input_file(char *path, char args[][1024]);

// get sockaddr, IPv4 or IPv6:
void* get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*) sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*) sa)->sin6_addr);
}

int main(void) {
	char argv[3][1024];
	char path[] = "input.in";
	read_input_file(path, argv);
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;
	struct sockaddr_storage their_addr;
	char buf[MAXBUFLEN];
	socklen_t addr_len;
	char s[INET6_ADDRSTRLEN];

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET6; // set to AF_INET to use IPv4
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE; // use my IP
	if ((rv = getaddrinfo(NULL, argv[0], &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol))
				== -1) {
			perror("listener: socket");
			continue;
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("listener: bind");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "listener: failed to bind socket\n");
		return 2;
	}

	freeaddrinfo(servinfo);

	printf("listener: waiting to recvfrom...\n");

	addr_len = sizeof their_addr;
	if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN - 1, 0,
			(struct sockaddr*) &their_addr, &addr_len)) == -1) {
		perror("recvfrom");
		exit(1);
	}
	buf[numbytes] = '\0';
	if (!fork()) {
		printf("listener: got packet from %s\n",
				inet_ntop(their_addr.ss_family,
						get_in_addr((struct sockaddr*) &their_addr), s,
						sizeof s));
		printf("listener: packet is %d bytes long\n", numbytes);
		buf[numbytes] = '\0';
		printf("listener: packet contains \"%s\"\n", buf);
		///////////////////////

		//////////////////////
		exit(0);
	}
	close(sockfd);
	return 0;
}

void read_input_file(char *path, char args[][1024]) {
	FILE *filePointer;
	int bufferLength = 255;
	char buffer[bufferLength]; /* not ISO 90 compatible */
	filePointer = fopen(path, "r");
	int i = 0;
	while (fgets(buffer, bufferLength, filePointer)) {
		strcpy(args[i], buffer);
		args[i][strlen(buffer) - 2] = '\0';
		printf("%s\n", args[i]);
		i++;
	}

	fclose(filePointer);
}

