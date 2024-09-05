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
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>

#include <iostream>
#include <queue>
#include <math.h>
#include <errno.h>
#include <cstdio>

using namespace std;

#define TIMEOUT 50000 /*50 ms*/
#define MSS 8000

struct sockaddr_in si_other;
int s, slen;

void diep(char *s) {
    perror(s);
    exit(1);
}

enum state_t {SS, CA, FR};
enum pkt_type_t {DATA, ACK, FIN, FINACK};

typedef struct segheader_t {
    uint64_t seq;
    uint64_t ack;
    int window_size;
    int data_len;
    pkt_type_t type;
} segheader_t;

typedef struct packet_t {
    segheader_t header;
    char data[MSS];
} packet_t;

queue <packet_t> packet_queue;
float cwnd;
float ssthresh;
int seq;
int PacketCount, PacketSentCount, PacketReceivedCount;
int ByteReadCount;
int DuplicateCount;
state_t state;

void send_queue(FILE* fp){
    fprintf(stderr, "ByteReadCount: %d\n", ByteReadCount);
    if(ByteReadCount != 0){
        char recv_buf[MSS];
        memset(recv_buf, 0, MSS);
        packet_t packet;
        //int i = 0;
        
        while((cwnd - packet_queue.size() >= 0) && ByteReadCount != 0){
            int readSize = fread(recv_buf, sizeof(char), min(ByteReadCount, int(MSS)), fp);
            if(readSize > 0){
                packet.header.type = DATA;
                packet.header.data_len = readSize;
                packet.header.seq = seq;
                seq = seq + readSize;
                ByteReadCount = ByteReadCount - readSize;
                memcpy(packet.data, &recv_buf, readSize);
                packet_queue.push(packet);
                fprintf(stderr, "%d\n", ByteReadCount);
            }
            //for(i = packet_queue.size(); i > 0; i--){
                PacketSentCount++; //how many packets are in the queue to send
            //}
            //fprintf(stderr, "i: %d\n", i);
        }
    }
    else{
        return;
    }
}

void switch_state(){
    while(1){
        if(state == SS){
            if(DuplicateCount > 2){
                //dup ack handler
                ssthresh = cwnd/2;
                cwnd = 16;
                state = FR;
                if(!packet_queue.empty()){
                    sendto(s, &packet_queue.front(), sizeof(packet_t), 0, (struct sockaddr*)&si_other, sizeof(si_other));
                }
                DuplicateCount = 0;
            }
            else if(DuplicateCount == 0){
                cwnd = cwnd*2;
                if(cwnd >= ssthresh){
                    state = CA;
                    return;
                }
            }
            break;
        }
        else if(state == CA){
            if (DuplicateCount > 2){
                ssthresh = cwnd/2;
                cwnd = 16;
                state = FR;
                if(!packet_queue.empty()){
                    sendto(s, &packet_queue.front(), sizeof(packet_t), 0, (struct sockaddr*)&si_other, sizeof(si_other));
                }
                DuplicateCount = 0;
            }
            else if (DuplicateCount == 0){
                cwnd += 8;
            }
            break;
        }
        else if(state == FR){
            if (DuplicateCount > 0){
                cwnd += 8;
                return;
            }
            else if (DuplicateCount == 0){
                cwnd += 8;
                state = CA;
            }
            break;
        }
        else{
            break;
        }
        if(cwnd > 256) cwnd = 256;
    }
    return;
}

void ack_state(packet_t* packet, FILE* fp){
    if(packet->header.ack < packet_queue.front().header.seq){
        return;
    }
    else if(packet->header.ack == packet_queue.front().header.seq){
        DuplicateCount++;
        switch_state();
    }
    else{
        DuplicateCount = 0;
        switch_state();
        int PacketNumber = ceil((packet->header.ack - packet_queue.front().header.seq)/(1.0 * MSS));
        int counter = 0;
        PacketReceivedCount += PacketNumber;
        while(!packet_queue.empty() && counter < PacketNumber){
            packet_queue.pop();
            counter++;
        }
        fprintf(stderr, "Sanity check, ByteCount: %d\n", ByteReadCount);
        send_queue(fp);
    }
}

