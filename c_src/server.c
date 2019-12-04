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
	int sublist_arr[USERS - 1];
	int last_read_twit_idx;
	int twit_idx_from_current_session;
	client_link next;
} client_node;

// Twit string linked list node structure
typedef struct twit_node *twit_link;
typedef struct twit_node {
	int twit_idx;
	int user_idx;
	char twit_str[141]; // twit length is 140 or less
	char *hashtag_list[11]; // hashtags can be 10 at most
	twit_link next;
	twit_link prev;
} twit_node;

client_link client_head; // global linked list client_head
twit_link twit_head; // global linked list twit_head
twit_link twit_tail; // global linked list twit_tail

char *user_arr[USERS] = { "John", "Pieter", "Jimbo", "Bill", "Kim" };
char *pw_arr[USERS] = { "jp123", "pp123", "jp123", "bp123", "kp123" };

pthread_mutex_t fwrite_mutex;

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
	printf("Sent a message to %d..\n", sockfd);
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

	np->twit_idx_from_current_session = twit_tail->twit_idx + 1; // set twit idx from current session
	if(!make_new) {
		if(fgets(tmp_buffer, 4, f) != NULL) { // read last read twit idx
			tmp_buffer[strlen(tmp_buffer) - 1] = '\0'; // change '\n' in the last column to '\0'
			np->last_read_twit_idx = atoi(tmp_buffer); // store into last read twit idx
		}

		while(fgets(tmp_buffer, 4, f) != NULL) { // read subscribe index
			tmp_buffer[strlen(tmp_buffer) - 1] = '\0'; // change '\n' in the last column to '\0'
			np->sublist_arr[subscribe_idx++] = atoi(tmp_buffer); // store into subscribe list array
		}
		for(;subscribe_idx < USERS - 1;subscribe_idx++) {
			np->sublist_arr[subscribe_idx] = -1; // store -1 into rest indices as a blank
		}
	}
	else {
		fprintf(f, "0\n"); // write 0 as the last read twit number
		for(subscribe_idx = 0;subscribe_idx < USERS - 1;subscribe_idx++) {
			fprintf(f, "-1\n");
			np->sublist_arr[subscribe_idx] = -1; // store -1 into rest indices as a blank
		}
	}
	fclose(f);
}

void edit_client_file(int user_idx, client_link np) {
	FILE *f;
	char filename[30];
	int i;

	sprintf(filename, "%d_subs.txt", user_idx);
	f = fopen(filename, "w");
	if(!f) {
		printf("fopen failed!\n");
		return;
	}

	np->last_read_twit_idx = twit_tail->twit_idx;
	printf("printing %d as last index...\n", np->last_read_twit_idx);
	fprintf(f, "%d\n", np->last_read_twit_idx);
	for(i=0;i<USERS - 1;i++) {
		fprintf(f, "%d\n", np->sublist_arr[i]);
	}
	fclose(f);
}

void show_subscriptions(int user_idx, client_link np) {
	int i;
	char buffer[MSG_LEN];

	send_message(np->sockfd, "Your subscription list:\n");
	if(np->sublist_arr[0] == -1) {
		send_message(np->sockfd, "You subscribed no one! Add someone please.\n");
		return;
	}
	for(i=0;i<USERS-1;i++) {
		if(np->sublist_arr[i] == -1) {
			break;
		}
		bzero(buffer, MSG_LEN);
		sprintf(buffer, "%s\n", user_arr[np->sublist_arr[i]]);
		write(np->sockfd, buffer, strlen(buffer));
	}
}

