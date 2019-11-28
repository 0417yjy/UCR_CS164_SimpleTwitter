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

int sockfd = 0; // global sockfd variable

void recv_msg_thread() {
	char recv_buffer[MSG_LEN];
	int rcv;

	while(1) {
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
		// if(fscanf(send_buffer, MSG_LEN, stdin) != NULL) {
			scanf("%1023s ", send_buffer);
			write(sockfd, send_buffer, MSG_LEN);
		// }
	}
}
 
int main(void)
{
  int n = 0;
  char recvBuff[MSG_LEN];
  struct sockaddr_in serv_addr;
  //struct hostent *hen;
 
  // initialize buffer
  memset(recvBuff, '0' ,sizeof(recvBuff));

  // create socket
  if((sockfd = socket(AF_INET, SOCK_STREAM, 0))< 0)
    {
      printf("\n Error : Could not create socket \n");
      return 1;
    }
  //hen = gethostbyname("server.jongyeon.cs164");
  /*hen = gethostbyaddr("10.0.0.4", 4, AF_INET);
  if(hen == NULL) {
    fprintf(stdout, "Host not found");
    exit(1);
  }*/
 
  // socket setting
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(4111);
  serv_addr.sin_addr.s_addr = inet_addr("10.0.0.4");
  //bcopy((char *)hen->h_addr,(char *)&serv_addr.sin_addr.s_addr, hen->h_length);
 
  // connect to server
  if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))<0)
    {
      printf("\n Error : Connect Failed \n");
      return 1;
    }

  /*fgets(recvBuff,MSG_LEN - 1, stdin);
  write(sockfd, recvBuff, sizeof(recvBuff));
 
  while((n = read(sockfd, recvBuff, sizeof(recvBuff)-1)) > 0) {
      recvBuff[n] = 0;
      if(fputs(recvBuff, stdout) == EOF) {
      	printf("\n Error : Fputs error");
      }
      printf("\n");
  }
 
  if( n < 0)
    {
      printf("\n Read Error \n");
    }
  (*/

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
 
  return 0;
}