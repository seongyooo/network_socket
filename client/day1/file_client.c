#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUF_SIZE 1024
void error_handling(char *msg);

int main(int argc, char *argv[]){
    int sd;
    FILE *fp;

    char buf[BUF_SIZE];
    int read_cnt;

    struct sockaddr_in serv_adr;

    if(argc != 3){
        printf("Usage %s <IP> <port> \n", argv[0]);
        exit(1);
    }

    fp=fopen("receive.c", "wb");
    sd = socket(PF_INET, SOCK_STREAM, 0);
    if(sd == -1){
        error_handling("socket() error");
    }

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr= inet_addr(argv[1]);
    serv_adr.sin_port = htons(atoi(argv[2]));

    if(connect(sd, (struct sockaddr*) &serv_adr, sizeof(serv_adr)) == -1){
        error_handling("connect() error");
    }

    while((read_cnt = read(sd, buf, BUF_SIZE)) != 0){
        fwrite((void*)buf, 1, read_cnt, fp);
    }

    puts("Received file data");
    write(sd, "Thank you", 10);

    fclose(fp);
    close(sd);

    return 0;

}

void error_handling(char *msg){
    printf("%s", msg);
    exit(1);
}