void add_subscription(int user_idx, client_link np) {
	char input_buffer[MSG_LEN];
	char recv_buffer[MSG_LEN];
	char filename[43];
	int i, j, rcv;
	FILE *f;

	// get user input
	printf("Add command from user '%s':%d\n", user_arr[user_idx], user_idx);
	send_message(np->sockfd, "Insert name of whom you want to subscribe: ");
	bzero(input_buffer, MSG_LEN);
	if(rcv = read(np->sockfd, input_buffer, MSG_LEN) < 0) {
		printf("Client(%s) didn't reply.\n", np->ip);
		return;
	}
	while(strlen(input_buffer) == 0) {
		rcv = read(np->sockfd, input_buffer, MSG_LEN);
	}
	input_buffer[strlen(input_buffer)] = '\0';
	printf("Got '%s' from %d\n", input_buffer, np->sockfd);

	// search inserted name
	for(i=0;i<USERS;i++) {
		if(user_idx != i) { // search except client himself
			printf("Comparing %s and %s...\n", input_buffer, user_arr[i]);
			if(strcmp(input_buffer, user_arr[i]) == 0) { // detected
				// send confirmation message
				bzero(recv_buffer, MSG_LEN);
				sprintf(recv_buffer, "%s has been found: ID is %d. Is it corret?\n", input_buffer, i);
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
								f = fopen(filename, "w");
								fprintf(f, "%d\n", np->last_read_twit_idx);
								for(j=0;j<USERS - 1;j++) {
									printf("fprinting %d to the file..\n", np->sublist_arr[j]);
									fprintf(f, "%d\n", np->sublist_arr[j]);
								}
								fclose(f);
								send_message(np->sockfd, "Subscribed\n");
								return;
							}
						}
					}
				}
			}
		}
	}
	// Couldn't find the name
	send_message(np->sockfd, "Couldn't find that name!\n");
}

void delete_subscription(int user_idx, client_link np) {
	char buffer[MSG_LEN];
	char recv_buffer[MSG_LEN];
	char filename[43];
	int i, j, rcv;
	FILE *f;

	// get user input
	printf("Delete command from user '%s':%d\n", user_arr[user_idx], user_idx);
	send_message(np->sockfd, "Insert name of whom you want to delete subscription: ");
	bzero(buffer, MSG_LEN);
	if(rcv = read(np->sockfd, buffer, MSG_LEN) < 0) {
		printf("Client(%s) didn't reply.\n", np->ip);
		return;
	}
	while(strlen(buffer) == 0) {
		rcv = read(np->sockfd, buffer, MSG_LEN);
	}
	buffer[strlen(buffer)] = '\0';
	printf("Got '%s' from %d\n", buffer, np->sockfd);

	// search inserted name
	for(i=0;i<USERS - 1;i++) {
		if(np->sublist_arr[i] == -1) {
			// couldn't find the name
			break;
		}
		printf("Comparing %s and %s...\n", buffer, user_arr[i]);
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
					fprintf(f, "%d\n", np->last_read_twit_idx);
					for(j=0;j<USERS - 1;j++) {
						fprintf(f, "%d\n", np->sublist_arr[j]);
					}

					// close file and return
					fclose(f);
					send_message(np->sockfd, "Subscription deleted\n");
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
		printf("i = %d, User is '%s'\n", i, user_arr[i]);
		if(strcmp(usrname, user_arr[i]) == 0) {
			if(strcmp(pw, pw_arr[i]) == 0) {
				// if username and password are found in saved user pool
				// return user's index
				printf("returning %d\n", i);
				return i;
			}
		}
	}
	// if authentication failed, return -1
	return -1;
}

enum menus {main_menu, write_menu, read_menu, subscribe_menu, hashtag_menu, hashsearch_menu, getoffline_super_menu, getoffline_part_menu};
void send_menu(int sockfd, int current_menu) {
	char buffer[MSG_LEN];

	bzero(buffer, MSG_LEN);
	switch(current_menu) {
		case main_menu:
		send_message(sockfd, "***** Main Menu *****\n1: Write a twit\n2: Read twits\n3: Set subscriptions\n4: Logout\n*********************\n");
		break;

		case write_menu:
		send_message(sockfd, "You can cancel by writing 'cancel'.\nWrite a message:\n");
		break;

		case read_menu:
		send_message(sockfd, "***** Read Menu *****\n1: Get twits\n2: See Offline Messages\n3: Hashtag Search\n-1: Exit to Main Menu\n*********************\n");
		break;

		case subscribe_menu:
		send_message(sockfd, "***** Subscribe *****\n1: Show all subscriptions\n2: Add a subscription\n3: Delete a subscription\n-1: Exit to Main Menu\n*********************\n");
		break;

		case hashtag_menu:
		send_message(sockfd, "Add hashtag. Enter '!e' to end adding hashtags: ");
		break;

		case hashsearch_menu:
		send_message(sockfd, "Write a hashtag you want to search: ");
		break;

		case getoffline_super_menu:
		send_message(sockfd, "***** Get Offline Messages *****\n1: Get all messages\n2: Get messages from a particular subscription\n********************************\n");
		break;

		case getoffline_part_menu:
		send_message(sockfd, "Write a name you want to get twits of: ");
		break;
	}
}

