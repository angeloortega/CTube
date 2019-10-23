#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>

#define PORT 10521
#define BUFFER_SIZE 4096
#define CLIENT_AMOUNT 10

    
struct sockaddr_in si_me;

struct client{
	char ip[25];
	struct tm startTime;
	struct sockaddr_in si_other;
	unsigned char buf[BUFFER_SIZE];
	int slen;
	int threadNumber;
};

struct server{
	struct tm startTime;
	long transferredBytes;
	long requestCount;
	long threadCount;
};

int s, i , recv_len;
//unsigned char buf[BUFFER_SIZE];
pthread_t clients[CLIENT_AMOUNT];
int clientCount = 0;
int threadResult = 0;
struct server serverInfo;

void serverLog(char* type, char* message){
	time_t t = time(NULL);
 	struct tm startTime = *localtime(&t);
	char fileName[256];
	sprintf(fileName,"logs/%d-%d-%d.txt", startTime.tm_year + 1900, startTime.tm_mon + 1,startTime.tm_mday);
	FILE* f = fopen(fileName,"a");
	if(f == NULL){
		perror("Logging error: ");
	}
	printf("%d:%d:%d - %s: %s\n", startTime.tm_hour, startTime.tm_min, startTime.tm_sec, type, message);
	fprintf(f, "%d:%d:%d - %s: %s", startTime.tm_hour,startTime.tm_min, startTime.tm_sec, type, message);
	fclose(f);
}

// function to clear buffer 
void clearBuf(char* b){ 
    int i; 
    for (i = 0; i < BUFFER_SIZE; i++) 
        b[i] = '\0'; 
}
 
void *terminalHandler(void *arg){
	int flag = 1;
	char option;
	time_t t = time(NULL);
 	serverInfo.startTime = *localtime(&t);
	while(flag){
		printf("------------------options------------------\n");
		printf("1)------------------------Server Start Time\n");
		printf("2)--------------Amount of transferred bytes\n");
		printf("3)----------------------Unique client count\n");
		printf("4)---------------------------Request amount\n");
		printf("5)--------------------------Threads created\n");
		printf("6)-------------------------------------Exit\n");
		printf("-------------------------------------------\n");
		printf("Please select an option: ");
		option = getchar();
		printf("\n");
		switch (option)
		{
		case '1':
			printf("Server start time:%d-%d-%d %d:%d:%d\n", serverInfo.startTime.tm_year + 1900, serverInfo.startTime.tm_mon + 1,serverInfo.startTime.tm_mday, serverInfo.startTime.tm_hour, serverInfo.startTime.tm_min, serverInfo.startTime.tm_sec);
			break;
		case '2':
			printf("Amount of bytes transferred:%ld\n",serverInfo.transferredBytes);
			break;
		case '3':
			printf("Client count:%d\n", clientCount);
			break;
		case '4':
			printf("Request amount:%ld\n",serverInfo.requestCount);
			break;
		case '5':
			printf("Threads created:%ld\n",serverInfo.threadCount);
			break;	
		case '6':
			flag = 0;
			printf("Closing server...\n");
			serverLog("STATUS:","server shutting down");
			break;
		default:
			printf("Invalid option selected\n");
			break;
		}
	}
	exit(0);
}

void *clientHandler(void *arg){
	struct client info =  *(struct client *)arg;
	time_t t = time(NULL);
 	struct tm time = *localtime(&t);
	char message[256];
	sprintf(message,"Thread #%d created! Time: %d-%d-%d %d:%d:%d\n",info.threadNumber, time.tm_year + 1900, time.tm_mon + 1,time.tm_mday, time.tm_hour, time.tm_min, time.tm_sec);
	serverLog("THREAD",message);
	//Saving client information
	sprintf(info.ip, "%s:%d", inet_ntoa(info.si_other.sin_addr), ntohs(info.si_other.sin_port));
	info.startTime = time;

	//print details of the client/peer and the data received
	sprintf(message,"Received packet from %s\n", info.ip);
	serverLog("THREAD",message);
	FILE* f = fopen("resources/small.mp4","rb");
	if(f == NULL){
		serverLog("ERROR",strerror(errno));
		exit(1);
	}
	size_t bytesRead = 0;
		// read file in chunks
	while ((bytesRead = fread(info.buf, 1, sizeof(info.buf), f)) > 0)
	{
		sendto(s, info.buf, bytesRead, 0, (struct sockaddr*)&info.si_other, info.slen); 
		unsigned int size = bytesRead;
		//printf("%x\n", size);
		clearBuf(info.buf); 
	}
	pthread_exit(0);
}

int main(int argc, char* argv[]){
	char request[256];
	//create a UDP socket
	if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
	{
		serverLog("ERROR",strerror(errno));
	}
	
	// zero out the structure
	memset((char *) &si_me, 0, sizeof(si_me));
	
	si_me.sin_family = AF_INET;
	si_me.sin_port = htons(PORT);
	si_me.sin_addr.s_addr = htonl(INADDR_ANY);
	
	//bind socket to port
	if( bind(s , (struct sockaddr*)&si_me, sizeof(si_me) ) == -1)
	{
		serverLog("ERROR",strerror(errno));
	}
	threadResult = pthread_create(&(clients[clientCount]), NULL, terminalHandler,NULL);
	//keep listening for data
	while(1)
	{
		printf("Waiting for data...\n");
		//try to receive some data, this is a blocking call
		struct client info;
		info.slen = sizeof(info.si_other);
		memset((char *) &info.si_other, 0, sizeof(info.si_other));
		clearBuf(info.buf);
		if ((recv_len = recvfrom(s, info.buf, BUFFER_SIZE, 0, (struct sockaddr *) &info.si_other, &info.slen)) == -1)
		{
			serverLog("ERROR",strerror(errno));
		}
		sprintf(request,"Data: %s\n" , info.buf); //Video name or id
		serverLog("REQUEST",request);
		info.threadNumber = ++clientCount;
		threadResult = pthread_create(&(clients[clientCount]), NULL, clientHandler,(void*)&info);

		if(threadResult != 0){
			serverLog("ERROR",strerror(errno));
			exit(1);
		}
	}


	while(clientCount > 0){
		threadResult = pthread_join(clients[--clientCount],NULL);
		if(threadResult != 0){
			serverLog("ERROR",strerror(errno));("Thread join");
		}
	}
	return 0;
}