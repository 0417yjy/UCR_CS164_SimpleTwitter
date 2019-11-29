#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <pthread.h>

#define MSG_LEN 1024

//shared variables
int sockfd = 0;

void recv_msg_thread() {
	char recv_buffer[MSG_LEN];
	int rcv;

	while(1) {
		bzero(recv_buffer, MSG_LEN);
		rcv = read(sockfd, recv_buffer, MSG_LEN);
		if(rcv > 0) {
			// message received
			printf("%s", recv_buffer);
		}
		else if(rcv == 0) {
			break;
		}
		else {
			break;
		}
	}
}

void send_msg_thread() {
	char send_buffer[MSG_LEN];
	while(1) {
		bzero(send_buffer, MSG_LEN);
		if(fgets(send_buffer, MSG_LEN, stdin) != NULL) {
			send_buffer[strlen(send_buffer) - 1] = '\0';
			write(sockfd, send_buffer, MSG_LEN);
		}
	}
}

inline void print_msg() {
	int n;
	char recvBuff[MSG_LEN];

	//printf("getting a message...\n");
	bzero(recvBuff, MSG_LEN);
	if(n = read(sockfd, recvBuff, MSG_LEN - 1) >= 0) {
		while(strlen(recvBuff) == 0) {
			n = read(sockfd, recvBuff, MSG_LEN - 1);
		}
		printf("%s", recvBuff);
  	}
	else if(n < 0) {
		printf("\n Read Error\n");
	}
}

int client_authenticate() {
	int i, n;
	char buffer[MSG_LEN];

	// get 1 message
	print_msg(); // print menu and query for username

	// send username
	bzero(buffer, MSG_LEN);
	fgets(buffer, MSG_LEN - 1, stdin);
	buffer[strlen(buffer) - 1] = '\0';
  	write(sockfd, buffer, sizeof(buffer));
	
	// get 1 message
	print_msg(); // print query for password

	// send password
	bzero(buffer, MSG_LEN);
	fgets(buffer, MSG_LEN - 1, stdin);
	buffer[strlen(buffer) - 1] = '\0';
  	write(sockfd, buffer, sizeof(buffer));

	// get 1 message
	bzero(buffer, MSG_LEN);
	if(n = read(sockfd, buffer, sizeof(buffer) - 1) >= 0) {
		while(strlen(buffer) == 0) {
			n = read(sockfd, buffer, MSG_LEN - 1);
		}
	}

	if(strcmp(buffer, "Authenticated") == 0) { // "Authenticated" received
		printf("authenticated\n");
		return 1;
	}
	else if(strcmp(buffer, "Failed") == 0) { // "Failed" received
		print_msg(); // get 1 message

		// send y or n
		do {
			bzero(buffer, MSG_LEN);
			fgets(buffer, MSG_LEN - 1, stdin);
			buffer[strlen(buffer) - 1] = '\0';
  			write(sockfd, buffer, sizeof(buffer));

			bzero(buffer, MSG_LEN);
			read(sockfd, buffer, sizeof(buffer) - 1);
			if(strcmp(buffer, "Goodbye\n") == 0) {
				printf("%s", buffer);
				return 0;
			}
		} while(strcmp(buffer, "retry") != 0);
		return 2; // flag for retry
	}
}
 
int main(void)
{
  int n = 0;
  int auth_flag;
  char recvBuff[MSG_LEN];
  struct sockaddr_in serv_addr;
 
  // initialize buffer
  memset(recvBuff, '0' ,sizeof(recvBuff));

  // create socket
  if((sockfd = socket(AF_INET, SOCK_STREAM, 0))< 0)
    {
      printf("\n Error : Could not create socket \n");
      return 1;
    }
 
  // socket setting
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(4111);
  serv_addr.sin_addr.s_addr = inet_addr("10.0.0.4");
 
  // connect to server
  if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))< 0)
    {
      printf("\n Error : Connect Failed \n");
      return 1;
    }

  // authenticating
  do {
  	auth_flag = client_authenticate();
  } while(auth_flag == 2); // keep retrying

  // authenticating succeed
  printf("authenticating ends: %d\n", auth_flag);
  if(auth_flag == 1) {
	printf("making threads\n");
  	pthread_t p_send;
  	if(pthread_create(&p_send, NULL, (void *)send_msg_thread, NULL) != 0) {
		printf("Creating pthread error\n");
		return -1;
 	}

  	pthread_t p_recv;
 	if(pthread_create(&p_recv, NULL, (void *)recv_msg_thread, NULL) != 0) {
		printf("Creating pthread error\n");
		return -1;
 	}

 	while(1) { }
  }

  return 0;
}