void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    //Open the file
    FILE *fp;
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf("Could not open file to send.");
        exit(1);
    }

	/* Determine how many bytes to transfer */

    slen = sizeof (si_other);

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_other, 0, sizeof (si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(hostUDPport);
    if (inet_aton(hostname, &si_other.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }


	/* Send data and receive acknowledgements on s*/

    //Create timeout tracker variable
    struct timeval time_val;
    time_val.tv_sec = 0;
    time_val.tv_usec = 100000;
    if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &time_val, sizeof(timeval)) == -1){
        diep("Error in setsockopt");
    }
    fprintf(stderr, "DID SOMETHING");
    //Split into two parts: 1. FSM
    //                      2. Functions on the FSM (send packets, receive ACKs from receiver)

    //CWND = 1 MSS but doubles every RTT until first loss
    packet_queue = {};
    cwnd = 8;
    seq = 0;
    ssthresh = 64;
    PacketCount = ceil(1.0 * bytesToTransfer/MSS);
    fprintf(stderr, "\nPacket Count: %d\n", PacketCount);
    ByteReadCount = bytesToTransfer;
    DuplicateCount = 0;
    state = SS;

    packet_t packet;
    send_queue(fp);
    while(PacketSentCount < PacketCount || PacketReceivedCount < PacketCount){
        if ((recvfrom(s, &packet, sizeof(packet_t), 0, NULL, NULL)) == -1){
            if (errno != EAGAIN || errno != EWOULDBLOCK) {
                diep("recvfrom()");
            }
            if (!packet_queue.empty()){
                ssthresh = cwnd / 2;
                cwnd = 16;
                state = SS;
                DuplicateCount = 0;

                queue <packet_t> temp_packet_queue = packet_queue; 
                for (int i = 0; i < 32; i++){
                    if (!temp_packet_queue.empty()){
                        sendto(s, &temp_packet_queue.front(), sizeof(packet_t), 0, (struct sockaddr*)&si_other, sizeof(si_other));
                        temp_packet_queue.pop();
                    }
                }
            }
        }
        else{
            if (packet.header.type == ACK){
                ack_state(&packet, fp);
            }
        }
        fprintf(stderr, "PacketSentCount:%d\n, PacketReceivedCount:%d\n, PacketCount:%d\n", PacketSentCount, PacketReceivedCount, PacketCount);
    }
    fprintf(stderr, "Reached EOC\n");
    packet_t EOCpacket;
    char EOCbuffer[sizeof(packet_t)];
    EOCpacket.header.type = FIN;
    EOCpacket.header.data_len = 0;
    memset(EOCpacket.data, 0, MSS);
    cout << "SENDING FIN" << endl;
    sendto(s, &EOCpacket, sizeof(packet_t), 0, (struct sockaddr*)&si_other, sizeof(si_other));
    while(1){
        if(recvfrom(s, EOCbuffer, sizeof(packet_t), 0, (struct sockaddr*)&si_other, (socklen_t*)&slen) == -1){
            if(errno != EAGAIN || errno != EWOULDBLOCK){
                diep("Error with recvfrom in end of connection");
            }
            else{
                //TIMEOUT
                EOCpacket.header.type = FIN;
                EOCpacket.header.data_len = 0;
                memset(EOCpacket.data, 0, MSS);
                sendto(s, &EOCpacket, sizeof(packet_t), 0, (struct sockaddr*)&si_other, sizeof(si_other));
            }
        }
        else{
            packet_t EOCack;
            memcpy(&EOCack, EOCbuffer, sizeof(packet_t));
            if(EOCack.header.type == FINACK){
                cout << "FINACK RECEIVED" << endl;
                EOCpacket.header.type = FINACK;
                EOCpacket.header.data_len = 0;
                sendto(s, &EOCpacket, sizeof(packet_t), 0, (struct sockaddr*)&si_other, sizeof(si_other));
                cout << "DONE SENDING PACKET" << endl;
                break;
            }
        }
        fprintf(stderr, "DID SOMETHING 3\n");
    }
    fclose(fp);
    return;
}

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;
    unsigned long long int numBytes;

    if (argc != 5) {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    udpPort = (unsigned short int) atoi(argv[2]);
    numBytes = atoll(argv[4]);



    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);


    return (EXIT_SUCCESS);
}