#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <pthread.h>

#define USERS 5
#define MSG_LEN 1024

// custom bool datatype
typedef enum bool {false, true} bool;

// Client linked list node structure
typedef struct client_node *client_link;
typedef struct client_node {
	int sockfd;
	char ip[16];
	char username[31];
	char password[31];
	client_link next;
} client_node;

client_link head; // global linked list head

char *user_arr[USERS] = { "John", "Pieter", "Jimbo", "Bill", "Kim" };
char *pw_arr[USERS] = { "jp123", "pp123", "jp123", "bp123", "kp123" };

void append_node(client_link head, client_link new_node) {
	// move to end of the list
	while(head->next) {
		head = head->next;
	}	
	// append the new one into the list
	head->next = new_node;
}

void delete_node(client_link head, client_link deleted_node) {
	// move head to previous node of target
	while(head->next != deleted_node) {
		head = head->next;
		if(head->next == NULL) {
			// target is unfound
			return;
		}
	}
	// assign target's link to previous node's link and delete
	head->next = deleted_node->next;
	free(deleted_node);
}

int authenticate(char *usrname, char*pw) {
	int i;
	for(i=0;i<USERS;i++) { // search user pool
		if(strcmp(usrname, user_arr[i]) == 0) {
			if(strcmp(pw, pw_arr[i]) == 0) {
				// if username and password are found in saved user pool
				// return user's index
				return i;
			}
		}
	}
	// if authentication failed, return -1
	return -1;
}

void send_menu(int sockfd, char *message) {
	char buffer[MSG_LEN];
	strcpy(buffer, message);
	write(sockfd, buffer, strlen(buffer));
}

void client_thread(void *client_node_addr) {
	bool would_disconnect = false;
	bool signin_flag = true;
	char username[31];
	char password[31];
	char recv_buffer[MSG_LEN];
	char send_buffer[MSG_LEN];
	client_link np = (client_link)client_node_addr;
	int rcv;

	while(signin_flag) {
		// authenticate user
		// get username by input
		send_menu(np->sockfd, "***** Sign in *****\n");
		send_menu(np->sockfd, "Username: ");
	
		if(read(np->sockfd, username, 31) <= 0) {
			printf("Client(%s) didn't input name.\n", np->ip);
			would_disconnect = true;
		} else {
			// get password by input
			printf("%s sent %s\n", np->ip, username);
			send_menu(np->sockfd, "Password: ");
			if(read(np->sockfd, password, 31) <= 0) {
				printf("Client(%s) didn't input password.\n", np->ip);
				would_disconnect = true;
			}
			else {
				printf("%s sent %s\n", np->ip, password);
				// match username and password
				if(authenticate(username, password) != -1) {
					// authentication success
					signin_flag = false;
					printf("User '%s'(%s) connected\n", username, np->ip);

					// insert user info
					strcpy(np->username, username);
					strcpy(np->password, password);

					// send welcome message
					send_menu(np->sockfd, "*******************\n");
					sprintf(send_buffer, "Welcome, %s.\n", username);
					write(np->sockfd, send_buffer, strlen(send_buffer));
				}
				else {
					// authentication failed
					send_menu(np->sockfd, "*******************\nAuthentication failed! Retry? (y/n)\n");
					if(read(np->sockfd, recv_buffer, MSG_LEN) <= 0) {
						printf("Client(%s) didn't reply.\n", np->ip);
						would_disconnect = true;
						signin_flag = false;
					}
					printf("%s sent %s\n", np->ip, recv_buffer);

					while((strcmp(recv_buffer, "n") != 0) && (strcmp(recv_buffer, "y") != 0)) {
						// user didn't reply with (y/n)
						send_menu(np->sockfd, "Please reply with 'y' or 'n'.\n");
						if(read(np->sockfd, recv_buffer, MSG_LEN) <= 0) {
							printf("Client(%s) didn't reply.\n", np->ip);
							would_disconnect = true;
							signin_flag = false;
							break;
						}
						printf("%s sent %s\n", np->ip, recv_buffer);
					}
					if(strcmp(recv_buffer, "n") == 0) {
						// user replied with 'n'
						would_disconnect = true;
						signin_flag = false;
						send_menu(np->sockfd, "Goodbye\n");
					}
				}
			}
		}
	} // authenticating ends

	// Interacting
	while(!would_disconnect) {
		rcv = read(np->sockfd, recv_buffer, MSG_LEN);
		printf("%s sent %s\n", np->ip, recv_buffer);

		if(rcv > 0) { // message received
			write(np->sockfd, recv_buffer, strlen(recv_buffer));
		}
		else if(rcv == 0) {
			// disconnect
			would_disconnect = true;
		}
		else {
			// error
			printf("Error occured!\n");
			would_disconnect = true;
		}
	}

	// Delete node
	close(np->sockfd);
	delete_node(head, np);
}
 
int main(void)
{
  int listenfd = 0,connfd = 0;
  struct sockaddr_in serv_addr, client_addr;
  int serv_addrlen, client_addrlen;
  char sendBuff[1025];  
  int numrv;
  client_link new_client;

  // initialize head node of linked list
  head = (client_link)malloc(sizeof(client_node)); 
  head->next = NULL;
 
  //creating socket
  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if(listenfd == -1) {
	printf("Socket creating failed\n");
	return -1;
  }
  printf("Socket retrieving success\n");
  
  // setting socket
  serv_addrlen = sizeof(serv_addr);
  client_addrlen = sizeof(client_addr);
  memset(&serv_addr, '0', sizeof(serv_addr));
  memset(&client_addr, '0', sizeof(client_addr));
  memset(sendBuff, '0', sizeof(sendBuff));
  serv_addr.sin_family = AF_INET;    
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
  serv_addr.sin_port = htons(4111);    
 
  //bind socket and listen
  bind(listenfd, (struct sockaddr*)&serv_addr,sizeof(serv_addr));
  if(listen(listenfd, 10) == -1){
      printf("Failed to listen\n");
      return -1;
  }
     
  
  while(1) {      
	connfd = accept(listenfd, (struct sockaddr*)&client_addr ,(socklen_t*)&client_addrlen); // accept awaiting request

	// print client info
	getpeername(connfd, (struct sockaddr*) &client_addr, (socklen_t*) &client_addrlen);
	printf("Client %s:%d connected.\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
	
	// make a new node and initialize it
	new_client = (client_link)malloc(sizeof(client_node));
	new_client->sockfd = connfd;
	strcpy(new_client->ip, inet_ntoa(client_addr.sin_addr));
	strcpy(new_client->username, "null");
	strcpy(new_client->password, "null");
	new_client->next = NULL;
	// append newcomer to linked list
	append_node(head, new_client);

	// make a new thread
	pthread_t id;
	if(pthread_create(&id, NULL, (void *)client_thread, (void *)new_client) != 0) {
		perror("Failed creating pthread.\n");
		return -1;
	}

	/*
	//strcpy(sendBuff, "Message from server33");
	read(connfd, sendBuff, sizeof(sendBuff)); 
	printf("%s\n", sendBuff);
	write(connfd, sendBuff, strlen(sendBuff));
 
	close(connfd);    
	sleep(1);*/
  }
 
 
  return 0;
}