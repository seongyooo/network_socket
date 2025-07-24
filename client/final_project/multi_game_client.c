#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUF_SIZE 1024
void error_handling(char *msg);


int main(int argc, char *argv[]){
    int sock;
    socklen_t adr_sz;

    struct sockaddr_in serv_adr, from_adr;

    // ./client 203.252.112.31 1234
    if (argc != 3) {
        printf("Usage : %s <IP> <port>\n", argv[0]);
        exit(1);
    }

    //socket()
    sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (sock == -1)
        error_handling("socket() error");
    
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family=AF_INET;
    serv_adr.sin_addr.s_addr=inet_addr(argv[1]);
    serv_adr.sin_port=htons(atoi(argv[2]));

    int start=1;
    // 시작하기 위한 send하기
    sendto(sock, &start, sizeof(int), 0, (struct sockaddr*) &serv_adr, sizeof(serv_adr));

    

    close(sock);
    return 0;
}

void error_handling(char *msg){
    printf("%s error\n", msg);
    exit(1);
}