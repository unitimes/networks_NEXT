#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <termios.h>
#include <fcntl.h>

#define BUF_SIZE 255
#define CLEAR_SCREEN_ANSI "\e[1;1H\e[2J"
void *sndMsg(void *arg);
void *rcvMsg(void *arg);

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
	/*clear terminal*/
	write(STDOUT_FILENO, CLEAR_SCREEN_ANSI, 12);

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

	printf("Connection successed.\n");
	pthread_create(&sndThread, NULL, sndMsg, (void *)&clntSock);
	pthread_create(&rcvThread, NULL, rcvMsg, (void *)&clntSock);
	pthread_join(sndThread, &retFromThread);
	pthread_join(rcvThread, &retFromThread);
	ret = 0;

error:
	close(clntSock);
	return ret;
}

void *sndMsg(void *arg)
{
	int *ret = (int *)malloc(sizeof(int));
	*ret = -1;
	int clntSock = *((int *)arg);
	char sndBuf[BUF_SIZE];

	/*get inputs by char in terminal*/
	/*struct termios oldt, newt;*/
	/*int ch;*/
	/*int oldf;*/
	/*tcgetattr(stdin, &oldt);*/
	/*newt = oldt;*/
	/*newt.c_lflag &= ~(ICANON | ECHO);*/
	/*oldf = fcntl(stdin, F_GETRL, 0);*/

	while(1)
	{
		/*tcsetattr(stdin, TCSANOW, &newt);*/
		/*fcntl(stdin, F_SETFL, oldf | O_NONBLOCK);*/
		
		printf("%c[%d;%df", 0x1B, 20, 1);

		fgets(sndBuf, BUF_SIZE, stdin);
		if(!strcmp(sndBuf, ":q\n") || !strcmp(sndBuf, "Q\n"))
		{
			close(clntSock);
			exit(0);
		}
		if(send(clntSock, sndBuf, BUF_SIZE, 0) == -1)
		{
			perror("recv");
			*ret = -1;
			goto exitThread;
		}
	}
exitThread:
	close(clntSock);
	return ret;
}

void *rcvMsg(void *arg)
{
	int *ret = (int *)malloc(sizeof(int));
	*ret = -1;
	int clntSock = *((int *)arg);
	int rcvedLen;
	int row = 1;
	char rcvBuf[BUF_SIZE];
	while(1)
	{
		rcvedLen = recv(clntSock, rcvBuf, BUF_SIZE - 1, 0);
		if(rcvedLen == -1)
		{
			perror("recv");
			*ret = -1;
			goto exitThread;
		}
		if(rcvedLen > 0)
		{
			printf("%c[%d;%df", 0x1B, row++, 1);
			fputs(rcvBuf, stdout);
			if(row >= 20)
			{
				row = 1;
				write(STDOUT_FILENO, CLEAR_SCREEN_ANSI, 12);
			}
		}
	}
exitThread:
	return ret;
}
