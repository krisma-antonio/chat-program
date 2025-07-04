/*
 * Kevin Scrivnor
 * Modified by: Krisma Antonio
 * Copyright (C) 2024
 * COMP 429 - Computer Networking
 *
 * A concurrent echo server using threading
 *
 *	gcc -Wall -g server.c -o server
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>

#define PORT 9001
#define BACKLOG 10
#define BUF_SIZE 4096
#define MAX_CLIENTS 10

int sockfd;
int clients;
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
void quit(int);
void sub_client(int);
void* handle_request(void*);
void add_client(int clifd);

enum Errors {
    NO_ERROR, NO_NEW_LINE, TOO_SHORT, TOO_LONG, INVALID, OUT_OF_ORDER, CMD_INVALID
};

int test_name(char*, char**);

// client socket
typedef struct clientSocket
{
	pthread_t id;
	int clientSocketFD;
	struct sockaddr_in* address;
	bool adminAccess;	
	char name[20];
} CLIENT_SOCKET;

// to store all connected client sockets
CLIENT_SOCKET* connectedSockets[MAX_CLIENTS];
int connectedSocketsCount = 0;

// MODERATOR Password
char storedPassword[13] = "iAmModerator";

// array of banned user names
char* bannedUsers[MAX_CLIENTS];
int bannedCount = 0;

int
main(int argc, char** argv)
{
   // on ctrl-c, quit
   signal(SIGINT, quit);
	
   clients = 0;

   sockfd = socket(AF_INET, SOCK_STREAM, 0);
   if(sockfd < 0) {
      perror("Problem with socket()");
      exit(EXIT_FAILURE);
   }

   struct sockaddr_in sa = {
      .sin_family = AF_INET,
      .sin_port = htons(PORT),
      .sin_addr = htonl(INADDR_ANY)
   };

   if(bind(sockfd, (struct sockaddr*) &sa, sizeof(sa)) < 0) {
      perror("Problem with bind()");
      exit(EXIT_FAILURE);
   }

   if(listen(sockfd, BACKLOG) < 0) {
      perror("Problem with listen()");
      exit(EXIT_FAILURE);
   }

   int port = ntohs(sa.sin_port);
   char ip[INET_ADDRSTRLEN];
   if(inet_ntop(AF_INET, (struct sockaddr*) &sa.sin_addr, ip, INET_ADDRSTRLEN) == NULL) {
      fprintf(stderr, "Couldn't convert server address\n");
   } else {
      printf("Listening on %s:%d\n", ip, port);
   }

   for(;;) {

       struct sockaddr_in* clia = malloc(sizeof(struct sockaddr_in));
       socklen_t addrlen = sizeof(struct sockaddr_in);
       int clifd = accept(sockfd, (struct sockaddr*) clia, &addrlen);
       if(clifd < 0) {
	  perror("Problem with accept()");
	  continue;
       }

	// create client thread
	if(clients+1 <= MAX_CLIENTS) {
	
		CLIENT_SOCKET* clientSocket = malloc(sizeof(CLIENT_SOCKET));
		clientSocket->clientSocketFD = clifd;
		clientSocket->address = clia;
	
		// store connected clients
		connectedSockets[connectedSocketsCount++] = clientSocket;
		add_client(clifd);
		printf("Clients connected: %d\n", clients);
	
		pthread_create(&(clientSocket)->id, NULL, handle_request, (void*)clientSocket);
	
	} else {
		printf("Too many clients connected. Closing...\n");
		close(clifd);
	}
   }
}

void
quit(int signo)
{
   printf("Shutting down server...\n");
   // if ctrl-C is given
   close(sockfd);
   exit(EXIT_SUCCESS);
}

void
sub_client(int clifd)
{
   pthread_mutex_lock(&mtx);
   printf("Client process has finished.\n\n");
   clients--;
   pthread_mutex_unlock(&mtx);
}

void
add_client(int clifd)
{
   pthread_mutex_lock(&mtx);
   clients++;
   pthread_mutex_unlock(&mtx);
}

void*
handle_request(void* p)
{
   CLIENT_SOCKET* client = (CLIENT_SOCKET*) p;

   int port = ntohs(client->address->sin_port);
   char ip[INET_ADDRSTRLEN];
   if(inet_ntop(AF_INET, &(client->address->sin_addr), ip, INET_ADDRSTRLEN) == NULL) {
      fprintf(stderr, "Couldn't convert client address\n");
   } else {
      printf("Received connection from %s on port %d\n", ip, port);
   }

   char* buf = (char*)malloc(sizeof(char) * BUF_SIZE);
   char* msg = (char*)malloc(sizeof(char) * BUF_SIZE);
   ssize_t numRead;
   bool registration = true;
   int errorNum;
   char* username = (char*)malloc(sizeof(char)* (strlen(buf)+1));

   // READ socket
   while((numRead = read(client->clientSocketFD, buf, BUF_SIZE)) > 0) {

	// connection first, check registration packet
	if(registration) {
	
		// if MODERATOR
		if(strncmp(buf, "MODERATOR ", 10) == 0) {
			char* password = &buf[10];
			
			if(!strncmp(password, storedPassword, 12) == 0) {
				printf("Wrong moderator password\n");
			}
			
			printf("Moderator entered the chatroom!\n");
			sprintf(msg, "Moderator entered the chatroom!\n");
	
			client->adminAccess = true;
			registration = false;
		}

		// if REGULAR USER
		else if(strncmp(buf, "NAME ", 5) == 0) {
			client->adminAccess = false;
	
			char* data = (char*)malloc(sizeof(char)* (strlen(buf)+1));
	
			 strcpy(data, buf);
		
			 // PARSE the name
			 errorNum = test_name(data, &buf);
		
			 // WRITE back name
			 if(errorNum == 0) {
				  strcpy(username, buf);
				  username[strcspn(username, "\n")] = 0;
	
				  // check if username is banned
				  for(int i = 0; i < bannedCount; i++) {
					 if(strcmp(bannedUsers[i], username) == 0) {
						 printf("Username %s is banned! Cannot connect\n", username);
						 sub_client(client->clientSocketFD);
						 close(client->clientSocketFD);
						 free(buf);
						 return NULL;
					 }
				  }

				  // check if username is already taken
				  for(int i = 0; i < connectedSocketsCount; i++) {
				     	if(strcmp(connectedSockets[i]->name, username) == 0) {
						 printf("Username %s is already taken! Cannot connect\n", username);
						 sub_client(client->clientSocketFD);
						 close(client->clientSocketFD);
						 free(buf);
						 return NULL;
					}
				  }
				  sprintf(msg, "%s entered the chatroom!\n", username);

				  printf("%s entered the chatroom!\n", username);
	
				  strcpy(client->name, username);
	
				  registration = false;
			 } else {
				  //sprintf(msg, "%d ", errorNum);
				  printf("ERROR: Invalid command!\n");
			 }

		 	free(data);
         	} else {
	      		fprintf(stderr, "Error, expecting NAME or MODERATOR packet first\n");
		}

	// registration is false
	} else {

		if(!client->adminAccess) {
			// REGULAR USER actions
			 if(strncmp(buf, "MSG ", 4) == 0) {
				  // parse message and print to server
				  char* message = &buf[4];
				  // message[strcspn(message, "\n")] = 0;

				  printf("%s: %s\n", username, message);
				  sprintf(msg, "%s: %s\n", username, message);

			 } else {
				  // sprintf(msg, "%d ", CMD_INVALID);
				  printf("ERROR: Invalid command!\n");
			 }
		 } else {
			 // MODERATOR actions
			 if(strncmp(buf, "KICK ", 5) == 0) {
				 char* kickName = &buf[5];
				 kickName[strcspn(kickName, "\n")] = 0;

				 for(int i = 0; i < connectedSocketsCount; i++) {
					 // printf("Names: %s\n", connectedSockets[i]->name);

					 if(strcmp(connectedSockets[i]->name, kickName)==0) {
						 //sub_client(connectedSockets[i]->clientSocketFD); 
						 close(connectedSockets[i]->clientSocketFD);
					    	 printf("%s is kicked out of the chatroom!\n", kickName);
						 break;
					 } else {
						 printf("%s is not found.\n", kickName);
					 }
				  }

			 } else if(strncmp(buf, "BAN ", 4) == 0)  {
				 char* banName = &buf[4];
				 banName[strcspn(banName, "\n")] = 0;

				 // Add to ban list
				 bannedUsers[bannedCount++] = strdup(banName);
				 printf("Username %s has been banned by the moderator\n", banName);

			 } else if(strncmp(buf, "TOPIC ", 6) == 0)  {
				 char* topic = &buf[6];
				 topic[strcspn(topic, "\n")] = 0;
				 sprintf(msg, "A topic '%s' is set by the moderator!\n", topic);
				 printf("A topic '%s' is set by the moderator!\n", topic);
			 } else {
			    printf("Invalid moderator command!\n");

			 }
		 }
	}

	// WRITE back messages to other clients
	ssize_t msgLen = strlen(msg);

	for(int i = 0; i < connectedSocketsCount; i++) {
		if(connectedSockets[i]->clientSocketFD != client->clientSocketFD) {
			if(write(connectedSockets[i]->clientSocketFD, msg, msgLen) != msgLen ) {
				fprintf(stderr, "Couldn't perform write() to other clients\n");
				break;
			}
		}
	}

   }

   /*// let user know who left the chatroom
   sprintf(msg, "%s has left the chatroom.\n", client->name);

   ssize_t msgLen = strlen(msg);
   for (int i = 0; i < connectedSocketsCount; i++) {
   if (connectedSockets[i] && connectedSockets[i]->clientSocketFD != client->clientSocketFD) {
   	write(connectedSockets[i]->clientSocketFD, msg, msgLen);
   }
   }*/

   // CLEAN UP
   sub_client(client->clientSocketFD);
   close(client->clientSocketFD);
   free(username);
   free(buf);
   free(msg);
   return NULL;
}

