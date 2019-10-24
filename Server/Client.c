
// client code for UDP socket programming 
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/socket.h> 
#include <sys/types.h> 
#include <unistd.h> 
#include <sys/time.h>
#define IP_PROTOCOL 0 
#define IP_ADDRESS "127.0.0.1" // localhost 
#define PORT_NO 10521
#define NET_BUF_SIZE 4096
#define sendrecvflag 0 
  
// function to clear buffer 
void clearBuf(char* b) 
{ 
    int i; 
    for (i = 0; i < NET_BUF_SIZE; i++) 
        b[i] = '\0'; 
} 

// function to receive file 
int recvFile(char* buf, int s) 
{ 
    int i; 
    unsigned char value; 
    for (i = 0; i < s; i++) { 
        value = buf[i]; 
        if (value == 0) 
            return 1; 
        else
            printf("%c", value); 
    } 
    return 0; 
} 
  
// driver code 
int main() 
{ 
    long total = 0;
    int sockfd, nBytes; 
    struct sockaddr_in addr_con; 
    int addrlen = sizeof(addr_con); 
    addr_con.sin_family = AF_INET; 
    addr_con.sin_port = htons(PORT_NO); 
    addr_con.sin_addr.s_addr = inet_addr(IP_ADDRESS); 
    unsigned char net_buf[NET_BUF_SIZE]; 
    FILE* fp; 
    sockfd = socket(AF_INET, SOCK_STREAM, IP_PROTOCOL); 
    int flag = 1;
    if (sockfd < 0) 
        printf("\nfile descriptor not received!!\n"); 
    else
        printf("\nfile descriptor %d received\n", sockfd); 
    if((connect(sockfd, (struct sockaddr *) &addr_con, addrlen)) == -1){
        perror("Can't connect to server: ");
        exit(1);
    }

    while (flag) { 
        // socket() 
        clearBuf(net_buf);

        printf("\nPlease enter file name to receive or 'exit':\n");   
        scanf("%s", net_buf);
        if(strcmp(net_buf,"exit")==0){
            flag = 0; }
        write(sockfd, net_buf, NET_BUF_SIZE); 
        printf("\n---------Data Received---------\n"); 
        total = 0;
        while (flag) { 
            // receive 
            clearBuf(net_buf); 
            nBytes = read(sockfd, net_buf, NET_BUF_SIZE); 
            total += nBytes;
            // process 
            unsigned int size = nBytes;
            if (nBytes < NET_BUF_SIZE) {
                break; 
            } 
        } 
        printf("Read %ld bytes!\n",total);
        printf("\n-------------------------------\n");
    }
    close(sockfd);
    return 0; 
} 
