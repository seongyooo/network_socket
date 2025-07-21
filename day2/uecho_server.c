#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUF_SIZE 1024
void error_handling(char *msg);

int main(int argc, char *argv[]){
    int serv_sock;
    char msg[BUF_SIZE];
    int str_len;
    socklen_t clnt_adr_sz;

    struct sockaddr_in serv_adr, clnt_adr;
    if(argc !=2){
        printf("Usage: %s <port>\n", argv[0]);
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
    serv_adr.sin_port = htons(atoi(argv[1]));

    if(bind(serv_sock, (struct sockaddr*) &serv_adr, sizeof(serv_adr)) == -1){
        error_handling("bind()");
    }

    // recvfrom() & sendto()
    while(1){
        clnt_adr_sz = sizeof(clnt_adr);
        str_len = recvfrom(serv_sock, msg, BUF_SIZE, 0, (struct sockaddr*) &clnt_adr, &clnt_adr_sz);
        
        sendto(serv_sock, msg, str_len, 0, (struct sockaddr*) &clnt_adr, clnt_adr_sz);
    }

    //close()
    close(serv_sock);
    return 0;
}

void error_handling(char *msg){
    printf("%s error\n", msg);
    exit(1);
}