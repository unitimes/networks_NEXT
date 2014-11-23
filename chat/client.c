#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <locale.h>
#include <ctype.h>

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
	int clntSock;
	int port;
	struct sockaddr_in servAddr;
	struct sockaddr_in verify;
	pthread_t sndThread, rcvThread;
	int ret = -1;
	argForT_t *args = (argForT_t *)malloc(sizeof(argForT_t));

	if(argc != 3)
	{
		printf("Please enter IP & port.\n");
		exit(1);
	}
	if(inet_pton(AF_INET, argv[1], &(verify.sin_addr)) == 0)
	{
		printf("Please enter correct IP addr.\n");
		exit(1);
	}
	for(int i = 0; i < strlen(argv[2]); i++)
	{
		if(!isdigit(argv[2][i]))
		{
			printf("Please enter correct Port number.\n");
			exit(1);
		}
	}
	if(port = atoi(argv[2]) > 65535)
	{
		printf("Please enter correct Port number.\n");
		exit(1);
	}

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
	servAddr.sin_port = htons(atoi(argv[2]));
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
	pthread_join(sndThread, NULL);
	pthread_join(rcvThread, NULL);
	printf("thread exit\n");
	ret = 0;

error:
	close(clntSock);
	endwin();
	return ret;
}

void *sndMsg(void *arg)
{
	argForT_t *args = (argForT_t *)arg;
	int sndLen;
	int clntSock = args->clntSock;
	WINDOW *chatWin = args->cWin;
	char sndBuf[BUF_SIZE];
	while(1)
	{
		getnstr(sndBuf, BUF_SIZE - 2);
		sndLen = strlen(sndBuf);
		sndBuf[sndLen] = '\n';
		sndBuf[sndLen + 1] = '\0';

		clearWin();
		refresh();

		if(!strcmp(sndBuf, ":q\n") || !strcmp(sndBuf, ":Q\n"))
		{
			goto exitThread;
		}
		if(send(clntSock, sndBuf, strlen(sndBuf) + 1, 0) == -1)
		{
			perror("send");
			goto exitThread;
		}
	}
exitThread:
	endwin();
	/*by using 'exit()' terminate all threads*/
	exit(0);
}

void *rcvMsg(void *arg)
{
	argForT_t *args = (argForT_t *)arg;
	int rcvedLen;
	int row = 1;
	int lines = 1;
	int clntSock = args->clntSock;
	WINDOW *chatWin = args->cWin;
	char rcvBuf[BUF_SIZE];
	char backBuf[BUF_SIZE];
	mvwprintw(chatWin, 2, 0, "");
	while(1)
	{
		rcvedLen = recv(clntSock, rcvBuf, BUF_SIZE - 1, 0);
		if(rcvedLen == -1)
		{
			perror("recv");
			goto exitThread;
		}
		if(rcvedLen == 0)
		{
			wprintw(chatWin, "Disconnected by the server.\n");
			wrefresh(chatWin);
			refresh();
			goto exitThread;
		}
			
		if(rcvedLen > 0 && strlen(rcvBuf) > 0)
		{
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
	endwin();
	/*by using 'exit()' terminate all threads*/
	exit(0);
}
