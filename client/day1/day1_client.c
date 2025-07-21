#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>

#define BUF_SIZE 1024

void error_handling(char *msg);

int main(int argc, char*argv[]){

    int sock;
    char buf[BUF_SIZE]="";
    int read_len=0, str_len=0;
    int idx=0;

    FILE *fp;

    struct sockaddr_in serv_addr;

    if(argc != 3){
        printf("Usage %s <IP> <port>\n", argv[0]);
        exit(1);
    }

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if(sock == -1){
        error_handling("socket() error");
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr= inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    if(connect(sock, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) == -1){
        error_handling("connect() error");
    }

    
    int file_list_len;
    read(sock, &file_list_len, sizeof(int));


    /* while(read_len = read(sock, &buf[idx++],1)){
        if(read_len == -1){
            error_handling("check");
        }

        // 파일 리스트 문자열 길이는? 미리 보내는 방식으로
        if(str_len >= file_list_len) break;
        str_len +=read_len;

    } */

    read(sock, buf, file_list_len);
    
    buf[file_list_len]='\0';
    
    char check[] = " ";
    char *result;
    char file_list[30][20];
    int id=0;

    result = strtok(buf, check);

    while(result != NULL){
        strcpy(file_list[id++], result);
        result = strtok(NULL, check);
    }

    for(int i=0; i<id; i++){
       
        if(i%2 == 0) printf("%d: %s\n", (i+1)/2+1, file_list[i]);
        // printf("\n%d: %s\n", i, file_list[i]);

    }

    


    char msg[BUF_SIZE];
    char file_num[BUF_SIZE];
    char number[BUF_SIZE];

    int num, recv_len, read_cnt;
    int recv_cnt=0;

    
    while(1){

        fputs("\nMENU\n", stdout);
        fputs("Input: A (print file list)\n", stdout);
        fputs("Input: B (select the file number)\n", stdout);
        fputs("Input: C (quit)\n\n", stdout);
    
        fgets(msg, BUF_SIZE, stdin);

        if(!strcmp(msg, "C\n")) break;

        if(!strcmp(msg, "A\n")){
            for(int i=0; i<id; i++){
                if(i%2 == 0) printf("%d: %s  |  %s\n", (i+1)/2+1, file_list[i], file_list[i+1]);

                // if(i%2 == 1) printf("%d: %s\n", (i+1)/2+1, file_list[i]);
            }

            continue;
        }

        if(!strcmp(msg, "B\n")){
            // 숫자 예외 처리 나중에 구현
            fputs("Input file number: ", stdout);
            fgets(file_num, BUF_SIZE, stdin);

            num = 2*atoi(file_num)-2;
            //printf("\n%d: ", num);
            //printf("%s\n", file_list[num]);
            
            write(sock, &num, sizeof(int)); //integer로 보낼 수 있음. sizeof(int)로 하면됨.
            //printf("filenum len: %ld", strlen(file_num));
        

            fp=fopen(file_list[num], "wb");

            read_cnt=0;
            /* while ((read_cnt = read(sock, buf, BUF_SIZE)) != 0){
                printf("read() test...\n");
                // if() 파일 크기 설정
                

                str_len+=read_cnt;
                fwrite((void*)buf, 1, read_cnt, fp);

                if(str_len >= atoi(file_list[num+1])) break; 
            }
            printf("finish\n"); */


            int total=0;
            buf[0]='\0';

            char test1[BUF_SIZE];
            while(total < atoi(file_list[num+1])){

                read_cnt = read(sock, test1, BUF_SIZE);

                fwrite((void*)test1, 1, read_cnt, fp);
                total+=read_cnt;
                printf("total: %d\n", total);
            }

            fclose(fp);
            printf("finish\n");
            // printf("%s\n", test1);
    }

}

    close(sock);
    return 0;
}

void error_handling(char *msg){
    printf("%s\n", msg);
    exit(1);
}