void send_a_twit(client_link target, twit_link tp) {
	char buffer[MSG_LEN];
	int j;

	bzero(buffer, MSG_LEN);
	sprintf(buffer, "[%d]%s: %s", tp->twit_idx, user_arr[tp->user_idx], tp->twit_str);
	j = 0;
	while(tp->hashtag_list[j]) {
		if(strcmp(tp->hashtag_list[j], "!e") == 0) {
			break;
		}
		strcat(buffer, " #");
		strcat(buffer, tp->hashtag_list[j]);
		j++;
	}
	strcat(buffer, "\n");
	write(target->sockfd, buffer, strlen(buffer));
}

void send_all_subscribers(twit_link tp) {
	int i;
	client_link p = client_head;
	int writer_idx = tp->user_idx;
	char buffer[MSG_LEN];

	while(p) {
		for(i=0;i<USERS - 1;i++) {
			if(p->sublist_arr[i] == -1) { // end of subscription list
				break;
			}
			if(p->sublist_arr[i] == writer_idx) {
				bzero(buffer, MSG_LEN);
				sprintf(buffer, "%s posted a twit just now:\n", user_arr[writer_idx]);
				write(p->sockfd, buffer, strlen(buffer));
				send_a_twit(p, tp);
				break;
			}
		}
		p = p->next; // find next client
	}
}

twit_link twit_str(int user_idx, char *twit) {
	twit_link new_twit;

	printf("Start posting: %d:%s\n", user_idx, twit);
	new_twit = (twit_link)malloc(sizeof(twit_node)); // make a new node

	//printf("Assign a new one to tail's next...\n");
	twit_tail->next = new_twit;

	//printf("Setting the newcomer...\n");
	// set the new one and append to the twit list
	new_twit->prev = twit_tail;
	new_twit->next = NULL;
	new_twit->user_idx = user_idx;
	new_twit->twit_idx = new_twit->prev->twit_idx + 1;
	strcpy(new_twit->twit_str, twit);

	// make the new one tail of the list
	twit_tail = new_twit;
	return new_twit;
}

void insert_hashtag(twit_link p, char *hashtag_buffer) {
	bool endflag_detected = false;
	char tmp_buffer[313];
	char *token;
	int i = 0;
	FILE *f;
	strcpy(tmp_buffer, hashtag_buffer);

	token = strtok(hashtag_buffer, ";");
	while(token != NULL) {
		printf("token : %s\n", token);
		p->hashtag_list[i] = (char*)malloc(sizeof(char) * 31);
		strcpy(p->hashtag_list[i], token);
		if(strcmp(token, "!e") == 0) {
			break;
		}
		i++;
		token = strtok(NULL, ";");
	}

	printf("Get a lock to edit twitlist...\n");
	// append to the twitlist.txt
	pthread_mutex_lock(&fwrite_mutex);
		f = fopen("twitlist.txt", "a");
		printf("%d\n%s\n%s\n", p->user_idx, p->twit_str, tmp_buffer);
		fprintf(f, "%d\n%s\n%s\n", p->user_idx, p->twit_str, tmp_buffer);
		fclose(f);
	pthread_mutex_unlock(&fwrite_mutex);
	printf("Editing twitlist completed!\n");

	send_all_subscribers(p);
}

