#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>

// 203.252.112.31
#define BUF_SiZE 1024

void error_handling(char *msg);

int main(int argc, char *argv[]){
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;

    struct timeval timeout;
    fd_set reads, cpy_reads;

    socklen_t adr_sz;
    int fd_max, str_len, fd_num, i;
    char buf[BUF_SiZE];

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    if(bind(serv_sock, (struct sockaddr*) &serv_adr, sizeof(serv_adr)) == -1){
        error_handling("bind() error");
    }

    if(listen(serv_sock, 5) == -1){
        error_handling("listen() error");
    }

    FD_ZERO(&reads);
    FD_SET(serv_sock, &reads);
    fd_max = serv_sock;

    while(1){
        cpy_reads = reads;
        timeout.tv_sec=5;
        timeout.tv_usec=5000;

        if((fd_num = select(fd_max+1, &cpy_reads, 0, 0, &timeout)) == -1)
            break;

        // event 발생 없이, timeout이 지났을 때
        if(fd_num == 0)
            continue;
        
        // fd_num이 양수일때 -> 현재 event가 발생한 디스크립터 수가 있을때
        for(int i=0; i<fd_max+1; i++){
            if(FD_ISSET(i, &cpy_reads)){
                if(i==serv_sock){ // connection request!
                    adr_sz = sizeof(clnt_adr);
                    clnt_sock = accept(serv_sock, (struct sockaddr*) &clnt_adr, &adr_sz);
                    FD_SET(clnt_sock, &reads);
                    if(fd_max < clnt_sock)
                        fd_max = clnt_sock;
                    printf("connected client: %d\n", clnt_sock);
                }
                else{ // read message!
                    str_len = read(i, buf, BUF_SiZE);

                    if(str_len == 0){ // close requset
                        FD_CLR(i, &reads);
                        close(i);
                        printf("closed client: %d\n", i);
                    }
                    else{
                        write(i, buf, str_len); // echo
                    }
                
                }
            }
        }
        

    }

}


void error_handling(char *msg){
    printf("%s", msg);
    exit(1);
}