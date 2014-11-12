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
// minsuk: it should be 16 at minimum (server sock + 15 client socket)

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		printf("Please enter a port number.\n");
		exit(1);
	}
	// minsuk: put this argument check below the declaration part

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
	// minsuk: redefine as    #define SND_BUF_SIZE (BUF_SIZE+15)	
	char greetBuf[50];
	char clntNum[10];
	// minsuk: 10 is not enough see server loop below
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
	// minsuk: what happens if argv[1] is not numeric?
	memset(&servAddr.sin_zero, 0, sizeof(servAddr.sin_zero));

	if(bind(servSock, (struct sockaddr *)&servAddr, sizeof(servAddr)))
	{
		perror("bind");
		goto error; 
	}
	// minsuk: I suggest you use setsockopt() to avoid 'address in use' error
	
	if(listen(servSock, 5))
	{
		perror("listen");
		goto error;
	}

	/*fcntlFlag = fcntl(servSock, F_GETFL, 0);*/
	/*fcntl(servSock, F_SETFL, fcntlFlag | O_NONBLOCK);*/
	// minsuk: it's better to make this socket, NON_blocking mode too.

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
				// minsuk: you should insert some code to avoide accepting 
				//         more than 15 connections, as cntlFdList[]
				fcntlFlag = fcntl(acceptedSock, F_GETFL, 0);
				fcntl(acceptedSock, F_SETFL, fcntlFlag | O_NONBLOCK);
				eventInfo.events = EPOLLIN | EPOLLET;
				// minsuk: I recomment not using EPOLLET for chatting.
				eventInfo.data.fd = acceptedSock;
				epoll_ctl(epollFd, EPOLL_CTL_ADD, acceptedSock, &eventInfo);
				sprintf(clntNum, "%d\n", acceptedSock);
				strcpy(greetBuf, greeting);
				strcat(greetBuf, clntNum);
				// minsuk: sprintf(greetBuf, "Your ID is %d\m", acceptedSock);
				//          (replacing 3 lines above)
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
				rcvedLen = recv(eventList[i].data.fd, readBuf, BUF_SIZE, 0);
				if(rcvedLen == 0)
				{
					eventInfo.events = EPOLLIN | EPOLLET;
					// minsuk: you dont need this for EPOLL_CTL_DEL
					eventInfo.data.fd = eventList[i].data.fd;
					epoll_ctl(epollFd, EPOLL_CTL_DEL, eventList[i].data.fd, &eventInfo);
					close(eventList[i].data.fd);
					for(int j = 0; j < connectedCnt; j++)
					{
						if(cntlFdList[j] == eventList[i].data.fd)
						{
							cntlFdList[j] = cntlFdList[connectedCnt - 1];
						}
						// minsuk: why dont you put 'break;' here?
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
					memset(sendBuf, 0, BUF_SIZE + 10);
					sprintf(clntNum, "%d said>> ", eventList[i].data.fd);
					// minsuk: if fd value is higher than to equal to 10, 
					//        clntNum[10] is not enough. (%d is not a signle digit)
					//        you declared client fd list as [15] !!!
					strcpy(sendBuf, clntNum);
					strcat(sendBuf, readBuf);
					// minsuk: this strcat will make buffer overflow (due to BUF_SIZE)
					//         may incur serious security breach.
					if(send(cntlFdList[j], sendBuf, BUF_SIZE + 10, 0) == -1)
					// minsuk: message size is strlen(sendBuf) + 1
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
	// minsuk: if you use peeror() correctly you dont need this
	return ret;
}