void get_twits(client_link np) {
	char buffer[MSG_LEN];
	int rcv;
	int num_getting;
	int i, j;
	twit_link tp;
	bool send_this_twit;

	// get user input
	send_message(np->sockfd, "How many twits you want to get?: ");
	bzero(buffer, MSG_LEN);
	if(rcv = read(np->sockfd, buffer, MSG_LEN) < 0) {
		printf("Client(%s) didn't reply.\n", np->ip);
		return;
	}
	while(strlen(buffer) == 0) {
		rcv = read(np->sockfd, buffer, MSG_LEN);
	}
	buffer[strlen(buffer)] = '\0';
	printf("Got %s from %d\n", buffer, np->sockfd);

	num_getting = atoi(buffer);
	if(!num_getting) {
		send_message(np->sockfd, "Getting twits canceled!\n");
	}
	else {
		// initialize variables
		tp = twit_tail; // start from the tail of the twit list
		i = 0;

		while(i < num_getting) {
			send_this_twit = false;
			// check whether getter subscribed this twit's owner
			for(j=0;j<USERS - 1;j++) {
				if(np->sublist_arr[j] == tp->user_idx) {
					// found on the subilst. send it
					send_this_twit = true;
					send_a_twit(np, tp);
				}
				else if(np->sublist_arr[j] == -1) {
					// couldn't find this twit's owner in getter's sublist
					break;
				}
			}
			if(send_this_twit) {
				i++;
			}
			if(tp->twit_idx > 0) {
				tp = tp->prev;
			}
			else {
				send_message(np->sockfd, "Searched all twits!\n");
				break;
			}
		}
	}
}

void search_hashtag(client_link np, char *hashtag) {
	twit_link tp = twit_tail;
	int twit_cnt = 0;
	bool send_this_twit;
	int i, j;

	while(twit_cnt < 10) {
		send_this_twit = false;
		// check whether getter subscribed this twit's owner
		for(j=0;j<USERS - 1;j++) {
			if(np->sublist_arr[j] == tp->user_idx) {
				// found on the subilst. check the hashtag.
				for(i=0;i<10;i++) {
					if(strcmp(hashtag, tp->hashtag_list[i]) == 0) {
						send_this_twit = true;
						send_a_twit(np, tp);
						break;
					}
					else if(strcmp("!e", tp->hashtag_list[i]) == 0) {
						break;
					}
				}
			}
			else if(np->sublist_arr[j] == -1) {
				// couldn't find this twit's owner in getter's sublist
				break;
			}
		}
		if(send_this_twit) {
			twit_cnt++;
		}
		if(tp->twit_idx > 0) {
			tp = tp->prev;
		}
		else {
			send_message(np->sockfd, "Searched all twits!\n");
			break;
		}
	}
}

void get_unread_all(client_link np) {
	twit_link tp = twit_tail;
	int current_twit_idx = np->twit_idx_from_current_session - 1;
	int j;

	while(np->last_read_twit_idx < current_twit_idx) {
		// check whether getter subscribed this twit's owner
		for(j=0;j<USERS - 1;j++) {
			if(np->sublist_arr[j] == tp->user_idx) {
				// found on the subilst. send it
				send_a_twit(np, tp);
			}
			else if(np->sublist_arr[j] == -1) {
				// couldn't find this twit's owner in getter's sublist
				break;
			}
		}
		if(tp->twit_idx > 0) {
			tp = tp->prev;
		}
		else {
			send_message(np->sockfd, "Searched all twits!\n");
			break;
		}
		current_twit_idx--;
	}

	// after get all unread messages, set the last read twit idx to current twit tail's twit_idx
	np->last_read_twit_idx = twit_tail->twit_idx;
}

