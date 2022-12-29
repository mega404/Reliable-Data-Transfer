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

struct p_timer {
	uint32_t seqno;
	chrono::time_point<std::chrono::system_clock> sentAt;
};

struct ack_packet {
	uint16_t check_sum;
	uint16_t len;
	uint32_t ackno;
};

ack_packet empty_ack = { 0, 0, 0 };
// 1 -> stop and wait
// 2 -> selective
int technique = 2;
vector<packet> filePackets;
vector<p_timer> packetsTimer;
struct sockaddr_in g_their_addr;
socklen_t g_addr_len;
int clientsock;

enum state {
	slow_start, congestion_avoidance, fast_recovery
};

int cwnSize = 1;
int ssthreshold = 63;
float plp;
int seed;

void read_input_file(char *path, char args[][1024]);
void create_file_packets(char *path);
packet create_packet(char data[], int size);
void send_packet(struct packet sent_packet);
void send_file_stop_and_wait();
bool recieve_ack_packet_stop_and_wait();
void congestion_control_selective();
ack_packet recieve_ack_packet();

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

			if ((clientsock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
				printf("Error while creating socket\n");
				return -1;
			}

			create_file_packets(file_name.data);
			if (technique == 1) {
				struct timeval timeout;
				timeout.tv_sec = 3;
				timeout.tv_usec = 0;
				if (setsockopt(clientsock, SOL_SOCKET, SO_RCVTIMEO, &timeout,
						sizeof(timeout)) < 0) {
					perror("failed allowing server socket to reuse address");
				}
				send_file_stop_and_wait();
			} else {
				cout << "congestion_control_selective \n" << "\n";
				congestion_control_selective();
			}
			exit(0);
		}
	}
	int returnStatus;
	waitpid(0, &returnStatus, 0);
	return 0;
}

void send_file_stop_and_wait() {
	uniform_real_distribution<> dis(0, 1);
	mt19937 gen(seed);
	char empty[] = "";
	// This packet contains number of packets will the
	// Client receive
	packet p = create_packet(empty, 0);
	p.seqno = filePackets.size();
	send_packet(p);

	int packetCounter = 0;
	state s = slow_start;
	while (packetCounter < filePackets.size()) {
		int remained_size = (filePackets.size() - packetCounter);
		for (int i = 0; i < min(cwnSize, remained_size); i++) {
			filePackets[packetCounter].seqno = packetCounter;
			bool timeout = true;
			while (timeout) {
				float propToSend = dis(gen);
				if (propToSend > plp) {
					cout << "packet sent : " << filePackets[packetCounter].seqno
							<< endl;
					send_packet(filePackets[packetCounter]);
				} else {
					cout << "packet dropped" << endl;
				}
				timeout = recieve_ack_packet_stop_and_wait();
			}
			packetCounter++;
		}
		cout << "cwinSIZE is " << cwnSize << endl;
		if (s == slow_start) {
			cwnSize *= 2;
		} else {
			cwnSize += 1;
		}
		if (cwnSize >= ssthreshold)
			s = congestion_avoidance;
	}
	cout << "Finished sending file " << endl;
}

bool recieve_ack_packet_stop_and_wait() {
	struct ack_packet received_ack;
	struct pollfd pfd = { .fd = clientsock, .events = POLLIN };
	int state = poll(&pfd, 1, TimeToWaitForPacket * 1000);
	recvfrom(clientsock, &received_ack, MAXBUFLEN, 0,
			(struct sockaddr*) &g_their_addr, &g_addr_len);
	if (state != 0) {
		cout << "Ack recived : " << received_ack.ackno << endl;
		return false;
	}
	return true;
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
	}
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

void congestion_control_selective() {
	uniform_real_distribution<> dis(0, 1);
	mt19937 gen(seed);
	char empty[] = "";
	bool finished = false;
	// This packet contains number of packets will the
	// Client receive
	packet p = create_packet(empty, 0);
	p.seqno = filePackets.size();
	send_packet(p);
	int packetCounter = 0, sent_packets_num, ack_counter;
	int acks[filePackets.size()];
	int total_acks = 0;
	state s = slow_start;
	while (!finished) {

		// Send packets to client and set timer for each one.
		int remained_size = (filePackets.size() - packetCounter);
		sent_packets_num = min(cwnSize, remained_size);
		filePackets.clear();
		packetsTimer.clear();
		int begin = packetCounter;
		for (int i = 0; i < sent_packets_num; i++) {
			struct p_timer t = { (uint32_t) packetCounter,
					chrono::system_clock::now() };
			packetsTimer.push_back(t);
			filePackets[packetCounter].seqno = packetCounter;
			float propToSend = dis(gen);
			if (propToSend > plp) {
				cout << "packet sent : " << filePackets[packetCounter].seqno
						<< endl;
				send_packet(filePackets[packetCounter]);
			} else {
				cout << "packet dropped seq no = "
						<< filePackets[packetCounter].seqno << endl;
			}
			packetCounter++;
		}

		// receive Acks from client
		ack_counter = begin;
		int received_acks = 0;
		while (ack_counter < packetCounter) {
			if (received_acks == sent_packets_num)
				break;
			ack_packet ack = recieve_ack_packet();
			if (ack.ackno == 0 && ack.check_sum == 0 && ack.len == 0) {
				acks[ack.ackno]++;
				if (acks[ack.ackno] == 1) {
					received_acks++;
					if (s == slow_start) {

					} else if (s == congestion_avoidance) {

					} else {
						s = congestion_avoidance;
					}
					cwnSize += 1;
				} else if (acks[ack.ackno] == 2) {
					received_acks--;
				} else {
					received_acks--;
					if (s == slow_start) {
						ssthreshold = cwnSize / 2;
						cwnSize = ssthreshold + 3;
						s = congestion_avoidance;
					} else if (s == congestion_avoidance) {
						ssthreshold = cwnSize / 2;
						cwnSize = ssthreshold + 3;
						s = fast_recovery;
					} else {

					}
					acks[ack.ackno] = 0;
					send_packet(filePackets[packetsTimer[ack_counter].seqno]);
				}
			}
			chrono::duration<double> elapsed_time = chrono::system_clock::now()
					- packetsTimer[ack.ackno].sentAt;

			if (acks[ack_counter] == 0 && (elapsed_time.count()) > 2) {
				cout << "Time out packet num:  "
						<< packetsTimer[ack_counter].seqno << "\n";
				ssthreshold = cwnSize / 2;
				cwnSize = 1;
				s = slow_start;
				send_packet(filePackets[packetsTimer[ack_counter].seqno]);
			}
			ack_counter++;
			if (ack_counter == packetCounter)
				ack_counter = begin;

		}
		total_acks += received_acks;
		cout << "cwinSIZE is " << cwnSize << endl;
		if (cwnSize >= ssthreshold)
			s = congestion_avoidance;

		if (total_acks >= filePackets.size() - 1)
			break;
	}
	cout << "Finished sending file " << endl;
}

ack_packet recieve_ack_packet() {
	struct ack_packet received_ack;
	struct pollfd pfd = { .fd = clientsock, .events = POLLIN };
	int state = poll(&pfd, 1, 1);
	recvfrom(clientsock, &received_ack, MAXBUFLEN, 0,
			(struct sockaddr*) &g_their_addr, &g_addr_len);
	if (state != 0) {
		cout << "Ack recived : " << received_ack.ackno << endl;
		return received_ack;
	}
	return empty_ack;
}

