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
	char sublist_fname[43];
	char twittlist_fname[45];
	int sublist_arr[USERS - 1];
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

void send_message(int sockfd, char *message) {
	char buffer[MSG_LEN];
	printf("Sent \n%s to %d..\n", message, sockfd);
	bzero(buffer, MSG_LEN);
	strcpy(buffer, message);
	write(sockfd, buffer, strlen(buffer));
}

void read_subscriptions(int user_idx, client_link np) {
	FILE *f;
	char filename[43];
	char tmp_buffer[5];
	bool make_new = false;
	int subscribe_idx = 0;

	sprintf(filename, "%d_subs.txt", user_idx);
	f = fopen(filename, "r");
	while (f == NULL) {
		printf("fopen failed!\n");
		f = fopen(filename, "w"); // if file doesn't exist, make a new one
		make_new = true;
	}
	
	if(!make_new) {
		while(fgets(tmp_buffer, 4, f) != NULL) { // read subscribe index
			tmp_buffer[strlen(tmp_buffer) - 1] = '\0'; // change '\n' in the last column to '\0'
			np->sublist_arr[subscribe_idx++] = atoi(tmp_buffer); // store into subscribe list array
		}
		for(;subscribe_idx < USERS - 1;subscribe_idx++) {
			np->sublist_arr[subscribe_idx] = -1; // store -1 into rest indices as a blank
		}
	}
	else {
		for(subscribe_idx = 0;subscribe_idx < USERS - 1;subscribe_idx++) {
			fprintf(f, "-1\n");
			np->sublist_arr[subscribe_idx] = -1; // store -1 into rest indices as a blank
		}
	}
	fclose(f);
}

void show_subscriptions(int user_idx, client_link np) {
	int i;
	char buffer[5];

	send_message(np->sockfd, "Your subscription list:\n");
	if(np->sublist_arr[0] == -1) {
		send_message(np->sockfd, "You subscribed no one! Add someone please.\n");
		return;
	}
	for(i=0;i<USERS-1;i++) {
		if(np->sublist_arr[i] == -1) {
			break;
		}
		bzero(buffer, 5);
		sprintf(buffer, "%s\n", user_arr[np->sublist_arr[i]]);
		write(np->sockfd, buffer, strlen(buffer));
	}
}

void add_subscription(int user_idx, client_link np) {
	char buffer[31];
	char recv_buffer[MSG_LEN];
	char filename[43];
	int i, j, rcv;
	FILE *f;
	
	// get user input
	send_message(np->sockfd, "Insert name of whom you want to subscribe: ");
	bzero(buffer, 31);
	if(rcv = read(np->sockfd, buffer, 31) < 0) {
		printf("Client(%s) didn't reply.\n", np->ip);
		return;
	}
	while(strlen(recv_buffer) == 0) { // doesn't work
		rcv = read(np->sockfd, buffer, 31);
	}
	buffer[strlen(buffer) - 1] = '\0';

	// search inserted name
	for(i=0;i<USERS;i++) {
		if(user_idx != i) { // search except client himself
			if(strcmp(buffer, user_arr[i]) == 0) { // detected
				// send confirmation message
				bzero(recv_buffer, MSG_LEN);
				sprintf(recv_buffer, "%s has been found: ID is %d. Is it corret?\n", buffer, i);
				write(np->sockfd, recv_buffer, strlen(recv_buffer));

				// get client's reply
				bzero(recv_buffer, MSG_LEN);
				if(rcv = read(np->sockfd, recv_buffer, MSG_LEN) < 0) {
					printf("Client(%s) didn't reply.\n", np->ip);
					return;
				}
				else if(rcv >=0) {
					while(strlen(recv_buffer) == 0) {
						rcv = read(np->sockfd, recv_buffer, 31);
					}
					//printf("got reply y/n %s sent %s\n", np->ip, recv_buffer);
					while((strcmp(recv_buffer, "n") != 0) && (strcmp(recv_buffer, "y") != 0)) {
						// user didn't reply with (y/n)
						send_message(np->sockfd, "Please reply with 'y' or 'n'.\n");
						bzero(recv_buffer, MSG_LEN);
						if(read(np->sockfd, recv_buffer, MSG_LEN) <= 0) {
							printf("Client(%s) didn't reply.\n", np->ip);
							break;
						}
						//printf("got reply y/n %s sent %s\n", np->ip, recv_buffer);
					}
					if(strcmp(recv_buffer, "n") == 0) {
						// user replied with 'n'
						send_message(np->sockfd, "Search another person..\n");
					}
					else {
						// user replied with 'y'
						for(j=0;j<USERS - 1;j++) {
							if(np->sublist_arr[j] == i) {
								// already subscribed
								send_message(np->sockfd, "Name exists in your subscription list!\n");
							}
							if(np->sublist_arr[j] == -1) { // add subscription into blank
								np->sublist_arr[j] = i;

								// append to the sublist file
								sprintf(filename, "%d_subs.txt", user_idx);
								f = fopen(filename, "a");
								fprintf(f, "%d\n", i);
								fclose(f);
								send_message(np->sockfd, "Subscribed");
								return;
							}
						}
					}
				}
			}
		}
	}
	// Couldn't find the name
	send_message(np->sockfd, "Couldn't find that name!");
}

