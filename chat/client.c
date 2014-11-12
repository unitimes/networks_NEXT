#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <locale.h>

#define BUF_SIZE 255
#define CLEAR_SCREEN_ANSI "\e[1;1H\e[2J"
void *sndMsg(void *arg);
void *rcvMsg(void *arg);
typedef struct argForThread
{
	WINDOW *cWin;
	int clntSock;
}argForT_t;

void clearWin()
{
	int i = LINES - 5;
	for(; i <= LINES; i++)
	{   
		mvprintw(i, 0, "\n");
	}   
	mvprintw(LINES - 5, 0, "\n");
}

void clearChatWin(WINDOW *chatWin)
{
	int i = 1;
	for(; i <= LINES - 7; i++)
	{   
		mvwprintw(chatWin, i, 0, "\n");
	}   
	mvwprintw(chatWin, 1, 0, "");
	wborder(chatWin, ' ', ' ', '-', '-', '-', '-', '-', '-');
}

int main(int argc, char *argv[])
{
	if(argc != 3)
	{
		printf("Please enter IP & port.\n");
		exit(1);
	}
	int clntSock;
	struct sockaddr_in servAddr;
	pthread_t sndThread, rcvThread;
	void *retFromThread;
	int ret = -1;
	argForT_t *args = (argForT_t *)malloc(sizeof(argForT_t));

	setlocale(LC_ALL, "ko_KR.utf8");
	setlocale(LC_CTYPE, "ko_KR.utf8");

	WINDOW *chatWin;
	initscr();
	refresh();

	chatWin = newwin(LINES - 7, COLS - 1, 0, 0); 
	wborder(chatWin, ' ', ' ', '-', '-', '-', '-', '-', '-');
	wrefresh(chatWin);

	mvprintw(LINES - 6, 0, "Write here:(type ':q' or ':Q' to quit)\n\n");
	refresh();

	if((clntSock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("socket");
		goto error;
	}

	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = inet_addr(argv[1]);
	//minsuk:  if argv[1] is standard IP address format?
	servAddr.sin_port = htons(atoi(argv[2]));
	//minsuk:  if argv[2] is non-numeric string?
	memset(&servAddr.sin_zero, 0, sizeof(servAddr.sin_zero));

	if(connect(clntSock, (struct sockaddr *)&servAddr, sizeof(servAddr)) == -1)
	{
		perror("connect");
		goto error;
	}

	mvwprintw(chatWin, 1, 0,"Connection successed.\n");
	args->cWin = chatWin;
	args->clntSock = clntSock;
	pthread_create(&sndThread, NULL, sndMsg, (void *)args);
	pthread_create(&rcvThread, NULL, rcvMsg, (void *)args);
	pthread_join(sndThread, &retFromThread);
	pthread_join(rcvThread, &retFromThread);
	// minsuk: if you dont use *retFromThread, just use NULL
	ret = 0;

error:
	close(clntSock);
	endwin();
	return ret;
}

// minsuk: All the thread related review comments also applies to rcvMsg() thread too. :-)
void *sndMsg(void *arg)
{
	int *ret = (int *)malloc(sizeof(int));
	*ret = -1;
	// minsuk: why don't you use global variable to return somthing?
	//	   malloc() and no free() is not a good idea though.
	argForT_t *args = (argForT_t *)arg;
	int clntSock = args->clntSock;
	int sndLen;
	WINDOW *chatWin = args->cWin;
	// minsuk: exhange line 112 and line 113 would improve the readability
	char sndBuf[BUF_SIZE];
	memset(sndBuf, 0, BUF_SIZE);
	// minsuk: memset() is not needed here. if it is needed, wit should be in while loop
	while(1)
	{
		getnstr(sndBuf, BUF_SIZE);
		// minsuk: it shoube be BUF_SIZE - 1 or BUF_SIZE - 2  for '\n', and '\0'
		sndLen = strlen(sndBuf);
		sndBuf[sndLen] = '\n';
		sndBuf[sndLen + 1] = '\0';
		// minsuk: this \0 insering is the readone whil memset() is not needed.

		clearWin();
		refresh();

		if(!strcmp(sndBuf, ":q\n") || !strcmp(sndBuf, "Q\n"))
		// minsuk: ":Q\n", not "Q\n" ? 
		{
			*ret = 0;
			goto exitThread;
		}
		if(send(clntSock, sndBuf, BUF_SIZE, 0) == -1)
		// minsuk: the message size to send is strlen(andBuf)+1, not BUF_SIZE
		{
			perror("send");
			*ret = -1;
			goto exitThread;
		}
	}
exitThread:
	close(clntSock);
	//minsuk:  clntSock is the same socket shared by sndMsg(), and rcvMsg() thread
	//         if one thread closed the socket first, what happen at the close() call in the other thread?
	endwin();
	// minsuk: I'm not quite sure but the same goes for the chatwin by calling endwin() in both threads
	exit(*ret);
	// minsuk: this is not correct, you have to use ptherad_exit()
	//          please read carefully pthread_exit();
	return ret;
	// minsuk: you dont need this
}

void *rcvMsg(void *arg)
{
	int *ret = (int *)malloc(sizeof(int));
	*ret = -1;
	argForT_t *args = (argForT_t *)arg;
	int clntSock = args->clntSock;
	WINDOW *chatWin = args->cWin;
	int rcvedLen;
	int row = 1;
	int lines = 1;
	char rcvBuf[BUF_SIZE];
	char backBuf[BUF_SIZE];
	mvwprintw(chatWin, 2, 0, "");
	while(1)
	{
		rcvedLen = recv(clntSock, rcvBuf, BUF_SIZE - 1, 0);
		// minsuk: you should process the case of rcvedLen == 0 (connection closed by peer)
		if(rcvedLen == -1)
		{
			perror("recv2");
			*ret = -1;
			goto exitThread;
		}
		if(rcvedLen > 0 && strlen(rcvBuf) > 0)
		{
			rcvBuf[rcvedLen] = '\0';
			// minsuk: we dont need this NULL attaching, send already did it.
			if(lines > LINES - 10)
			{
				lines = 1;
				clearChatWin(chatWin);
			}
			wprintw(chatWin, rcvBuf);
			wrefresh(chatWin);
			lines += strlen(rcvBuf) / COLS + 1;
			printw("");
			refresh();
		}
	}
exitThread:
	close(clntSock);
	endwin();
	exit(*ret);
	return ret;
}
