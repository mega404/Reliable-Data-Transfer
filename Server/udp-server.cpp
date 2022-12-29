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
#include <sys/wait.h>
#include <cstdio>
#include <vector>
#include <random>
#include <math.h>
#include <sstream>
#include <chrono>
#include <sys/poll.h>

using namespace std;
#define MYPORT "4950"    // the port users will be connecting to
#define MAXBUFLEN 509
#define TimeToWaitForPacket 2

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

vector<packet> filePackets;
struct sockaddr_in g_their_addr;
socklen_t g_addr_len;
int clientsock;
//0 is for slowStart
//1 is for congestion avoidance
//2 is for Fast recovery
int state = 0;
int cwnSize = 1;
int ssthreshold = 63;
float plp;
int seed;

//int timerArary[] = new timerArray[cwnSize];

void read_input_file(char *path, char args[][1024]);
void create_file_packets(char *path);
packet create_packet(char data[], int size);
void send_packet(struct packet sent_packet);
bool recieve_ack_packet();
void send_file_stop_and_wait();

/*// get sockaddr, IPv4 or IPv6:
 void* get_in_addr(struct sockaddr *sa) {
 if (sa->sa_family == AF_INET) {
 return &(((struct sockaddr_in*) sa)->sin_addr);
 }

 return &(((struct sockaddr_in6*) sa)->sin6_addr);
 }*/

int main(void) {
	char argv[3][1024];
	char path[] = "input.in";
	read_input_file(path, argv);
	int sockfd;
	struct sockaddr_in client, server;
	int rv;
	int numbytes;
	seed = stoi(argv[1]);
	plp = stof(argv[2]);
	cout << "plp is " << plp << endl;

	sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if (sockfd < 0) {
		printf("Error while creating socket\n");
		return -1;
	}
	printf("Socket created successfully\n");

	// Set port and IP:
	server.sin_family = AF_INET;
	server.sin_port = htons(4950);
	server.sin_addr.s_addr = inet_addr("127.0.0.1");

	// Bind to the set port and IP:
	if (bind(sockfd, (struct sockaddr*) &server, sizeof(server)) < 0) {
		printf("Couldn't bind to the port\n");
		return -1;
	}
	printf("Done with binding\n");

	printf("listener: waiting to recvfrom...\n");
	while (1) {
		struct packet file_name;
		int len = sizeof(client);
		if (recvfrom(sockfd, &file_name, MAXBUFLEN, 0,
				(struct sockaddr*) &client, &len) < 0) {
			printf("Couldn't receive\n");
			return -1;
		}
		cout << "Server got new connection\n";
		int status = 0;
		if (!fork()) {
			g_their_addr = client;
			g_addr_len = sizeof(client);
			struct timeval timeout;
			timeout.tv_sec = 3;
			timeout.tv_usec = 0;
			if ((clientsock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
				printf("Error while creating socket\n");
				return -1;
			}
			if (setsockopt(clientsock, SOL_SOCKET, SO_RCVTIMEO, &timeout,
					sizeof(timeout)) < 0) {
				perror("failed allowing server socket to reuse address");
			}
			create_file_packets(file_name.data);
			//cout<<"file packets length is : "<<filePackets.size()<<endl;
			send_file_stop_and_wait();
			exit(0);
		}
	}
	int returnStatus;
	waitpid(0, &returnStatus, 0);
	return 0;
}

void send_file_stop_and_wait() {
	cout << "iam here " << endl;
	uniform_real_distribution<> dis(0, 1);
	mt19937 gen(seed);
	//float propToSend = dis(gen);
	char empty[] = "";
	// This packet contains number of packets will the
	// Client receive
	packet p = create_packet(empty, 0);
	p.seqno = filePackets.size();
	send_packet(p);

	int packetCounter = 0;
	while (packetCounter < filePackets.size()) {
		//cout<<"iam in while loop"<<endl;
		int remained_size = (filePackets.size() - packetCounter);
		//int ackToRecieve[cwnSize];
		//int ackCounter = 0;

		for (int i = 0; i < min(cwnSize, remained_size); i++) {
			//cout<<"iam in for loop"<<endl;
			filePackets[packetCounter].seqno = packetCounter;

			bool timeout = true;
			while (timeout) {
				float propToSend = dis(gen);
				//cout<<"propToSend : "<<propToSend<<endl;
				if (propToSend > plp) {
					cout << "packet sent : " << filePackets[packetCounter].seqno
							<< endl;
					send_packet(filePackets[packetCounter]);
				} else {
					cout << "packet dropped" << endl;
				}
				timeout = recieve_ack_packet();
			}
			packetCounter++;
		}
		cout << "cwinSIZE is " << cwnSize << endl;
		if (state == 0) {
			cwnSize *= 2;
		} else {
			cwnSize += 1;
		}
		if (cwnSize >= ssthreshold)
			state = 1;
	}
	cout << "Finished sending file " << endl;
}

void read_input_file(char *path, char args[][1024]) {
	FILE *filePointer;
	int bufferLength = 255;
	char buffer[bufferLength];
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

void create_file_packets(char *path) {
	FILE *fileptr;
	long filelen;
	int numbytes;
	struct packet file_packet;
	fileptr = fopen(path, "rb");  // Open the file in binary mode
	cout << "iam here and fileptr is : " << fileptr << endl;
	fseek(fileptr, 0, SEEK_END);          // Jump to the end of the file
	filelen = ftell(fileptr);         // Get the current byte offset in the file
	rewind(fileptr);
	char send_buffer[500]; // no link between BUFSIZE and the file size
	while (!feof(fileptr)) {
		int nb = fread(send_buffer, 1, 500, fileptr);
		file_packet = create_packet(send_buffer, nb);
		filePackets.push_back(file_packet);
		/*		nb = fread(send_buffer, 1, 500, fileptr);*/
		/*		cout << nb << " \n";*/
		/*		file_packet = create_packet(send_buffer, nb);*/
	}

	/*	filePackets.push_back(file_packet);*/
	file_packet = { 0, 0, 0, "" };
	filePackets.push_back(file_packet);
	return;
}

packet create_packet(char data[], int size) {
	struct packet pack;
	memcpy(pack.data, data, size);
	pack.len = size + 8;
	return pack;
}

void send_packet(struct packet sent_packet) {
	int numbytes;
	if ((numbytes = sendto(clientsock, &sent_packet, sizeof(sent_packet), 0,
			(struct sockaddr*) &g_their_addr, g_addr_len)) == -1) {
		perror("talker: sendto");
		exit(1);
	}
}

bool recieve_ack_packet() {
	struct ack_packet received_ack;

	//auto start = std::chrono::system_clock::now();
	//while (std::chrono::duration<double>(std::chrono::system_clock::now() - start) < std::chrono::duration<double>(TimeToWaitForPacket)) {
	struct pollfd pfd = { .fd = clientsock, .events = POLLIN };
	int state = poll(&pfd, 1, TimeToWaitForPacket * 1000);
	recvfrom(clientsock, &received_ack, MAXBUFLEN, 0,
			(struct sockaddr*) &g_their_addr, &g_addr_len);
	/*if (numbytes  == -1) {
	 perror("recvfrom");
	 exit(1);
	 }*/

	if (state != 0) {
		cout << "Ack recived : " << received_ack.ackno << endl;
		return false;
	}
	return true;
}