void get_unread_from(client_link np, char *subscription) {
	twit_link tp = twit_tail;
	int current_twit_idx = np->twit_idx_from_current_session - 1;
	bool first_twit = false;
	int first_twit_idx;

	while(np->last_read_twit_idx < current_twit_idx) {
		// check whether getter subscribed this twit's owner
		if(strcmp(subscription, user_arr[tp->user_idx]) == 0) {
			// found on the subilst. send it
			if(!first_twit) {
				first_twit = true;
				first_twit_idx = current_twit_idx;
			}
			send_a_twit(np, tp);
		}
		if(tp->twit_idx > 0) {
			tp = tp->prev;
		}
		else {
			send_message(np->sockfd, "Searched all twits!\n");
			break;
		}
		current_twit_idx--;
	}

	// after get all unread messages, set the last read twit idx to the latest twit read
	np->last_read_twit_idx = first_twit_idx;
}

int get_unread_num(client_link np) {
	twit_link tp = twit_tail;
	int current_twit_idx = tp->twit_idx;
	int j;
	int cnt = 0;

	while(np->last_read_twit_idx < current_twit_idx) {
		// check whether getter subscribed this twit's owner
		for(j=0;j<USERS - 1;j++) {
			if(np->sublist_arr[j] == tp->user_idx) {
				// found on the subilst. count it
				cnt++;
			}
			else if(np->sublist_arr[j] == -1) {
				// couldn't find this twit's owner in getter's sublist
				break;
			}
		}
		if(tp->twit_idx > 0) {
			tp = tp->prev;
		}
		else {
			break;
		}
		current_twit_idx--;
	}

	return cnt;
}

void client_thread(void *client_node_addr) {
	bool would_disconnect = false;
	bool signin_flag = true;
	char username[31];
	char password[31];
	int user_idx;
	char recv_buffer[MSG_LEN];
	char send_buffer[MSG_LEN];
	char hashtag_buffer[313] = {};
	client_link np = (client_link)client_node_addr;
	int rcv;
	int state = main_menu;
	int menu_selected;
	int hashtag_cnt;
	twit_link tp;

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
				user_idx = authenticate(username, password);
				if(user_idx != -1) {
					//user_idx--;
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
					sprintf(send_buffer, "Welcome, %s.\nYou have %d unread messages.\n", username, get_unread_num(np));
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
			printf("got %s from %d\n", recv_buffer, np->sockfd);
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
					edit_client_file(user_idx, np);
					send_message(np->sockfd, "Goodbye\n");
					break;

					default:
					send_message(np->sockfd, "Invalid input! Try again.\n");
					break;
				}
				break;

				case write_menu:
				if(strcmp(recv_buffer, "cancel") != 0) { // If client doesn't write 'cancel'
					if(strlen(recv_buffer) > 141) { // check twit's length
						send_message(np->sockfd, "Your twit is too long to post! Return to main menu..\n");
						state = main_menu;
					}
					else {
						tp = twit_str(user_idx, recv_buffer); // post it
						hashtag_cnt = 0;
						state = hashtag_menu;
					}
				}
				else {
					state = main_menu;
				}
				break;

				case read_menu:
				switch(menu_selected) {
					case 1:
					get_twits(np);
					state = main_menu;
					break;

					case 2:
					state = getoffline_super_menu;
					break;

					case 3:
					state = hashsearch_menu;
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
					printf("user_idx: %d\n", user_idx);
					add_subscription(user_idx, np);
					break;

					case 3:
					delete_subscription(user_idx, np);
					break;

					case -1:
					state = main_menu;
				}
				break;

				case hashtag_menu:
				hashtag_cnt++;
				recv_buffer[strlen(recv_buffer)] = '\0';
				if(strlen(hashtag_buffer)) {
 					strcat(hashtag_buffer, ";"); // add ';' as a delimeter
					strcat(hashtag_buffer, recv_buffer);
					printf("buffer is now '%s'\n", hashtag_buffer);
				}
				else {
					strcpy(hashtag_buffer, recv_buffer);
					printf("buffer is now '%s'\n", hashtag_buffer);
				}
				if((hashtag_cnt > 10) || (strcmp(recv_buffer, "!e") == 0)) {
					send_message(np->sockfd, "You posted a twit!\n");
					printf("Let's insert hashtags in '%s'\n", hashtag_buffer);
					insert_hashtag(tp, hashtag_buffer);
					state = main_menu;
				}
				else {
					state = hashtag_menu;
				}
				break;

				case hashsearch_menu:
				search_hashtag(np, recv_buffer);
				state = main_menu;
				break;

				case getoffline_super_menu:
				switch(menu_selected) {
					case 1:
					get_unread_all(np);
					state = main_menu;
					break;

					case 2:
					state = getoffline_part_menu;
					break;
				}
				break;

				case getoffline_part_menu:
				get_unread_from(np, recv_buffer);
				state = main_menu;
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
	printf("'%s' disconnected. Closing socket %d...\n", np->username, np->sockfd);
	close(np->sockfd);
	delete_node(client_head, np);
}

