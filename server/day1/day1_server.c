#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>

#define BUF_SIZE 1024 //이것도 설정 더 큰 데이터가 들어오면 어떻게 될까?

void error_handling(char *msg);


int main(int argc, char *argv[]){
    
    struct send_pkt;
    DIR * dp;
    struct dirent* file;
    struct stat sb;

    FILE *fp;

    int serv_sock, clnt_sock;
    char buf[BUF_SIZE]="";
    char size[10];
    int idx=0;

    struct sockaddr_in serv_addr, clnt_addr;
    socklen_t clnt_addr_size;

    if(argc != 2){
        printf("Usage %s <port>\n", argv[0]);
        exit(1);
    }

    dp = opendir(".");


    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if(serv_sock == -1){
        error_handling("socket() error");
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[1]));

    if(bind(serv_sock, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) == -1){
        error_handling("bind() error");
    }

    if(listen(serv_sock, 5) == -1){
        error_handling("listen() error");
    }

    clnt_addr_size = sizeof(clnt_addr);
    

    int str_len=0, check=1, read_len=0;
    int num;
    int msg;

    char empty[] = " ";
    char *result;
    char file_list[30][20];
    int id=0;
    int read_cnt;

    
    clnt_sock = accept(serv_sock, (struct sockaddr*) &clnt_addr, &clnt_addr_size);

    while(1){
        if(clnt_sock == -1){
            error_handling("accept() error");
        }
    

        if(check){
            while((file = readdir(dp)) != NULL){

                if(stat(file->d_name, &sb) == -1){
                    error_handling("stat(), error");
                }

                if(S_ISREG(sb.st_mode)){
                    sprintf(size, "%ld", sb.st_size);
                    strcat(buf, file->d_name);
                    strcat(buf, " ");
                    strcat(buf, size);
                    strcat(buf, " ");            
                }
            }
            closedir(dp);

            int file_list_len = strlen(buf);
            if(write(clnt_sock, &file_list_len, sizeof(int)) == -1){
                error_handling("write() error");
            }

            if(write(clnt_sock, buf, file_list_len) == -1){
                error_handling("write() error");
            }

            // printf("%s: %ld", buf, strlen(buf));

            result = strtok(buf, empty);

            while(result != NULL){
                strcpy(file_list[id++], result);
                result = strtok(NULL, empty);
            }


            check=0;
        }
        else{
            idx=0;
            str_len = 0;
            /* while(read_len = read(clnt_sock, &msg, sizeof(int))){
                if(read_len == -1){
                    error_handling("read() error");
                }
                // 
                // if(read_len == 0) break;
                printf("read call: %d\n", str_len);
            } */

            read(clnt_sock, &msg, sizeof(int));

            // msg[strlen(msg)]=0;
            // printf("Message: %d\n", msg);
        
            // num = 2*atoi(msg)-2;
            fp =fopen(file_list[msg], "rb");
            // printf("file name: %s\n", file_list[num]);

            char test[BUF_SIZE];
            while(1){
                read_cnt = fread((void*)test, 1, BUF_SIZE, fp);
                
                if (read_cnt < BUF_SIZE){
                    write(clnt_sock, test, read_cnt);
                    break;
                }
                
                write(clnt_sock, test, read_cnt);

            } 

            // printf("%s\n", test);
            fclose(fp);
            

        }
    
    


// close(clnt_sock);
}
    close(clnt_sock);
    close(serv_sock);
    return 0;
}

void error_handling(char *msg){
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}