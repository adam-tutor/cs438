/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

//#define PORT "8888" // the port client will be connecting to 

#define MAXDATASIZE 100000 // max number of bytes we can get at once 

#define HEAD "GET /%s HTTP/1.1\r\nUser-Agent: Wget/1.12 (linux-gnu)\r\nHost: %s:%s\r\nConnection: Keep-Alive\r\n\r\n"

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
	int sockfd, numbytes;  
	char buf[MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];

	char url[200];
	char address[200];
	char port[10];
	char filename[200];
	memset(url, 0, sizeof(url));
	memset(address, 0, sizeof(address));
	memset(port, 0, sizeof(port));
	memset(filename, 0, sizeof(filename));

	if (argc != 2) {
	    fprintf(stderr,"usage: client hostname\n");
	    exit(1);
	}
	if (strncmp(argv[1], "http://", strlen("http://")) != 0) {
	    fprintf(stderr,"usage: client hostname\n");
	    exit(1);
	}
	int port_flag = 1;
	strcpy(url, argv[1]);
	/* Start parsing after the http://, which is the 7th pos of argv[1]/url */
	char *url_start = url + 7;
	/* Address ends, port begins at ':' */
	char *ip_end = strchr(url_start, ':');

	/* If there is no port, port is automatically set to 80 */
	if(ip_end == NULL){
		strcpy(port, "80");
		ip_end = strchr(url_start, '/');
		port_flag = 0;
	}

	/* Address is copied from end of HTTP to colon */
	strncpy(address, url_start, ip_end - url_start);
	address[ip_end - url_start] = '\0';
	
	char *file_start;
	/* Port is copied from colon to slash and start of file name */
	if(port_flag == 1){
		char *port_start = ip_end + 1;
		char *port_end = strchr(port_start, '/');
		strncpy(port, port_start, port_end - port_start);
		port[port_end - port_start] = '\0';
		file_start = port_end + 1;
	}
	else{
		file_start = ip_end + 1;
	}

	/* Rest of argv[1] is the filename and path */
	//char *file_start = port_end + 1;
	strcpy(filename, file_start);

	//segment into address -> happens after //
	//into port -> happens after :
	//into filename -> happens after /

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	//fprintf(stderr, "client: failed to connect p == NULL\nport: %sz\nservinfo: %sz\naddress: %sz\n, p: %sz\n", port, servinfo, address, p);
	//fprintf("wtf why wont this work %s", &servinfo);


	if ((rv = getaddrinfo(address, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect nut");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);
	printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure

	char request[MAXDATASIZE];
	memset(request, 0, sizeof(request));
	sprintf(request, HEAD, filename, address, port);
	fprintf(stderr, "Request: %s\n", request);
	
	send(sockfd, request, strlen(request), 0);
	/*if((numbytes = send(sockfd, request, strlen(request), 0)) == -1){
		perror("recv");
	}*/

	FILE* outputFile;
	if((outputFile=fopen("output","wb"))==NULL){
		perror("client: failed to create file");
		exit(1);
	}
	//fprintf(stderr, "numbytes: %d\n", numbytes);
	//fprintf(stderr, "Buffer: %s\n", buf);
	int check_flag = 0;
	memset(buf, '\0', MAXDATASIZE);
	while(1) {
		numbytes = recv(sockfd, buf, MAXDATASIZE, 0);
		//fprintf(stderr, "numbytes: %d\n", numbytes);
		if(numbytes > 0){
			if(!check_flag){
				char* pos_start = strstr(buf, "\r\n\r\n") + 4;
				fwrite(pos_start, 1, strlen(pos_start), outputFile);
				check_flag = 1;
			}
			else{
				fwrite(buf, 1, numbytes, outputFile);
			}
		}
		else{
			fclose(outputFile);
			break;
		}
	}

	printf("client: received '%s'\n",buf);

	//fclose(outputFile);
	close(sockfd);

	return 0;
}

