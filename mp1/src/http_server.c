/*
** server.c -- a stream socket server demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define PORT "3490"  // the port users will be connecting to

#define BACKLOG 10	 // how many pending connections queue will hold
#define MAXDATASIZE 100000

#define STR4 "HTTP/1.0 400 Bad Request\r\n"
#define HEAD "HTTP/1.1 200 OK\r\n\r\n"

void sigchld_handler(int s)
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char* argv[])
{
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;

	if(argc != 2){
		fprintf(stderr, "input: ./http_server <port>\n");
		exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	//fprintf(stderr, "p: %s\n, p_star: %s\n, servinfo: %s\n", p, *p, servinfo);

	printf("server: waiting for connections...\n");

	freeaddrinfo(servinfo); // all done with this structure


	while(1) {  // main accept() loop
		char buffer[MAXDATASIZE];
		char request_buf[MAXDATASIZE];
		char filename[MAXDATASIZE];
		char err_buff[MAXDATASIZE];
		memset(buffer, 0, sizeof(buffer));
		memset(request_buf, 0, sizeof(request_buf));
		memset(filename, '\0', sizeof(filename));
		memset(err_buff, '\0', sizeof(err_buff));
		sprintf(filename, HEAD);
		FILE* file;
		int num_bytes;
		int start_flag = 1;

		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
		printf("server: got connection from %s\n", s);
		
		rv = recv(new_fd, buffer, sizeof(buffer), 0);

		if (!fork()) { // this is the child process
			close(sockfd);
			
			if(strncmp(buffer, "GET ", 4) != 0){
				fprintf(stderr, "Invalid req");
				exit(1);
			}
			fprintf(stderr, "buffer: %s\n", buffer);
			int num_parents = 0;
			char* path_start = buffer + 4; //bypass "GET " and move to file name
			char* path_end = strchr(path_start, ' ');
			//if(path_end != NULL){
				size_t path_length = path_end - path_start;
				strncpy(request_buf, path_start, path_length);
				request_buf[path_length] = '\0';
			//}


			fprintf(stderr, "File requested: %s\n", request_buf);
			file = fopen(request_buf, "rb");
			if(file == NULL){
				fprintf(stderr, "null????");
				char full_path[MAXDATASIZE];
				memset(full_path, '\0', sizeof(full_path));
				int count = 0;
				char* request_ptr = request_buf;
				while(*request_ptr){
					if(*request_ptr == '/'){
						count++;
					}
					request_ptr++;
				}
				num_parents = count;
				fprintf(stderr, "numparents: %d\n", num_parents);
				snprintf(full_path, sizeof(full_path), "%s", request_buf);
				if(num_parents > 1){
					for(int i = 0; i < num_parents; i++){
						snprintf(full_path, sizeof(full_path), "../%s", request_buf);
					}
				}
				else{
					snprintf(full_path, sizeof(full_path), ".%s", request_buf);
				}
				fprintf(stderr, "full_path: %s\n", full_path);
				file = fopen(full_path, "rb");
				if(file == NULL){
					strcpy(err_buff, "HTTP/1.1 404 Whoops, file not found\r\n\r\n");
					if(send(new_fd, err_buff, strlen(err_buff), 0) == -1){
						perror("Cannot open file.");
						exit(1);
					}
				}
			}
			size_t send_result;
			//memset(filename, '\0', MAXDATASIZE);
			while(1){
				if(start_flag){
					num_bytes = fread(filename + strlen(HEAD), sizeof(char), MAXDATASIZE - strlen(HEAD), file);
					fprintf(stderr, "Filename: %s\n", filename);
					send_result = send(new_fd, filename, num_bytes + strlen(HEAD), 0);
					start_flag = 0;
				}
				else{
					num_bytes = fread(filename, sizeof(char), MAXDATASIZE, file);
					//fprintf(stderr, "Filename: %s\n", filename);
					send_result = send(new_fd, filename, num_bytes, 0);
				}
				if(num_bytes > 0){
					//fprintf(stderr, "Num_bytes is %d long\n", num_bytes);
					if(send_result == -1){
						printf("Error retrieving packets");
						break;
					}
				}
				else{
					break; //error or end of the file
				}
				//memset(filename, '\0', MAXDATASIZE);
			}
			fclose(file);
			close(new_fd);
			exit(0);
		}
		close(new_fd);  // parent doesn't need this
	}

	return 0;
}	

