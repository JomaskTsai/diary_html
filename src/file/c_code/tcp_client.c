#include "tcp_client.h"

#define BufferLength 1024  

/*
int main(int argc, char *argv[]) 
{
    if(argc < 4 )
    {
        printf("Usage:  socle_nc SERVER_IP SERVER_PORT MSG\nex: socle_nc 10.63.240.34 8000 \"{(100,100,300)}\"\n");
    }
    else
    {
        return nc_client(argv[1],atoi(argv[2]),argv[3]);
    }       
    return 0;    
}
*/

int nc_client(char *ip , int port , char *msg)
{
    int sd, rc, length = sizeof(int);
    struct sockaddr_in serveraddr;
    char buffer[BufferLength];
    char buffer2[BufferLength];
    char server[255];
    char temp;
    int totalcnt = 0;
    struct hostent *hostp;
    
    if(!ip) 
        return -1;  //invalid ip
        
    if(!msg) 
        return -1;  //invalid msg
    
//    printf("Connecting to the %s, port %d  msg:%s\n\n", ip, port , msg);
    
    if((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    {
        perror("Client-socket() error");
        return -1;
    }
    strcpy(server, ip);
    memset(&serveraddr, 0x00, sizeof(struct sockaddr_in));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(port);

    if((serveraddr.sin_addr.s_addr = inet_addr(server)) == (unsigned long)INADDR_NONE) 
    {
        hostp = gethostbyname(server);
        if(hostp == (struct hostent *)NULL) {
            printf("HOST NOT FOUND --> ");
            printf("h_errno = %d\n",h_errno);
            close(sd);
            return -1;
        }
        memcpy(&serveraddr.sin_addr, hostp->h_addr, sizeof(serveraddr.sin_addr));
    }

    if((rc = connect(sd, (struct sockaddr *)&serveraddr, sizeof(serveraddr))) < 0) {
        perror("Client-connect() error");
        close(sd);
        return -1;
    } 

    rc = write(sd, msg, strlen(msg));
    if(rc < 0) {
        perror("Client-write() error");
        rc = getsockopt(sd, SOL_SOCKET, SO_ERROR, &temp, &length);
        if(rc == 0) {
            /* Print out the asynchronously received error. */
            errno = temp;
            perror("SO_ERROR was");
        }
        close(sd);
        return -1;
    } 

    totalcnt = 0;
    while(totalcnt < BufferLength) 
    {
        rc = read(sd, &buffer[totalcnt], BufferLength-totalcnt);
        if(rc < 0) {
            perror("Client-read() error");
            close(sd);
            return -1 ;
        } 
        else if (rc == 0) 
        {   
            if(totalcnt)
            {
              buffer[totalcnt] = '\0';
              printf("%s", buffer);
            }                     
            close(sd);
            return -1 ;
        } else
            totalcnt += rc;
    }
    close(sd);
    return 0;
}
