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
	pthread_join(sndThread, &retFromThread);
	pthread_join(rcvThread, &retFromThread);
	ret = 0;

error:
	close(clntSock);
	endwin();
	return ret;
}

void *sndMsg(void *arg)
{
	int *ret = (int *)malloc(sizeof(int));
	*ret = -1;
	argForT_t *args = (argForT_t *)arg;
	int clntSock = args->clntSock;
	int sndLen;
	WINDOW *chatWin = args->cWin;
	char sndBuf[BUF_SIZE];
	memset(sndBuf, 0, BUF_SIZE);
	while(1)
	{
		getnstr(sndBuf, BUF_SIZE);
		sndLen = strlen(sndBuf);
		sndBuf[sndLen] = '\n';
		sndBuf[sndLen + 1] = '\0';

		clearWin();
		refresh();

		if(!strcmp(sndBuf, ":q\n") || !strcmp(sndBuf, "Q\n"))
		{
			*ret = 0;
			goto exitThread;
		}
		if(send(clntSock, sndBuf, BUF_SIZE, 0) == -1)
		{
			perror("send");
			*ret = -1;
			goto exitThread;
		}
	}
exitThread:
	close(clntSock);
	endwin();
	exit(*ret);
	return ret;
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
		if(rcvedLen == -1)
		{
			perror("recv2");
			*ret = -1;
			goto exitThread;
		}
		if(rcvedLen > 0 && strlen(rcvBuf) > 0)
		{
			rcvBuf[rcvedLen] = '\0';
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
