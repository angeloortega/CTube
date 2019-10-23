
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
    int sockfd, nBytes; 
    struct sockaddr_in addr_con; 
    int addrlen = sizeof(addr_con); 
    addr_con.sin_family = AF_INET; 
    addr_con.sin_port = htons(PORT_NO); 
    addr_con.sin_addr.s_addr = inet_addr(IP_ADDRESS); 
    unsigned char net_buf[NET_BUF_SIZE]; 
    FILE* fp; 
  
    // socket() 
    sockfd = socket(AF_INET, SOCK_DGRAM, 
                    IP_PROTOCOL); 
  
    if (sockfd < 0) 
        printf("\nfile descriptor not received!!\n"); 
    else
        printf("\nfile descriptor %d received\n", sockfd); 
/*     struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,(char*)&timeout,sizeof(timeout));
 */
    while (1) { 
        clearBuf(net_buf);
        printf("\nPlease enter file name to receive:\n");   
        scanf("%s", net_buf); 
        sendto(sockfd, net_buf, NET_BUF_SIZE, 
               sendrecvflag, (struct sockaddr*)&addr_con, 
               addrlen); 
  
        printf("\n---------Data Received---------\n"); 
  
        while (1) { 
            // receive 
            clearBuf(net_buf); 
            nBytes = recvfrom(sockfd, net_buf, NET_BUF_SIZE, 
                              sendrecvflag, (struct sockaddr*)&addr_con, 
                              &addrlen); 
  
            // process 
            unsigned int size = nBytes;
			printf("%s",net_buf);
            if (nBytes < NET_BUF_SIZE) {
                printf("Received last package\n");
                
                break; 
            } 
        } 
        printf("\n-------------------------------\n"); 
    } 
    return 0; 
} 
