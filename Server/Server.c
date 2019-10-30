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
#include <sys/shm.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <assert.h>
#include <dirent.h>


#define PORT 8080
#define BUFFER_SIZE 524288
#define CLIENT_AMOUNT 25
#define CHUNK_SIZE 524093 //BUFFER_SIZE - http content
#define DIFFERENCE 195

typedef enum {CLIENTS=0,IPS=1,LOG=2,REQUESTS=3,TRANSFERRED=4} resource;

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short  *array;
};

struct client{
	char ip[25];
	struct tm startTime;
	struct sockaddr_in si_other;
	/*unsigned*/ char buf[BUFFER_SIZE];
	socklen_t slen;
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
	pthread_t thread;

};

int s;
static int threadResult = 0;
static struct server serverInfo;
static struct client clients[CLIENT_AMOUNT];
static struct sockaddr_in si_me;
static int semaphores[5];

static int semaphore_v(resource res){
	struct sembuf sem_b;
	sem_b.sem_num = 0; 
	sem_b.sem_op = 1; 
	sem_b.sem_flg = SEM_UNDO;
	int item = (int) res;
	if (semop(semaphores[item], &sem_b, 1) == -1) { 
		perror("semaphore_v failed\n"); 
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
	if (semop(semaphores[item], &sem_b, 1) == -1) { 
		perror("semaphore_p failed\n"); 
		return(0); 
	} 
	return(1);
}

static void del_semvalue(void) // This removes the semaphore from the system
{ 
	union semun sem_union;

	for(int i = 0; i < 5 ; i++){
		if (semctl(semaphores[i], 0, IPC_RMID, sem_union) == -1) 
			perror("Failed to delete semaphore\n");
	}
}

static int set_semvalue(void) // This initializes the semaphore
{ 
	union semun sem_union;
	int value = 1;
	sem_union.val = 1; 
	int random = 0;
	for(int i = 0; i < 5 ; i++){
		random = rand()%8999+1000;
		semaphores[(int) i] = semget((key_t) random,1, 0666 | IPC_CREAT);
		if (semctl(semaphores[i], 0, SETVAL, sem_union) == -1)
			value = 0;
	}
	return(value);
}

void serverLog(char* type, char* message){
	int flag;
	do{
		flag = semaphore_p(LOG);
		if(!flag){
			sleep(1);
		}
	}
	while(!flag);
	time_t t = time(NULL);
 	struct tm *startTime = localtime(&t);
	char fileName[256];
	sprintf(fileName,"logs/%d-%d-%d.txt", (*startTime).tm_year + 1900,  (*startTime).tm_mon + 1, (*startTime).tm_mday);
	FILE* f = fopen(fileName,"a");
	if(f == NULL){
		f = fopen(fileName,"w");
		if(f == NULL){
			perror("Logging error: ");
		}
	}
	else{
		printf("%d:%d:%d - %s: %s\n",  (*startTime).tm_hour,  (*startTime).tm_min,  (*startTime).tm_sec, type, message);
		fprintf(f, "%d:%d:%d - %s: %s",  (*startTime).tm_hour, (*startTime).tm_min,  (*startTime).tm_sec, type, message);
		fclose(f);

	}
	if (semaphore_v(LOG)==-1)
		exit(EXIT_FAILURE);
}

void registerClient(){
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

char** strSplit(char* input, const char a_delim)
{
    char* a_str = (char *)malloc(strlen(input)+1);
    strcpy(a_str,input);
    char** result    = 0;
    size_t count     = 0;
    char* tmp        = a_str;
    char* last_comma = 0;
    char delim[2];
    delim[0] = a_delim;
    delim[1] = 0;

    /* Count how many elements will be extracted. */
    while (*tmp)
    {
        if (a_delim == *tmp)
        {
            count++;
            last_comma = tmp;
        }
        tmp++;
    }

    /* Add space for trailing token. */
    count += last_comma < (a_str + strlen(a_str) - 1);

    /* Add space for terminating null string so caller
       knows where the list of returned strings ends. */
    count++;

    result = malloc(sizeof(char*) * count);

    if (result)
    {
        size_t idx  = 0;
        char* token = strtok(a_str, delim);

        while (token)
        {
            assert(idx < count);
            *(result + idx++) = strdup(token);
            token = strtok(0, delim);
        }
        assert(idx == count - 1);
        *(result + idx) = 0;
    }
	free(a_str);

    return result;
}

void regenerate(){
	char content[BUFFER_SIZE] = "[";
	char intermediate[4096];
	char fileName[128] = "resources/json/videos.json";
	char filePath[128];
	char categories [3][25] = {"FMS Argentina", "FMS Espanha", "FMS Internacional"};
	char imageName[25];
	long fileSize;
	int i = 0;
	remove(fileName);
	FILE* f = fopen(fileName,"a");
	FILE* f2;
	DIR *d;
	char date[10];
	time_t t = time(NULL);
 	struct tm timeS = *localtime(&t);
	sprintf(date,"%d-%d-%d",timeS.tm_mday, timeS.tm_mon + 1, timeS.tm_year + 1900);
	
    struct dirent *dir;
    d = opendir("resources/video");
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
			if(!((dir->d_name)[0] == '.')){
				sprintf(filePath,"resources/video/%s", dir->d_name);
				f2 = fopen(filePath,"rb");
				fseek (f2 , 0 , SEEK_END);
				fileSize = ftell(f2); 
				strcpy(imageName,dir->d_name);
				fclose(f2);
				int nameSize = strlen(imageName);
				imageName[nameSize-1] = 'g';
				imageName[nameSize-2] = 'p';
				imageName[nameSize-3] = 'j';
				if(!i)
					sprintf(intermediate,"\n{ \"url\": \"%s\",\n\"title\": \"%s\",\"description\" : \"%s\",\n\"size\" : %ld,\n\"creationDate\" : \"%s\",\n\"category\" : \"%s\",\n\"poster\" : \"%s\"\n}",dir->d_name,dir->d_name,dir->d_name,fileSize,date,categories[i%3],imageName);
				else
					sprintf(intermediate,",\n{ \"url\": \"%s\",\n\"title\": \"%s\",\"description\" : \"%s\",\n\"size\" : %ld,\n\"creationDate\" : \"%s\",\n\"category\" : \"%s\",\n\"poster\" : \"%s\"\n}",dir->d_name,dir->d_name,dir->d_name,fileSize,date,categories[i%3],imageName);
				i++;
				strcat(content,intermediate);
			}
        }
        closedir(d);
    }
	strcat(content,"\n]");
	if(f == NULL){
		f = fopen(fileName,"w");
		if(f == NULL){
			perror("Video genereation error: ");
		}
	}
	else{
		fprintf(f, "%s",  content);
		fclose(f);
	}
}

