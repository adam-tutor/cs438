#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>

#include <iostream>
#include <queue>

#define MSS 8000

using namespace std;

struct sockaddr_in si_me, si_other;
int s, slen;

void diep(char *s) {
    perror(s);
    exit(1);
}

enum state_t {SS, CA, FR};
enum pkt_type_t {DATA, ACK, FIN, FINACK};

typedef struct {
    uint64_t seq;
    uint64_t ack;
    int window_size;
    int data_len;
    pkt_type_t type;
} segheader_t;

typedef struct {
    segheader_t header;
    char data[MSS];
} packet_t;


typedef struct {
    bool operator()(packet_t a, packet_t b){
        return a.header.seq > b.header.seq;
    }
} compare;

priority_queue<packet_t, vector<packet_t>, compare> packet_queue;

void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
    
    slen = sizeof (si_other);


    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_me, 0, sizeof (si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    printf("Now binding\n");
    if (bind(s, (struct sockaddr*) &si_me, sizeof (si_me)) == -1)
        diep("bind");


	/* Now receive data and send acknowledgements */  
    int recv_length = 0; 
    int expected_ack = 0;
    int termination = 0;
    FILE* fp = fopen(destinationFile, "wb");
    if(fp == NULL){
        diep("Error: Could not open file.");
        fprintf(stderr, "Error: Could not open file.");
    }
    fprintf(stderr, "did something");
    while(1){
        //First handshake
        packet_t recv_packet;
        packet_t ack;
        recv_length = (recvfrom(s, &recv_packet, sizeof(packet_t), 0, (sockaddr *)&si_other, (socklen_t*)&slen));
        if(recv_length == -1){
            diep("Error in recvfrom()");
        }
        fprintf(stderr, "Packet type: %d\n", recv_packet.header.type);
        if(recv_packet.header.type == FIN){
            cout << "FIN" << endl;
            ack.header.ack = expected_ack;
            ack.header.type = FINACK;
            if(sendto(s, &ack, sizeof(packet_t), 0, (sockaddr*)&si_other, (socklen_t)sizeof(si_other)) == -1){
                diep("ACK's sendto()");
            }
            break;
        }
        else if(recv_packet.header.type == DATA){
            fprintf(stderr, "expected_ack: %d\n", expected_ack);
            if(recv_packet.header.seq > expected_ack){
                if(packet_queue.size() < 1000){
                    packet_queue.push(recv_packet);
                }
            }
            else{
                fwrite(recv_packet.data, sizeof(char), recv_packet.header.data_len, fp);
                fprintf(stderr, "Packet data: %s\n", recv_packet.data);
                expected_ack += recv_packet.header.data_len;
                fprintf(stderr, "expected_ack after: %d\n", expected_ack);
                fprintf(stderr, "Packet Queue Size: %ld\n", packet_queue.size());
                while(!packet_queue.empty() && (packet_queue.top().header.ack == expected_ack)){
                    fwrite((packet_queue.top().data), sizeof(char), (packet_queue.top().header.data_len), fp);
                    fprintf(stderr, "Packet data: %s\n", packet_queue.top().data);

                    expected_ack += (packet_queue.top().header.data_len);
                    packet_queue.pop();
                }
            }
            ack.header.ack = expected_ack;
            ack.header.type = ACK;
            if(sendto(s, &ack, sizeof(packet_t), 0, (sockaddr*)&si_other, (socklen_t)sizeof(si_other)) == -1){
                diep("ACK's sendto()");
            }
        }
        fprintf(stderr, "did something here");
    }
    fclose(fp);

    close(s);
		printf("%s received.\n", destinationFile);
    return;
}

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;

    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    udpPort = (unsigned short int) atoi(argv[1]);

    reliablyReceive(udpPort, argv[2]);
}