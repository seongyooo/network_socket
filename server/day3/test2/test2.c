#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>

#define BUF_SIZE 1024
// 203.252.112.31
void error_handling(char *msg);

int main(int argc, char *argv[]){
    int serv_sd, clnt_sd;
    FILE *fp;
    char buf[BUF_SIZE];
    int read_cnt;

    struct timeval timeout;
    fd_set reads, cpy_reads;

    int fd_max, str_len, fd_num, i;
    
    struct sockaddr_in serv_addr, clnt_addr;
    socklen_t clnt_addr_size;

    if(argc != 2){
        printf("Usage %s <port>\n", argv[0]);
        exit(1);
    }

    fp = fopen("file_server.c", "rb");
    serv_sd = socket(PF_INET, SOCK_STREAM, 0);
    if(serv_sd == -1){
        error_handling("socket() error");
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[1]));

    if(bind(serv_sd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) == -1){
        error_handling("bind() error");
    }

    if(listen(serv_sd, 5) == -1){
        error_handling("listen() error");
    }

    FD_ZERO(&reads);
    FD_SET(serv_sd, &reads);
    fd_max = serv_sd;

    while(1){
        cpy_reads = reads;
        timeout.tv_sec = 5;
        timeout.tv_usec=5000;

        if((fd_num = select(fd_max+1, &cpy_reads, 0, 0, &timeout)) == -1){
            break;
        }

        printf("loop\n");
        if(fd_num == 0){
            continue;
        }

        for(int i=0; i<fd_max+1; i++){
            if(FD_ISSET(i, &cpy_reads)){
                if(i == serv_sd){
                    clnt_addr_size = sizeof(clnt_addr);
                    printf("check!\n");
                    clnt_sd = accept(serv_sd, (struct sockaddr*) &clnt_addr, &clnt_addr_size);
                    if(clnt_sd == -1){
                        error_handling("accept() error");
                    }

                    FD_SET(clnt_sd,&reads);
                    if(fd_max < clnt_sd){
                        fd_max = clnt_sd;
                    }
                    printf("Connected client: %d\n", clnt_sd);
                }
                else{
                    fp = fopen("select_file_server.c", "rb");
                    while(1){
                        read_cnt = fread((void*)buf, 1, BUF_SIZE, fp);
                        if(read_cnt < BUF_SIZE){
                            write(i, buf, read_cnt);
                            break;
                        }
                        write(i, buf, BUF_SIZE);
                    }
                    fclose(fp);
                    close(i);
                    printf("Closed client: %d\n", i);
                }
            }
        }
    }

    close(clnt_sd);
    close(serv_sd);

    return 0;
    
}

void error_handling(char *msg){
    printf("%s\n", msg);
    exit(1);
}