void parseRequest(char* result[],char *request){
	//Only GET requests are supported
	//EX GET /video/small.mp4/0 HTTP/1.1
	// GET /page/index.html
	// GET /image/image.png
    char messageType[128];
	char message[128];
	char** parts;
	char* subbuff;
	char * pch;
	char * request1 = (char *)malloc(strlen(request)+1);
	strcpy(request1, request);
	pch = strstr (request1,"\r\n");
	strncpy (pch,"\0\0",2);
	parts = strSplit(request1, '/');
	if(parts){
        strcpy(messageType, *(parts + 1));
		strcpy(message, *(parts + 2));
		int len = strlen(message) - 4; //Doesn't take " HTTP" in consideration
		if(len > 0){
			subbuff = malloc(sizeof(char*) *len);
			memcpy(subbuff, &message[0], len-1);
			subbuff[len-1] = '\0';
			result[1] = malloc(sizeof(char*) *128);
			strcpy(result[1], subbuff);
			free(subbuff);
		}
		//}
	}
	
    result[0] = malloc(sizeof(char*) *128);
	
    strcpy(result[0], messageType);
	free(parts);
	free(request1);
	
} 

void parseRange(long result[],char *request){
	//Only GET requests are supported
	//EX GET /video/small.mp4/0 HTTP/1.1
	// GET /page/index.html
	// GET /image/image.png
    char beginningC[128];
	char endC[128];
	long beginning = 0l;
	long end = 0l;
	int flag = 0;
	char** parts;
	char *location;
	location = strstr(request,"Range:");
	if(location){
		parts = strSplit(location, '-');
		if(parts){
			if(*(parts+1)[0] >= '0' && *(parts+1)[0] <= '9'){
				flag = 1;
				strcpy(endC, *(parts+1));
			}
			strcpy(beginningC, *parts);
			parts = strSplit(beginningC, '=');
			if(parts){
				strcpy(beginningC, *(parts+1));
			}
			
		}
	}
	int digitLength = strlen(beginningC);
	for(int i=0; i<digitLength; i++){
		beginning = beginning * 10l + (beginningC[i] - '0' );
	}
	if(flag){
		digitLength = strlen(endC);
		for(int i=0; i<digitLength; i++){
			end = end * 10l + (endC[i] - '0' );
		}
	}
	else{
		end = -1;
	}
	free(parts);
	result[0] = beginning;
	result[1] = end;
} 


