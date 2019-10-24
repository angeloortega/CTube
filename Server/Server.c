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
#include <signal.h>
#include <sys/sem.h>

#define PORT 10521
#define BUFFER_SIZE 4096
#define CLIENT_AMOUNT 25

typedef enum {CLIENTS=0,IPS=1,LOG=2,REQUESTS=3,TRANSFERRED=4} resource;

struct client{
	char ip[25];
	struct tm startTime;
	struct sockaddr_in si_other;
	unsigned char buf[BUFFER_SIZE];
	int slen;
	int threadNumber;
	int fd;
	int occupied;
	pthread_t thread;
};

struct server{
	struct tm startTime;
	long transferredBytes;
	long requestCount;
	long threadCount;
	int clientCount;
};

int s;
static int threadResult = 0;
static struct server serverInfo;
static struct client clients[CLIENT_AMOUNT];
static char clientIps[25][CLIENT_AMOUNT*4];
static struct sockaddr_in si_me;
static int semaphores[5];

static int semaphore_v(resource res){
	struct sembuf sem_b;
	sem_b.sem_num = 0; 
	sem_b.sem_op = 1; 
	sem_b.sem_flg = SEM_UNDO;
	int item = (int) res;
	if (semop(item, &sem_b, 1) == -1) { 
		fprintf(stderr, "semaphore_v failed\n"); 
		return(0); 
	} 
	return(1);
}

static int semaphore_p(resource res){
	struct sembuf sem_b;
	sem_b.sem_num = 0; 
	sem_b.sem_op = -1;
	sem_b.sem_flg = SEM_UNDO; 
	int item = (int) res;
	if (semop(item, &sem_b, 1) == -1) { 
		fprintf(stderr, "semaphore_p failed\n"); 
		return(0); 
	} 
	return(1);
}

static void del_semvalue(void) // This removes the semaphore from the system
{ 
	union semun sem_union;

	for(resource i = CLIENTS; i <= TRANSFERRED ; i = resource(i+1)){
		if (semctl(semaphores[(int) i ], 0, IPC_RMID, sem_union) == -1) 
			fprintf(stderr, "Failed to delete semaphore\n");
	}
	return(value);

}

static int set_semvalue(void) // This initializes the semaphore
{ 
	union semun sem_union;
	int value = 1;
	sem_union.val = 1; 

	for(resource i = CLIENTS; i <= TRANSFERRED ; i = resource(i+1)){
		semaphores[(int) i] = semget((key_t)(rand()%9999+1000)), 1, 0666 | IPC_CREAT);
		if (semctl(semaphores[(int) i ], 0, SETVAL, sem_union) == -1) 
			value = 0;
	}
	return(value);
}

void serverLog(char* type, char* message){
	while(!semaphore_p(LOG))
				sleep(1);
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
	if (semaphore_v(LOG)==-1)
		exit(EXIT_FAILURE);
}

void registerClient(char* ip){
	int i = 0;
	while(!semaphore_p(IPS))
				sleep(1);
	while(i < CLIENT_AMOUNT*4){
		if(clientIps[i] = '\0'){
			strcpy(clientIps[i], ip);
			break;
		}
		if(strcmp(ip, clientIps[i]) == 0){
			return
		}
	}
	if (semaphore_v(IPS)==-1)
		exit(EXIT_FAILURE);
	if (semaphore_p(CLIENTS)){
		serverInfo.clientCount++;
		if (semaphore_v(CLIENTS)==-1)
			exit(EXIT_FAILURE);
	}					
}

int findAvailableClient(){
	int i = 0;
	while(i < CLIENT_AMOUNT){
		if(!clients[i].occupied)
			return i;
		i++;
	}
	return -1;
}


void serverClose(){
	int i = 0;
	while(i < CLIENT_AMOUNT){
		if(!clients[i].occupied){
			threadResult = pthread_join(clients[i].thread,NULL);
			if(threadResult != 0){
				serverLog("ERROR",strerror(errno));("Thread join");
			}
		}
		i++;
	}
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
			printf("Client count:%d\n", serverInfo.clientCount);
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
			serverLog("STATUS","server shutting down");
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
	char message[256]; //Used for logging purposes
	info.startTime = time;//Time when the thread was created

	sprintf(message,"Thread #%d created! Time: %d-%d-%d %d:%d:%d\n",info.threadNumber, time.tm_year + 1900, time.tm_mon + 1,time.tm_mday, time.tm_hour, time.tm_min, time.tm_sec);
	serverLog("THREAD",message);

	//Read from client 
	read(info.fd,&info.buf,BUFFER_SIZE);

	//Saving client information
	sprintf(info.ip, "%s:%d", inet_ntoa(info.si_other.sin_addr), ntohs(info.si_other.sin_port));
	registerClient(info.ip);
	//print details of the client/peer and the data received
	sprintf(message,"Received packet from %s with data %s\n", info.ip, info.buf);
	serverLog("REQUEST",message);
	if (semaphore_p(REQUESTS)){
		server.requestCount++;
		if (semaphore_v(REQUESTS)==-1)
			exit(EXIT_FAILURE);
	}				
	
	switch (info.buf)
	{
	case "index":
		//Generate index
		break;
	
	default:
		sprintf(message, "resources/%s",info.buf);
		FILE* f = fopen(message,"rb");
		if(f == NULL){
			serverLog("ERROR",strerror(errno));
			exit(1);
		}
		size_t bytesRead = 0;
		// read file in chunks
		while ((bytesRead = fread(info.buf, 1, sizeof(info.buf), f)) > 0)
		{
			if (semaphore_p(TRANSFERRED)){
				serverInfo.transferredBytes += bytesRead;
				if (semaphore_v(TRANSFERRED)==-1)
					exit(EXIT_FAILURE);
			}			
			write(info.fd,&info.buf,BUFFER_SIZE); 
			unsigned int size = bytesRead;
			//printf("%x\n", size);
		}
		break;
	}

	close(info.fd);
	info.occupied = 0;
	pthread_exit(0);
}

int main(int argc, char* argv[]){
	char request[256];
	//create a TCP socket
	if ((s=socket(AF_INET, SOCK_STREAM, 0)) == -1)
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
	//Server terminal thread
	threadResult = pthread_create(), NULL, terminalHandler,NULL);

	if (!set_semvalue()) { 
			serverLog("ERROR", "Failed to initialize semaphores\n"); 
			exit(EXIT_FAILURE); 
		}
	serverLog("STATUS","Semaphores have been initiallized...\n");
	//keep listening for data
	listen(s, CLIENT_AMOUNT);
	while(1)
	{
		serverLog("STATUS","Server waiting for data...\n");

		struct client info = {.occupied = 0, .slen = 0};
		info.slen = sizeof(info.si_other);
		memset((char *) &info.si_other, 0, sizeof(info.si_other));
		
		int clientNumber = findAvailableClient();
		
		if(clientNumber < 0){
			serverLog("STATUS","Server is currently too busy to handle additional requests...\n");
		}
		else{
			//Opening connection
			info.fd = accept(s,(struct sockaddr *)&info.si_other, &info.slen);
			serverInfo.threadCount++;
			info.threadNumber = clientNumber;
			clients[clientNumber] = info;
			threadResult = pthread_create(&(clients[clientNumber].thread), NULL, clientHandler,(void*)&clients[clientNumber]);
			if(threadResult != 0){
				serverLog("ERROR",strerror(errno));
				exit(1);
			}
		}
	}
	return 0;
}