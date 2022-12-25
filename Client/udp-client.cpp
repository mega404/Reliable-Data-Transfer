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

#define MAXBUFLEN 508
char host[] = "localhost";
char server_port[] = "4950";

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
packet create_packet(char *data);
void get_file_name(char *path, char *file_name);
void receive_file(int sockfd, char *file);

int main(void) {
	char file_name[20] = "";
	char argv[3][1024];
	char path[] = "input.in";
	read_input_file(path, argv);
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET6; // set to AF_INET to use IPv4
	hints.ai_socktype = SOCK_DGRAM;

	if ((rv = getaddrinfo(argv[0], argv[1], &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and make a socket
	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol))
				== -1) {
			perror("talker: socket");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "talker: failed to create socket\n");
		return 2;
	}
	struct packet file = create_packet(argv[2]);

	if ((numbytes = sendto(sockfd, &file, sizeof(file), 0, p->ai_addr,
			p->ai_addrlen)) == -1) {
		perror("talker: sendto");
		exit(1);
	}

	printf("talker: sent %d bytes to %s\n", file.len, argv[1]);
	get_file_name(argv[2], file_name);
	cout << "Extracted file name: " << file_name << "\n";
	receive_file(sockfd, file_name);
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

packet create_packet(char *data) {
	struct packet pack;
	strcpy(pack.data, data);
	pack.len = strlen(data) + 8;
	return pack;
}

void receive_file(int sockfd, char *file) {
	printf("Reading Data\n");
	struct sockaddr_storage their_addr;
	socklen_t addr_len;
	addr_len = sizeof their_addr;

	int size = 500;
	int numbytes;
	char p_array[size];
	struct packet received_data;
	FILE *recievedFile = fopen(file, "wb");

	if ((numbytes = recvfrom(sockfd, &received_data, MAXBUFLEN, 0,
			(struct sockaddr*) &their_addr, &addr_len)) == -1) {
		perror("recvfrom");
		exit(1);
	}
	while (1) {
		if (received_data.len == 0)
			break;
		fwrite(received_data.data, sizeof(char), received_data.len - 8,
				recievedFile);

		if ((numbytes = recvfrom(sockfd, &received_data, MAXBUFLEN, 0,
				(struct sockaddr*) &their_addr, &addr_len)) == -1) {
			perror("recvfrom");
			exit(1);
		}
	}
	fclose(recievedFile);
	printf("Finished reading\n");
	fflush(stdout);
}

void get_file_name(char *path, char *file_name) {
	char parsed[100][1024];
	char *token;
	char *rest = path;
	int i = 0;

	while ((token = strtok_r(rest, "\\", &rest))) {
		strcpy(parsed[i], token);
		i++;
	}
	strcpy(file_name, parsed[i - 1]);
}