void serverClose(){
	int i = 0;
	while(i < CLIENT_AMOUNT){
		if(clients[i].occupied){
			close(clients[i].fd);
			threadResult = pthread_join(clients[i].thread,NULL);
			if(threadResult != 0){
				serverLog("ERROR",strerror(errno));
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
		sleep(3);
		printf("------------------options------------------\n");
		printf("1)------------------------Server Start Time\n");
		printf("2)--------------Amount of transferred bytes\n");
		printf("3)----------------------Unique client count\n");
		printf("4)---------------------------Request amount\n");
		printf("5)--------------------------Threads created\n");
		printf("6)-------------------------Regenerate index\n");
		printf("7)-------------------------------------Exit\n");
		printf("-------------------------------------------\n");
		printf("Please select an option: ");
		scanf(" %c",&option);
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
			//TODO Regenerate index
			regenerate();
			serverLog("STATUS","Index regenerated");
			break;
		case '7':
			flag = 0;
			close(s);
			printf("Closing server...\n");
			serverLog("STATUS","server shutting down");
			del_semvalue();
			break;
		default:
			printf("Invalid option selected: %c\n",option);
			break;
		}
		printf("\n\n\n");
	}
	exit(0);
}

void *clientHandler(void *arg){
	char message[BUFFER_SIZE];
	char *request[2];
	char * fileReaded = malloc(sizeof(char*) * 128);
	long range[2];
	struct client info =  *(struct client *)arg;
	long fileSize = 0;
	int message_length;
	long total = 0;
	info.occupied = 1;
	time_t t = time(NULL);
 	struct tm timeS = *localtime(&t);
	info.startTime = timeS;//Time when the thread was created
	sprintf(info.ip, "%s:%d", inet_ntoa(info.si_other.sin_addr), ntohs(info.si_other.sin_port));
	registerClient();
	sprintf(message,"Thread with IP %s created! Time: %d-%d-%d %d:%d:%d\n",info.ip, timeS.tm_year + 1900, timeS.tm_mon + 1,timeS.tm_mday, timeS.tm_hour, timeS.tm_min, timeS.tm_sec);
	serverLog("THREAD",message);
	//Read from client 
	read(info.fd,&info.buf,BUFFER_SIZE);
	printf("Request:\n %s", info.buf);
	if(info.buf[0] == '\0' || strstr(info.buf, "style")){
		//write(info.fd,'\0',1);
	}
	else{
		parseRequest(request, info.buf);
		sprintf(message, "resources/%s/%s",request[0],request[1]); //Example: resources/video/small.mp4
		serverLog("REQUEST",info.buf);
		total = 0;
		if (semaphore_p(REQUESTS)){
			serverInfo.requestCount++;
			if (semaphore_v(REQUESTS)==-1)
				exit(EXIT_FAILURE);
		}
		FILE* f = fopen(message,"rb");
		if(f == NULL){
			serverLog("ERROR",strerror(errno));
			sprintf(info.buf,"HTTP/1.1 404 ERROR\nContent-Type: text/plain\nContent-Length: 19\n\nInvalid GET request");
			write(info.fd,&info.buf, strlen(info.buf) + DIFFERENCE); 
			memset(&message, 0, BUFFER_SIZE);
			memset(&info.buf, 0, BUFFER_SIZE);
		}
		else{
			if(strcmp(request[0],"video") == 0){
				size_t bytesRead = 0;
				strcpy(fileReaded,request[1]);
				fseek (f , 0 , SEEK_END);
				fileSize = ftell(f); 
				rewind (f);
				//Getting the file sizesss
				//Send range support and first chunk
				printf("info :%s",info.buf);
				parseRange(range, info.buf);
				fseek(f,range[0],SEEK_SET);
				
				bytesRead = fread(message, 1, (size_t) CHUNK_SIZE, f);
				if(range[1] > fileSize){
					range[1] = fileSize-1;
				}
				if(range[1] < 0 && range[0] == 0){
					sprintf(info.buf,"HTTP/1.1 200 OK\r\nContent-Type: video/mp4\r\nContent-Length: %d\r\nAccept-Ranges: bytes\r\n\r\n",(int)fileSize);
				}
				else{
					if(range[1] < 0)
					{
						range[1] = (bytesRead + range[0])-1;
					}
					sprintf(info.buf,"HTTP/1.1 206 Partial Content\r\nContent-Type: video/mp4\r\nContent-Length: %d\r\nAccept-Ranges: bytes\r\nContent-Range: bytes %ld-%ld/%ld\r\n\r\n",(int)(range[1] - range[0] + 1),range[0],range[1],fileSize);
					
				}

				message_length = strlen(info.buf);
				memcpy(&info.buf[message_length], &message[0], (size_t) bytesRead);
				total += bytesRead;
				write(info.fd,&info.buf, bytesRead + message_length);
				memset(&message, 0, BUFFER_SIZE);
				memset(&info.buf, 0, BUFFER_SIZE);

				fclose(f);
			}
			else{
				if(strcmp(request[0],"image") == 0){
					size_t bytesRead = 0;
					bytesRead = fread(message, 1, (size_t) CHUNK_SIZE, f);
					fileSize = bytesRead;
					sprintf(info.buf,"HTTP/1.1 200 OK\r\nContent-Type: image/jpg\r\nContent-Length: %d\r\nAccept-Ranges: bytes\r\n\r\n",(int)fileSize);
					message_length = strlen(info.buf);
					memcpy(&info.buf[message_length], &message[0], (size_t) bytesRead);
					total += bytesRead;
					write(info.fd,&info.buf, bytesRead + message_length);
					memset(&message, 0, BUFFER_SIZE);
					memset(&info.buf, 0, BUFFER_SIZE);
					fclose(f);
					
				}
				else{

					//Index
					//Read JSON
					//Compare
					//Cambio o no

					if(strcmp(request[0],"page") == 0){
						size_t bytesRead = 0;
						bytesRead = fread(message, 1, (size_t) CHUNK_SIZE, f);
						fileSize = bytesRead;
						sprintf(info.buf,"HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\nContent-Type: text/html\r\nContent-Length: %d\r\n\r\n",(int)fileSize);
						message_length = strlen(info.buf);
						memcpy(&info.buf[message_length], &message[0], (size_t) bytesRead);
						total += bytesRead;
						write(info.fd,&info.buf, bytesRead + message_length);
						memset(&message, 0, BUFFER_SIZE);
						memset(&info.buf, 0, BUFFER_SIZE);
						fclose(f);
					
					}
					else{
						if(strcmp(request[0],"json") == 0){
						size_t bytesRead = 0;
						bytesRead = fread(message, 1, (size_t) CHUNK_SIZE, f);
						fileSize = bytesRead;
						sprintf(info.buf,"HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\nContent-Type: text/plain\r\nContent-Length: %d\r\n\r\n",(int)fileSize);
						message_length = strlen(info.buf);
						memcpy(&info.buf[message_length], &message[0], (size_t) bytesRead);
						total += bytesRead;
						write(info.fd,&info.buf, bytesRead + message_length);
						memset(&message, 0, BUFFER_SIZE);
						memset(&info.buf, 0, BUFFER_SIZE);
						fclose(f);
					
						}
						else{
							sprintf(info.buf,"HTTP/1.1 404 ERROR\nContent-Type: text/plain\nContent-Length: 19\n\nInvalid GET request");
							write(info.fd,&info.buf, strlen(info.buf) + DIFFERENCE); 
							memset(&message, 0, BUFFER_SIZE);
							memset(&info.buf, 0, BUFFER_SIZE);
						}
					}
					
				}
			}
		}		
		if (semaphore_p(TRANSFERRED)){
				serverInfo.transferredBytes += total;
				if (semaphore_v(TRANSFERRED)==-1)
					exit(EXIT_FAILURE);
		}
	}
	sprintf(message, "Sent: %ld bytes",total);
	serverLog("REQUEST",message);
	t = time(NULL);
	timeS = *localtime(&t);
	info.startTime = timeS;//Time when the thread was destroyed
	sprintf(message,"Thread with IP %s stopped! Time: %d-%d-%d %d:%d:%d\n",info.ip, timeS.tm_year + 1900, timeS.tm_mon + 1,timeS.tm_mday, timeS.tm_hour, timeS.tm_min, timeS.tm_sec);
	serverLog("THREAD",message);
	close(info.fd);
	free(fileReaded);
	free(request[0]);
	free(request[1]);
	info.occupied = 0;
	pthread_exit(0);
}

int main(int argc, char* argv[]){
	srand(time(NULL));
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

	if (!set_semvalue()) { 
			serverLog("ERROR", "Failed to initialize semaphores"); 
			exit(EXIT_FAILURE); 
		}
	//Server terminal thread
	threadResult = pthread_create(&serverInfo.thread, NULL, terminalHandler,NULL);

	serverLog("STATUS","Semaphores have been initiallized...");
	//keep listening for data
	listen(s,CLIENT_AMOUNT);
	signal(SIGCHLD,SIG_IGN);
	int clientNumber = 0;
	while(1)
	{
		serverLog("STATUS","Server waiting for data...");

		struct client info = {.occupied = 0, .slen = 0};
		info.slen = sizeof(info.si_other);
		memset((char *) &info.si_other, 0, sizeof(info.si_other));
		
		clientNumber++;
		clientNumber = clientNumber % CLIENT_AMOUNT;
		if(clientNumber < 0){
			serverLog("STATUS","Server is currently too busy to handle additional requests...");
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