#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>

#define BUF_SIZE 255
#define EPOLL_SIZE 15

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		printf("Please enter a port number.\n");
		exit(1);
	}

	int servSock, acceptedSock, epollFd;
	int fcntlFlag;
	int eventCnt;
	int rcvedLen;
	int cntlFdList[15];
	int connectedCnt = 0;
	int ret = -1;
	struct sockaddr_in servAddr, clntAddr;
	char readBuf[BUF_SIZE];
	char sendBuf[BUF_SIZE + 10];
	char greetBuf[50];
	char clntNum[10];
	char *greeting = "Your id is: ";
	socklen_t addrSize = sizeof(clntAddr);
	struct epoll_event *eventList;
	struct epoll_event eventInfo;
	FILE *writeFp;

	if((servSock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("socket");
		goto error;
	}

	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = INADDR_ANY;
	servAddr.sin_port = htons(atoi(argv[1]));
	memset(&servAddr.sin_zero, 0, sizeof(servAddr.sin_zero));

	if(bind(servSock, (struct sockaddr *)&servAddr, sizeof(servAddr)))
	{
		perror("bind");
		goto error; 
	}

	if(listen(servSock, 5))
	{
		perror("listen");
		goto error;
	}

	epollFd = epoll_create(EPOLL_SIZE);
	eventList = malloc(sizeof(struct epoll_event) * EPOLL_SIZE);
	eventInfo.events = EPOLLIN;
	eventInfo.data.fd = servSock;
	epoll_ctl(epollFd, EPOLL_CTL_ADD, servSock, &eventInfo);

	while(1)
	{
		eventCnt = epoll_wait(epollFd, eventList, EPOLL_SIZE, -1);
		if(eventCnt == -1)
		{
			perror("wait");
			goto error;
		}

		for(int i = 0; i < eventCnt; i++)
		{
			if(eventList[i].data.fd == servSock)
			{
				acceptedSock = accept(servSock, (struct sockaddr *)&clntAddr, &addrSize);
				if(acceptedSock == -1)
				{
					perror("accept");
					goto error;
				}
				fcntlFlag = fcntl(acceptedSock, F_GETFL, 0);
				fcntl(acceptedSock, F_SETFL, fcntlFlag | O_NONBLOCK);
				eventInfo.events = EPOLLIN | EPOLLET;
				eventInfo.data.fd = acceptedSock;
				epoll_ctl(epollFd, EPOLL_CTL_ADD, acceptedSock, &eventInfo);
				sprintf(clntNum, "%d\n", acceptedSock);
				strcpy(greetBuf, greeting);
				strcat(greetBuf, clntNum);
				if(send(acceptedSock, greetBuf, strlen(greetBuf) + 1, 0) == -1)
				{
					perror("send");
					goto error;
				}
				cntlFdList[connectedCnt] = acceptedSock;
				connectedCnt++;
				printf("%d host(s) on connections.\n%d has connected.\n", connectedCnt, acceptedSock);
				continue;
			}
			while(1)
			{
				rcvedLen = recv(eventList[i].data.fd, readBuf, BUF_SIZE - 1, 0);
				if(rcvedLen == 0)
				{
					eventInfo.events = EPOLLIN | EPOLLET;
					eventInfo.data.fd = eventList[i].data.fd;
					epoll_ctl(epollFd, EPOLL_CTL_DEL, eventList[i].data.fd, &eventInfo);
					close(eventList[i].data.fd);
					for(int j = 0; j < connectedCnt; j++)
					{
						if(cntlFdList[j] == eventList[i].data.fd)
						{
							cntlFdList[j] = cntlFdList[connectedCnt - 1];
						}
					}
					connectedCnt--;
					printf("%d host(s) on connections.\n%d has disconnected.\n", connectedCnt, eventList[i].data.fd);
					break;
				}
				if(rcvedLen < 0)
				{
					if(errno != EAGAIN)
					{
						perror("recv");
					}
					break;
				}
				for(int j = 0; j < connectedCnt; j++)
				{
					readBuf[rcvedLen] = '\0';
					memset(sendBuf, 0, BUF_SIZE + 10);
					sprintf(clntNum, "%d said>> ", eventList[i].data.fd);
					strcpy(sendBuf, clntNum);
					strcat(sendBuf, readBuf);
					if(send(cntlFdList[j], sendBuf, BUF_SIZE + 10, 0) == -1)
					{
						perror("send");
						goto error;
					}
				}
			}
		}
	}
	ret = 0;
error:
	close(servSock);
	close(epollFd);
	printf("%d\n", ret);
	return ret;
}
