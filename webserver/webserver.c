#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>

#define BUF_SIZE 4096 
#define SMALL_BUF 255 

typedef enum
{
	ERR400,
	ERR404,
}errorType;

void *handleRequest(void *sock);
void sendData(int sock, char *contentType, char *fileName);
char *determineContentType(char *fileName);
void sendError(int sock, errorType errType);

int main(int argc, char *argv[]) 
{

	int servSock, acceptedSock;
	int port;
	int sockOptVal = 1;
	int ret = -1;
	char readBuf[BUF_SIZE];
	struct sockaddr_in servAddr, clntAddr;
	socklen_t addrSize = sizeof(clntAddr);
	pthread_t handleThread;

	if(argc != 2) 
	{
		printf("Please enter a port number.\n");
		exit(1);
	}
	for(int i = 0; i < strlen(argv[1]); i++)
	{
		if(!isdigit(argv[1][i]))
		{
			printf("Please enter a port number correctly.\n");
			exit(1);
		}
	}
	if(port = atoi(argv[1]) > 65535)
	{
		printf("Please enter a port number correctly.\n");
		exit(1);
	}

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

	setsockopt(servSock, SOL_SOCKET, SO_REUSEADDR, (void *)&sockOptVal, sizeof(sockOptVal));

	if(listen(servSock, 20))
	{
		perror("listen");
		goto error;
	}

	while(1)
	{
		acceptedSock = accept(servSock, (struct sockaddr *)&clntAddr, &addrSize);
		if(acceptedSock == -1)
		{
			perror("accept");
			goto error;
		}
		pthread_create(&handleThread, NULL, handleRequest, &acceptedSock);
		pthread_detach(handleThread);
	}
	ret = 0;
error:
	close(servSock);
	return ret;
}

void *handleRequest(void *sock)
{
	int clntSock = *((int *)sock);
	int recvLen;
	char reqBuf[BUF_SIZE];
	char method[SMALL_BUF];
	char contentType[SMALL_BUF];
	char fileName[SMALL_BUF];


	recvLen = recv(clntSock, reqBuf, BUF_SIZE, 0);
	if(recvLen == -1)
	{
		perror("recv");
		goto exitThread;
	}
	if(recvLen == 0)
	{
		printf("disconnected...\n");
		goto exitThread;
	}
	if(recvLen > 0)
	{
		if(strstr(reqBuf, "HTTP/") == NULL)
		{
			perror("request");
			goto exitThread;
		}

		strcpy(method, strtok(reqBuf, " "));
		strcpy(fileName, strtok(NULL, " "));

		if(strcmp(method, "GET") != 0)
		{
			sendError(clntSock, ERR400);
			goto exitThread;
		}

		if(!strcmp(fileName, "/"))
			strcpy(fileName, "index.html");
		else
		{
			for(int i = 0; i < strlen(fileName); i++)
			{
				fileName[i] = fileName[i + 1];
			}
		}

		strcpy(contentType, determineContentType(fileName));
		sendData(clntSock, contentType, fileName);
	}
exitThread:
	close(clntSock);
}

char *determineContentType(char *fileName)
{
	char file[SMALL_BUF];
	char extension[SMALL_BUF];
	strcpy(file, fileName);
	strtok(file, ".");
	strcpy(extension, strtok(NULL, "."));
	if(!strcmp(extension, "html")||!strcmp(extension, "htm"))
		return "text/html";
	/*it's runnig good, if the type for image doesn't be setted.*/
	if(!strcmp(extension, "png"))
		return "image/png";
	if(!strcmp(extension, "jpg"))
		return "image/jpeg";
	if(!strcmp(extension, "jpeg"))
		return "image/jpeg";
	return "text/plain";
}

void sendData(int sock, char *conType, char *fileName)
{
	char protocol[] = "HTTP/1.0 200 OK\r\n";
	char server[] = "Server: Linux Web Server\r\n";
	char contentLength[SMALL_BUF]; 
	char contentType[SMALL_BUF];
	char buf[BUF_SIZE];
	off_t contentLen;
	ssize_t readedSize;
	int sendFile;
	sendFile = open(fileName, O_RDONLY);

	if(sendFile == -1)
	{
		sendError(sock, ERR404);
		return;
	}

	contentLen = lseek(sendFile, 0, SEEK_END);
	lseek(sendFile, 0, SEEK_SET);

	if(sizeof(off_t) == sizeof(long int))
		sprintf(contentLength, "Content-length: %ld\r\n", contentLen);
	else
		sprintf(contentLength, "Content-length: %lld\r\n", contentLen);
	sprintf(contentType, "Content-type: %s\r\n\r\n", conType);

	if(send(sock, protocol, strlen(protocol), 0) == -1)
	{
		perror("send");
		goto error;
	}
	printf("%s", protocol);
	if(send(sock, server, strlen(server), 0) == -1)
	{
		perror("send");
		goto error;
	}
	printf("%s", server);
	if(send(sock, contentLength, strlen(contentLength), 0) == -1)
	{
		perror("send");
		goto error;
	}
	printf("%s", contentLength);
	if(send(sock, contentType, strlen(contentType), 0) == -1)
	{
		perror("send");
		goto error;
	}
	printf("%s", contentType);

	while((readedSize = read(sendFile, buf, BUF_SIZE)) != 0)
	{
		if(readedSize == -1)
		{
			perror("read");
			goto error;
		}
		if(send(sock, buf, readedSize, 0) == -1)
		{
			perror("send");
			goto error;
		}
	printf("%s", buf);
	}
error:
	close(sendFile);
	return;
}

void sendError(int sock, errorType errType)
{
	char protocol[SMALL_BUF];
	char server[SMALL_BUF];
	char contentLength[SMALL_BUF]; 
	char contentType[SMALL_BUF];
	char content[SMALL_BUF];
	int contentLen;

	if(errType == ERR400)
	{
		strcpy(protocol, "HTTP/1.0 400 Bad Request\r\n");
		strcpy(server,  "Server: Linux Web Server\r\n");
		strcpy(contentType, "Content-type: text/html\r\n\r\n");
		strcpy(content, "<html><head><title>400 Error</title></head><body>We don't service responses to methods other than 'GET'.</body></html>");
		contentLen = strlen(content) + 1;
		sprintf(contentLength, "Content-length: %d\r\n", contentLen);
		goto send;
	}

	if(errType == ERR404)
	{
		strcpy(protocol, "HTTP/1.0 404 Not Found\r\n");
		strcpy(server,  "Server: Linux Web Server\r\n");
		strcpy(contentType, "Content-type: text/html\r\n\r\n");
		strcpy(content, "<html><head><title>404: Not Found</title></head><body>404: Not Found.</body></html>");
		contentLen = strlen(content) + 1;
		sprintf(contentLength, "Content-length: %d\r\n", contentLen);
		goto send;
	}
send:
	if(send(sock, protocol, strlen(protocol), 0) == -1)
	{
		perror("send");
		goto error;
	}
	printf("%s", protocol);
	if(send(sock, server, strlen(server), 0) == -1)
	{
		perror("send");
		goto error;
	}
	printf("%s", server);
	if(send(sock, contentLength, strlen(contentLength), 0) == -1)
	{
		perror("send");
		goto error;
	}
	printf("%s", contentLength);
	if(send(sock, contentType, strlen(contentType), 0) == -1)
	{
		perror("send");
		goto error;
	}
	printf("%s", contentType);
	if(send(sock, content, strlen(content), 0) == -1)
	{
		perror("send");
		goto error;
	}
error:
	return;
}
