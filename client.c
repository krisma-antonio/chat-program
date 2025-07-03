/*
    Kevin Scrivnor
    Modified by: Krisma Antonio
    Copyright (C) Spring 2025
    COMP 429

    ncurses required
    -lncurses required to compile with gcc
    ubuntu: sudo apt install libncurses-dev
    https://invisible-island.net/ncurses/ncurses-intro.html

    gcc client.c -o client -Wall -lncurses

    testing server: nc -4kl localhost 9001
        (assumes you're using port 9001)
*/

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <ncurses.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_MSG 140
#define HIST_SIZE 256

void printUsage(char* prog);

void finish(int);
void clearText(WINDOW* win);
void refreshChat(WINDOW* chat);
void addMessage(char* msg);
void* handleMessage(void* p);

int joinServer(char* host);
ssize_t readLine(char* buffer, size_t n);
ssize_t sendLine(char* buffer, size_t n);

// our two windows, the chat messages and text entry box
WINDOW* chat;
WINDOW* text;

// message history
char** messages;
int pos = 0;

// socket file descriptor for server
int sfd;

// avoid writing/reading the socket at the same time
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_t tid;

int
main(int argc, char** argv)
{
    // connect to the server
    if(argc != 2) {
        printUsage(argv[0]);
        exit(EXIT_FAILURE);
    }
    sfd = joinServer(argv[1]);
    if(sfd == -1) {
        perror("Failed to connect to server");
        exit(EXIT_FAILURE);
    }

    // on ctrl-C, close it down
    signal(SIGINT, finish);

    // setup ncurses
    initscr();              // initialize the main screen
    keypad(stdscr, TRUE);   // watch the keyboard
    cbreak();               // input without enter
    echo();                 // display it to the screen
    refresh();

    // int nlines, int ncols, int begin_y, int begin_x);
    chat = newwin(LINES-3, COLS, 0, 0); // new window at 0,0
    text = newwin(3, COLS, LINES-3, 0);  // new window

    // create borders around the windows
    box(chat, 0, 0);
    box(text, 0, 0);

    // draw them
    wrefresh(chat);
    wrefresh(text);

    // setup message history
    // note: if the user resizes the window, this may cause problems
    messages = malloc(sizeof(char*) * HIST_SIZE);
    for(int i = 0; i < HIST_SIZE; i++) {
        messages[i] = calloc('\0', sizeof(char) * COLS-3);
    }
    // say welcome message
     addMessage("WELCOME to #nonsense\n");
    // addMessage(SERVER_JOINED);
    refreshChat(chat);

    // move cursor to text message section
    clearText(text);
    move(LINES-2, 1);

    // thread for handling user typed messages
    pthread_create(&tid, NULL, handleMessage, NULL);

    // handle messages
    for(;;) {
        char msg[MAX_MSG];
	     ssize_t amountReceived;
        if( (amountReceived = readLine(msg, MAX_MSG)) == -1) {
            break;
        }

        pthread_mutex_lock(&mtx);
		  // read what was sent from server
		  /*if(strncmp("5 ", msg, 2) == 0) {
		      addMessage("ERROR: Invalid command.\n");
				finish(0);
	     } else if(strncmp("6 ", msg, 2) == 0) {
				addMessage("ERROR: Invalid command.\n");
				finish(0);
		  } else {*/
				addMessage(msg);
       		refreshChat(chat);
		  //} 

        pthread_mutex_unlock(&mtx);
    }

    // addMessage(SERVER_QUIT);
    refreshChat(chat);

    finish(0);
}

void
printUsage(char* prog)
{
    fprintf(stderr, "IP Address of server required\n");
    fprintf(stderr, "%s <ip address>\n", prog);
}

/*
 * ncurses functions
 */

void
refreshChat(WINDOW* chat)
{
    clearText(chat);

    int totalLines = LINES-5;
    int start = pos - totalLines;
    if(pos - totalLines < 0) {
        totalLines = pos;
        start = 0;
    }
    for(int i = 0; i < totalLines; i++) {
        mvwprintw(chat, i+1, 1, "%s", messages[start++]);
    }
    wrefresh(chat);
}

void
addMessage(char* msg)
{
    if(pos == HIST_SIZE) {
        pos = 0;
    }
    strcpy(messages[pos++], msg);
}

void
clearText(WINDOW* win)
{
    wclear(win);
    box(win, 0, 0);
    wrefresh(win);
}

void
finish(int signo)
{
    endwin();
    close(sfd);

    exit(EXIT_SUCCESS);
}

void*
handleMessage(void* p)
{

    int ch;
    char msg[MAX_MSG];
    int len = 0;

    for (;;) {
        ch = getch();
        switch(ch) {
            case '\n':
                pthread_mutex_lock(&mtx);
                // mark end of message
                msg[len++] = '\n';
                msg[len++] = '\0';

                // add to chat and clear text entry box
                // you may want to remove addMessage() once your server is working

                addMessage(msg);
                refreshChat(chat);
                clearText(text);


					 // check if user enters EXIT command
 					 if (strncmp("EXIT", msg, 4) == 0) {
						finish(0);
					 }

					 // send over network
				    sendLine(msg, len);

                // reset length count and move cursor back
                len = 0;
                move(LINES-2, 1);

                pthread_mutex_unlock(&mtx);
                break;
            default:
                msg[len++] = ch;
        }
    }

}

/*
 * network functions
 */

int
joinServer(char* host)
{
    // TODO: connect to server _host_ on your protocol's chosen port #
    //       and return the socket file descriptor
	
	struct sockaddr_in sa = {
		.sin_family = AF_INET,
		.sin_port = htons(9001),
	};

	// Convert the string address to 
	inet_pton(AF_INET, host, &sa.sin_addr);

	int sfd = socket(AF_INET, SOCK_STREAM, 0);
	printf("Client socket created!\n");

	// Connect to that server
	if (connect(sfd, (struct sockaddr*) &sa, sizeof(sa)) < 0) {
        printf("Error in connection.\n");
        exit(1);
    }
 
    printf("Connected to Server.\n");

    return sfd; 
}

ssize_t
readLine(char* buf, size_t n)
{
    // TODO: see readLine() function from Kerrisk book (https://man7.org/tlpi/)
    //       it's available for free on O'Reilly Learning
    //       the global variable _sfd_ can be used for the socket file descriptor

	ssize_t numRead; 
	size_t totRead;
	char ch;

	if (n <= 0 || buf == NULL) {
 		errno = EINVAL;
 		return -1;
 	}

	totRead = 0;
	for (;;) {
		 numRead = read(sfd, &ch, 1);
		 if (numRead == -1) {
			  if (errno == EINTR)
					continue;
			  else
					return -1;
		 } else if (numRead == 0) { 
			  if (totRead == 0)
					return 0;
			  else
					break;
		 } else { 
			  if (totRead < n - 1) {
					totRead++;
					*buf++ = ch;
			  }
			  if (ch == '\n')
					break;
		 }
	}
	*buf = '\0';
	return totRead;

}

ssize_t
sendLine(char* buffer, size_t n)
{
    // TODO: probably the same book
	 ssize_t numWritten; 
	 size_t totWritten; 
	 for (totWritten = 0; totWritten < n; ) {
		  numWritten = write(sfd, buffer, n - totWritten);
		  if (numWritten <= 0) {
				if (numWritten == -1 && errno == EINTR)
					 continue; 
				else
					 return -1;
		  }
		  totWritten += numWritten;
		  buffer += numWritten;
	 }
	 return totWritten; 
}


