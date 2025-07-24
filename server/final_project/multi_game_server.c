#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>

#define BUF_SIZE 1024
void error_handling(char *msg);

int main(int argc, char *argv[]){
    int serv_sock;
    socklen_t clnt_adr_sz;

    struct sockaddr_in serv_adr, clnt_adr;

    // ./server 2 4 4 30 1234
    if(argc !=6){
        printf("Usage: %s <player> <grid size> <panel> <time> <port>\n", argv[0]);
        exit(1);
    }

    // socket 생성
    serv_sock = socket(PF_INET, SOCK_DGRAM, 0);
    if(serv_sock == -1){
        error_handling("socket");
    }

    // bind
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[5]));

    if(bind(serv_sock, (struct sockaddr*) &serv_adr, sizeof(serv_adr)) == -1){
        error_handling("bind()");
    }

    clnt_adr_sz = sizeof(clnt_adr);
    // 시작하기 위한 recvfrom()
    int start=0;
    while(1){
        recvfrom(serv_sock, &start, sizeof(int), 0, (struct sockaddr*) &clnt_adr, &clnt_adr_sz);
        printf("Start %d\n", start);

        if(start) break;
    }


    close(serv_sock);
    return 0;
}

void error_handling(char *msg){
    printf("%s error\n", msg);
    exit(1);
}