void delete_subscription(int user_idx, client_link np) {
	char buffer[31];
	char recv_buffer[MSG_LEN];
	char filename[43];
	int i, j, rcv;
	FILE *f;
	
	// get user input
	send_message(np->sockfd, "Insert name of whom you want to delete subscription: ");
	bzero(buffer, 31);
	if(rcv = read(np->sockfd, buffer, MSG_LEN) < 0) {
		printf("Client(%s) didn't reply.\n", np->ip);
		return;
	}
	while(strlen(recv_buffer) == 0) {
		rcv = read(np->sockfd, buffer, MSG_LEN);
	}
	buffer[strlen(buffer) - 1] = '\0';

	// search inserted name
	for(i=0;i<USERS - 1;i++) {
		if(np->sublist_arr[i] == -1) {
			// couldn't find the name
			break;
		}
		if(strcmp(buffer, user_arr[np->sublist_arr[i]]) == 0) {
			// send confirmation message
			bzero(recv_buffer, MSG_LEN);
			sprintf(recv_buffer, "%s has been found: ID is %d. Is it corret?\n", buffer, np->sublist_arr[i]);
			write(np->sockfd, recv_buffer, strlen(recv_buffer));

			// get client's reply
			bzero(recv_buffer, MSG_LEN);
			if(rcv = read(np->sockfd, recv_buffer, MSG_LEN) < 0) {
				printf("Client(%s) didn't reply.\n", np->ip);
				return;
			}
			else if(rcv >=0) {
				while(strlen(recv_buffer) == 0) {
					rcv = read(np->sockfd, recv_buffer, 31);
				}
				//printf("got reply y/n %s sent %s\n", np->ip, recv_buffer);
				while((strcmp(recv_buffer, "n") != 0) && (strcmp(recv_buffer, "y") != 0)) {
					// user didn't reply with (y/n)
					send_message(np->sockfd, "Please reply with 'y' or 'n'.\n");
					bzero(recv_buffer, MSG_LEN);
					if(read(np->sockfd, recv_buffer, MSG_LEN) <= 0) {
						printf("Client(%s) didn't reply.\n", np->ip);
						return;
					}
					//printf("got reply y/n %s sent %s\n", np->ip, recv_buffer);
				}
				if(strcmp(recv_buffer, "n") == 0) {
					// user replied with 'n'
					send_message(np->sockfd, "Search another person..\n");
				}
				else {
					// user replied with 'y'
					for(j=i;j<USERS-2;j++) {
						// shift array left once
						np->sublist_arr[j] = np->sublist_arr[j+1];
					}
					// make last subscription blank
					np->sublist_arr[USERS-2] = -1;
	
					// edit sublist file
					sprintf(filename, "%d_subs.txt", user_idx);
					f = fopen(filename, "w");
					for(j=0;j<USERS - 1;j++) {
						if(np->sublist_arr[j] == -1) {
							break;
						}
						fprintf(f, "%d\n", np->sublist_arr[j]);
					}

					// close file and return
					fclose(f);
					send_message(np->sockfd, "Subscribed");
					return;
				}
			}
		}
	}
	// Couldn't find the name
	send_message(np->sockfd, "Couldn't find that name!");
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

enum menus {main_menu, write_menu, read_menu, subscribe_menu};
void send_menu(int sockfd, int current_menu) {
	char buffer[MSG_LEN];
	
	bzero(buffer, MSG_LEN);	
	switch(current_menu) {
		case main_menu:
		send_message(sockfd, "***** Main Menu *****\n1: Write a tweet\n2: Read tweets\n3: Set subscriptions\n4: Logout\n*********************\n");
		break;
		
		case write_menu:
		send_message(sockfd, "You can cancel by writing 'cancel'.\nWrite a message:\n");
		break;

		case read_menu:
		send_message(sockfd, "***** Read Menu *****\n1: Get all tweets\n2: Get unread tweets\n-1: Exit to Main Menu\n*********************\n");
		break;

		case subscribe_menu:
		send_message(sockfd, "***** Subscribe *****\n1: Show all subscriptions\n2: Add a subscription\n3: Delete a subscription\n-1: Exit to Main Menu\n*********************\n");
		break;
	}
}

void client_thread(void *client_node_addr) {
	bool would_disconnect = false;
	bool signin_flag = true;
	char username[31];
	char password[31];
	int user_idx;
	char recv_buffer[MSG_LEN];
	char send_buffer[MSG_LEN];
	client_link np = (client_link)client_node_addr;
	int rcv;
	int state = main_menu;
	int menu_selected;

	while(signin_flag) {
		// authenticate user
		// get username by input
		send_message(np->sockfd, "***** Sign in *****\nUsername: ");
	
		bzero(recv_buffer, 31);
		if(rcv = read(np->sockfd, username, 31) < 0) {
			printf("Client(%s) didn't input name.\n", np->ip);
			would_disconnect = true;
		} 
		else if(rcv >= 0) {
			while(strlen(username) == 0) {
				rcv = read(np->sockfd, username, 31);
			}
			// get password by input
			printf("got username: %s sent %s\n", np->ip, username);
			send_message(np->sockfd, "Password: ");
			bzero(password, 31);
			if(rcv = read(np->sockfd, password, 31) < 0) {
				printf("Client(%s) didn't input password.\n", np->ip);
				would_disconnect = true;
			}
			else if(rcv >= 0) {
				while(strlen(password) == 0) {
					rcv = read(np->sockfd, password, 31);
				}
				printf("got password: %s sent %s\n", np->ip, password);
				// match username and password
				if(user_idx = authenticate(username, password) != -1) {
					// authentication success
					signin_flag = false;
					printf("User '%s'(%s) connected\n", username, np->ip);

					// insert user info
					strcpy(np->username, username);
					strcpy(np->password, password);
					read_subscriptions(user_idx, np);

					// send welcome message
					send_message(np->sockfd, "Authenticated");
					send_message(np->sockfd, "*******************\n");
					bzero(send_buffer, MSG_LEN);
					sprintf(send_buffer, "Welcome, %s.\n", username);
					write(np->sockfd, send_buffer, strlen(send_buffer));
				}
				else {
					// authentication failed
					send_message(np->sockfd, "Failed");
					send_message(np->sockfd, "*******************\nAuthentication failed! Retry? (y/n)\n");
					bzero(recv_buffer, MSG_LEN);
					if(rcv = read(np->sockfd, recv_buffer, MSG_LEN) < 0) {
						printf("Client(%s) didn't reply.\n", np->ip);
						would_disconnect = true;
						signin_flag = false;
					}
					else if(rcv >=0) {
						while(strlen(recv_buffer) == 0) {
							rcv = read(np->sockfd, recv_buffer, 31);
						}
						printf("got reply y/n %s sent %s\n", np->ip, recv_buffer);
						while((strcmp(recv_buffer, "n") != 0) && (strcmp(recv_buffer, "y") != 0)) {
							// user didn't reply with (y/n)
							send_message(np->sockfd, "Please reply with 'y' or 'n'.\n");
							bzero(recv_buffer, MSG_LEN);
							if(read(np->sockfd, recv_buffer, MSG_LEN) <= 0) {
								printf("Client(%s) didn't reply.\n", np->ip);
								would_disconnect = true;
								signin_flag = false;
								break;
							}
							printf("got reply y/n %s sent %s\n", np->ip, recv_buffer);
						}
						if(strcmp(recv_buffer, "n") == 0) {
							// user replied with 'n'
							would_disconnect = true;
							signin_flag = false;
							send_message(np->sockfd, "Goodbye\n");
						}
						else {
							// user replied with 'y'
							send_message(np->sockfd, "retry");
						}
					}	
				}
			}
		}
	} // authenticating ends

	// Interacting
	while(!would_disconnect) {
		//send menu
		send_menu(np->sockfd, state);

		//read client's reply
		bzero(recv_buffer, MSG_LEN);
		if(rcv = read(np->sockfd, recv_buffer, MSG_LEN) >= 0) {
			while(strlen(recv_buffer) == 0) {
				rcv = read(np->sockfd, recv_buffer, MSG_LEN);
			}
			menu_selected = atoi(recv_buffer);

			switch(state) { // Transitions
				case main_menu:
				switch(menu_selected) {
					case 1:
					state = write_menu;
					break;

					case 2:
					state = read_menu;
					break;

					case 3:
					state = subscribe_menu;
					break;

					case 4:
					would_disconnect = true;
					send_message(np->sockfd, "Goodbye\n");
					break;

					default:
					send_message(np->sockfd, "Invalid input! Try again.\n");
					break;
				}
				break;

				case write_menu:
				if(strcmp(recv_buffer, "cancel") != 0) { // If client doesn't write 'cancel'
					//tweet_str(user_idx, recv_buffer); // post it
				}
				state = main_menu;
				break;

				case read_menu:
				switch(menu_selected) {
					case 1:
					//get_all_tweets(user_idx);
					state = main_menu;
					break;

					case 2:
					//get_unread_tweets(user_idx);
					state = main_menu;
					break;

					case -1:
					state = main_menu;
					break;

					default:
					send_message(np->sockfd, "Invalid input! Try again.\n");
					break;
				}
				break;

				case subscribe_menu:
				switch(menu_selected) {
					case 1:
					show_subscriptions(user_idx, np);
					break;

					case 2:
					add_subscription(user_idx, np);
					break;

					case 3:
					delete_subscription(user_idx, np);
					break;

					case -1:
					state = main_menu;
				}
				break;
			}

			switch(state) { // Actions
				
			}
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

  }
 
  return 0;
}