int
test_name(char* userData, char** buffer)
{

   if(strncmp(userData, "NAME ", 5) != 0) {
      fprintf(stderr, "Error, expecting name packet first\n");
      return OUT_OF_ORDER;
   } else {
        char* name = &userData[5];

	int len = strlen(name);
	*buffer = (char*)malloc(sizeof(char) * (len+1));

		/*
		name[strcspn(name, "\n")] = 0;
      if(strstr(name, "\\n") == NULL) {
          fprintf(stderr, "ERROR: Invalid command\n");
          return NO_NEW_LINE;
      }

		// not counting the '\\n' characters
      if(len-3 < 3) {
          fprintf(stderr, "Error, name %s not long enough\n", name);
          return TOO_SHORT;
      }
      if (len-3 > 20) {
          fprintf(stderr, "Error, name %s is too long\n", name);
          return TOO_LONG;
      }

		
      for(int i = 0; i < len-3; i++) {
          char c = name[i];
			 if( (c >= 'a' && c <= 'z')  || (c >= 'A' && c <= 'Z') || (c == '_') || (c == '.')) {
             continue;
          } else {
             fprintf(stderr, "Name %s contains invalid characters...\n", name);
             return INVALID;
          }
      }*/

      strcpy(*buffer,name);
      return NO_ERROR;
   }

}


