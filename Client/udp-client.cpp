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
#include <fstream>
#include <cstring>
#include <string>
using namespace std;

#define MAXBUFLEN 508
char host[] = "localhost";
char server_port[] = "4950";
int sockfd;

struct sockaddr_storage g_their_addr;
socklen_t g_addr_len;
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
static packet received_packets[10000000];

void read_input_file(char *path, char args[][1024]);
packet create_packet(char *data);
void get_file_name(char *path, char *file_name);
void receive_file(char *file);
ack_packet create_Ack_packet(int ack_no);
void send_ack(ack_packet ack);

int main(void) {
	char file_name[20] = "";
	char argv[3][1024];
	char path[] = "input.in";
	read_input_file(path, argv);
	struct sockaddr_in server;

	/*	int sockfd;*/
	g_addr_len = sizeof g_their_addr;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;
	int port = htons(atoi(argv[1]));

	/* Create a datagram socket in the internet domain and use the
	 * default protocol (UDP).
	 */
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket()");
		exit(1);
	}

	/* Set up the server name */
	server.sin_family = AF_INET; /* Internet Domain    */
	server.sin_port = port; /* Server Port        */
	server.sin_addr.s_addr = inet_addr("127.0.0.1"); /* Server's Address   */

	struct packet file = create_packet(argv[2]);

	if ((numbytes = sendto(sockfd, &file, sizeof(file), 0,
			(struct sockaddr*) &server, sizeof(server))) == -1) {
		perror("talker: sendto");
		exit(1);
	}

	printf("talker: sent %d bytes to %s\n", file.len, argv[1]);
	get_file_name(argv[2], file_name);
	cout << "Extracted file name: " << file_name << "\n";
	receive_file(file_name);
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

void receive_file(char *file) {
	printf("Reading Data\n");
	int size = 500;
	int numbytes;
	char p_array[size];
	struct packet received_data;
	int expected_seqno = 1;
	ack_packet ackPacket;
	if ((numbytes = recvfrom(sockfd, &received_data, MAXBUFLEN, 0,
			(struct sockaddr*) &g_their_addr, &g_addr_len)) == -1) {
		perror("recvfrom");
		exit(1);
	}

	// This array to hold packets in place according to packet seqno
	// 					Important
	// client will receive the last package as zero to stop
	// this will be the last package in received_data[]
	// write received_data[]  except the last one.
	cout << "Num of packets will be received " << received_data.seqno << "\n";
	int n = received_data.seqno;
	int ack = 0, acks_number = 0;
	int length_of_last_packet;
	while (1) {
		if ((numbytes = recvfrom(sockfd, &received_data, MAXBUFLEN, 0,
				(struct sockaddr*) &g_their_addr, &g_addr_len)) == -1) {
			perror("recvfrom");
			exit(1);
		}
		if (received_data.len == 0)
			break;
		received_packets[received_data.seqno] = received_data;
		/*		memcpy(received_packets[received_data.seqno], received_data.data,
		 received_data.len);*/
		cout << "recieved packet no : " << received_data.seqno << "\n";
		ack = received_data.seqno;
		ackPacket = create_Ack_packet(ack);
		send_ack(ackPacket);
		acks_number++;
		length_of_last_packet = received_data.len;
		cout << "sent Ack packet no : " << ackPacket.ackno << "\n";
	}
	/*here you should write received_packets to file*/
	FILE *recievedFile = fopen(file, "wb");
	cout << length_of_last_packet << " \n";
	for (int i = 0; i < n; i++) {
		fwrite(received_packets[i].data, sizeof(char),
				received_packets[i].len - 8, recievedFile);
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
	cout << "last element is : " << parsed[i - 1] << endl;
	strcpy(file_name, parsed[i - 1]);
}

ack_packet create_Ack_packet(int ack_no) {
	struct ack_packet pack;
	pack.len = 8;
	pack.ackno = ack_no;
	return pack;
}

void send_ack(ack_packet ackPacket) {
	int numbytes;
	ackPacket.len = 8;
	ackPacket.check_sum = 1;
	if ((numbytes = sendto(sockfd, &ackPacket, sizeof(ackPacket), 0,
			(struct sockaddr*) &g_their_addr, g_addr_len)) == -1) {
		perror("talker: sendto");
		exit(1);
	}
}
