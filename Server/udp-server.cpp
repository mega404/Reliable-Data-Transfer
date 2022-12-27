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

using namespace std;
#define MYPORT "4950"    // the port users will be connecting to
#define MAXBUFLEN 509


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

vector<packet> filePackets;

//0 is for slowStart
//1 is for cognestion avoidance
//2 is for Fast recovery
int state = 0;

int cwnSize = 1;
int ssthreshold = 63;

//int timerArary[] = new timerArray[cwnSize];

void read_input_file(char *path, char args[][1024]);
void create_file_packets(int sockfd, char *path);
packet create_packet(char data[], int size);
void send_packet(int sockfd, struct packet sent_packet);

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
	socklen_t addr_len;
	char s[INET6_ADDRSTRLEN];

    int seed = stoi(argv[1]);
    mt19937 gen(seed);

    //cout << "arg 3 is " <<argv[2]<<endl;
    float plp = stof(argv[2]);
    //istringstream(argv[3]) >> plp;
    cout<<"plp is "<<plp<<endl;
    //srand(seed);

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
	while (1) {
		struct packet file_name;
		addr_len = sizeof their_addr;
		if ((numbytes = recvfrom(sockfd, &file_name, MAXBUFLEN - 1, 0,
				(struct sockaddr*) &their_addr, &addr_len)) == -1) {
			perror("recvfrom");
			exit(1);
		}
		cout << "Server got new connection\n";
		int status = 0;
		if (!fork()) {
			g_their_addr = their_addr;
			g_addr_len = addr_len;
            create_file_packets(sockfd, file_name.data);
            int packetCounter=0;
            uniform_real_distribution<> dis(0,1);
            while(packetCounter < filePackets.size()) {
                int remained_size =  (filePackets.size()-packetCounter);
                //int ackToRecieve[cwnSize];
                //int ackCounter = 0;

                for (int i=0; i<min(cwnSize, remained_size); i++) {
                    filePackets[packetCounter].seqno = packetCounter;

                    float propToSend = dis(gen);

                    //cout<<"propToSend : "<<propToSend<<endl;
                    if(propToSend > plp) {
                        cout << "packet sent : " << filePackets[packetCounter].seqno << endl;
                        send_packet(sockfd, filePackets[packetCounter]);
                    }
                    struct ack_packet received_ack;
                    /*if ((numbytes = recvfrom(sockfd, &received_ack, MAXBUFLEN, 0,
                                             (struct sockaddr*) &their_addr, &addr_len)) == -1) {
                        perror("recvfrom");
                        exit(1);
                    }
                    cout<<"Ack recived : "<<received_ack.ackno<<endl;*/
                    /*if (received_ack.ackno == filePackets[packetCounter].seqno+1) {
                        //ackCounter2++;
                        cout<<"Ack recived : "<<received_ack.ackno<<endl;
                        //continue;
                    }*/
                    packetCounter++;
                    //ackToRecieve[ackCounter] = packetCounter;
                    //ackCounter++;
                }

                /*int ackCounter2=0;
                for (int i=0; i<ackCounter; i++){
                    struct ack_packet received_ack;
                    if ((numbytes = recvfrom(sockfd, &received_ack, MAXBUFLEN, 0,
                                             (struct sockaddr*) &their_addr, &addr_len)) == -1) {
                        perror("recvfrom");
                        exit(1);
                    }
                    if (received_ack.ackno == ackToRecieve[ackCounter2]) {
                        ackCounter2++;
                        cout<<"Ack recived : "<<received_ack.ackno<<endl;
                        //continue;
                    }
                }*/

                cout<<"cwinSIZE is " << cwnSize << endl;
                if (state == 0) {
                    cwnSize *= 2;
                }else {
                    cwnSize += 1;
                }
                if (cwnSize >= ssthreshold) state = 1;
            }
            cout<<"Finished sending file "<<endl;

			exit(0);
		}
	}
	int returnStatus;
	waitpid(0, &returnStatus, 0);
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

void create_file_packets(int sockfd, char *path) {
	FILE *fileptr;
	long filelen;
	int numbytes;
	struct packet file_packet;
	fileptr = fopen(path, "rb");  // Open the file in binary mode
	fseek(fileptr, 0, SEEK_END);          // Jump to the end of the file
	filelen = ftell(fileptr);         // Get the current byte offset in the file
	rewind(fileptr);
	char send_buffer[500]; // no link between BUFSIZE and the file size
	int nb = fread(send_buffer, 1, 500, fileptr);
	file_packet = create_packet(send_buffer, nb);

	while (!feof(fileptr)) {
        filePackets.push_back(file_packet);
		nb = fread(send_buffer, 1, 500, fileptr);
        file_packet = create_packet(send_buffer, nb);
	}

    filePackets.push_back(file_packet);
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

void send_packet(int sockfd, struct packet sent_packet) {
	int numbytes;
	if ((numbytes = sendto(sockfd, &sent_packet, sizeof(sent_packet), 0,
			(struct sockaddr*) &g_their_addr, g_addr_len)) == -1) {
		perror("talker: sendto");
		exit(1);
	}
}