int main(void)
{
	int listenfd = 0,connfd = 0;
  	struct sockaddr_in serv_addr, client_addr;
  	int serv_addrlen, client_addrlen;
  	client_link new_client;
	twit_link new_twit, prev_twit;
	char tmp_buffer[313];
	char *token;
	int tmp_user_idx;
  	FILE *f;
	int i, j;

  	// initialize head node of client list
  	client_head = (client_link)malloc(sizeof(client_node));
  	client_head->next = NULL;

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
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(4111);

	//bind socket and listen
	bind(listenfd, (struct sockaddr*)&serv_addr,sizeof(serv_addr));
	if(listen(listenfd, 10) == -1) {
		printf("Failed to listen\n");
		return -1;
	}

	// initialize head node of twit list
	twit_head = (twit_link)malloc(sizeof(twit_node));
	twit_head->next = NULL;
	twit_tail = twit_head;

	//read twit list
	f = fopen("twitlist.txt", "r");
	if (f == NULL) {
		printf("Couldn't read twitlist.txt!\n");
		return 1;
	}

	i = 0;
	while(fgets(tmp_buffer, 313, f) != NULL) { // read twit list
		tmp_buffer[strlen(tmp_buffer) - 1] = '\0'; // change '\n' in the last column to '\0'
		new_twit = (twit_link)malloc(sizeof(twit_node)); // make a new twit node

		new_twit->twit_idx = i;

		tmp_user_idx = atoi(tmp_buffer); // get index of user
		new_twit->user_idx = tmp_user_idx;

		fgets(tmp_buffer, 313, f); // get twit string
		tmp_buffer[strlen(tmp_buffer) - 1] = '\0'; // change '\n' in the last column to '\0'
		strcpy(new_twit->twit_str, tmp_buffer);
		//printf("Read twit '%s'\n", tmp_buffer);

		fgets(tmp_buffer, 313, f); // get hashtags
		tmp_buffer[strlen(tmp_buffer) - 1] = '\0'; // change '\n' in the last column to '\0'
		token = strtok(tmp_buffer, ";"); // tokenize hashtags by ';'

		j = 0;
		while(token != NULL) {
			//printf("Read hashtag '%s'\n", token);
			new_twit->hashtag_list[j] = (char *)malloc(sizeof(char)*30);
			strcpy(new_twit->hashtag_list[j++], token); // copy into hashtag list
			token = strtok(NULL, ";");
		}

		new_twit->next = NULL;
		new_twit->prev = prev_twit;
		prev_twit = new_twit;
		twit_tail = new_twit;
		i++;
	}
	printf("Read %d twits\n", i);
	fclose(f);

	// init mutex lock
	printf("Initializing mutex...\n");
	if (pthread_mutex_init(&fwrite_mutex, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }


	printf("Ready to accept clients...\n");
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
		append_node(client_head, new_client);

		// make a new thread
		pthread_t id;
		if(pthread_create(&id, NULL, (void *)client_thread, (void *)new_client) != 0) {
			perror("Failed creating pthread.\n");
			return -1;
		}
	}

	pthread_mutex_destroy(&fwrite_mutex);

	return 0;
}
