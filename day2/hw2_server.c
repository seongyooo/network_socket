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

typedef struct{
    unsigned int seq;
    unsigned int data_len;
    unsigned long file_size;
    unsigned char data[BUF_SIZE];
}pkt_t;

int main(int argc, char *argv[]){
    clock_t start_time, end, s, e;
    double duration;
    double timeout = 1000;

    int serv_sock;
    int total_data_len=0;
    socklen_t clnt_adr_sz;

    pkt_t serv_pkt;

    struct sockaddr_in serv_adr, clnt_adr;
    if(argc !=3){
        printf("Usage: %s <port> <file_name>\n", argv[0]);
        exit(1);
    }

    // socket 생성
    serv_sock = socket(PF_INET, SOCK_DGRAM, 0);
    if(serv_sock == -1){
        error_handling("socket");
    }

    int flag = fcntl(serv_sock, F_GETFL, 0);
    fcntl(serv_sock, F_SETFL, flag | O_NONBLOCK);

    // bind
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    if(bind(serv_sock, (struct sockaddr*) &serv_adr, sizeof(serv_adr)) == -1){
        error_handling("bind()");
    }

    char file_name[BUF_SIZE]; 
    strcpy(file_name, argv[2]); // 보낼 파일 정하기

    int file_name_len = strlen(file_name);
    int start=0;
    long file_size;
    struct stat sb;

    FILE *fp;
    char buf[BUF_SIZE];
    int read_cnt;
    int seq_check;

    clnt_adr_sz = sizeof(clnt_adr);

    // 시작하기 위한 recvfrom()
    while(1){
        recvfrom(serv_sock, &start, sizeof(int), 0, (struct sockaddr*) &clnt_adr, &clnt_adr_sz);
        printf("Start %d\n\n", start);

        if(start) break;
    }

    //구조체 보내기
    start_time = clock();
    serv_pkt.seq = 1; 
    int loss=0;

    while(1){
        printf("SEQ: %d\n", serv_pkt.seq);
        sendto(serv_sock, &serv_pkt, sizeof(pkt_t), 0, (struct sockaddr*) &clnt_adr, clnt_adr_sz);
        recvfrom(serv_sock, &seq_check, sizeof(int), 0, (struct sockaddr*) &clnt_adr, &clnt_adr_sz);

        printf("ACK: %d\n\n", seq_check);
        if(seq_check == serv_pkt.seq) {
            serv_pkt.seq++;
            break;
        }
        loss++;
    }
    printf("first loss: %d\n", loss);

    // 파일 사이즈 구하기
    if(stat(file_name, &sb) == -1){
        error_handling("stat(), error");
    }
    file_size = sb.st_size;
    
    // 파일 이름 크기, 이름, 파일 사이즈 전송
    serv_pkt.data_len = file_name_len;
    serv_pkt.file_size = file_size;
    strcpy(serv_pkt.data, file_name);
    total_data_len+=file_name_len;

    
    while(1){
        printf("SEQ: %d\n", serv_pkt.seq);
        sendto(serv_sock, &serv_pkt, sizeof(pkt_t), 0, (struct sockaddr*) &clnt_adr, clnt_adr_sz);
        recvfrom(serv_sock, &seq_check, sizeof(int), 0, (struct sockaddr*) &clnt_adr, &clnt_adr_sz);
        
        printf("ACK: %d\n\n", seq_check);
        if(seq_check == serv_pkt.seq) {
            serv_pkt.seq++;
            break;
        }
        loss++;
    }
    printf("file_data loss: %d\n", loss);

    
    // 파일 내용 전송
    fp =fopen(file_name, "rb");
    while(1){
        // read_cnt = fread((void*)buf, 1, BUF_SIZE, fp);
        read_cnt = fread((void*)serv_pkt.data, 1, BUF_SIZE, fp);
        total_data_len+=read_cnt;

        if (read_cnt < BUF_SIZE){
            serv_pkt.data_len=read_cnt; 

            // serv_pkt.data를 바로 읽기(네트워크 프로그래밍에서는 strcpy 가급적 X)
            // strcpy(serv_pkt.data, buf); // 바이너리 파일은 여기서 string으로 되서 문제 발생. 그러면 seq랑 같이 보내는 방법은?
           
            while(1){
                printf("SEQ: %d\n", serv_pkt.seq);
                sendto(serv_sock, &serv_pkt, sizeof(pkt_t), 0, (struct sockaddr*) &clnt_adr, clnt_adr_sz);
                recvfrom(serv_sock, &seq_check, sizeof(int), 0, (struct sockaddr*) &clnt_adr, &clnt_adr_sz);

                printf("ACK: %d\n\n", seq_check);
                if(seq_check == serv_pkt.seq) {
                    break;
                }
                loss++;
            }
            break;
        }
        serv_pkt.data_len=read_cnt;
        // strcpy(serv_pkt.data, buf);
    
        while(1){
            printf("SEQ: %d\n", serv_pkt.seq);
            sendto(serv_sock, &serv_pkt, sizeof(pkt_t), 0, (struct sockaddr*) &clnt_adr, clnt_adr_sz);
            recvfrom(serv_sock, &seq_check, sizeof(int), 0, (struct sockaddr*) &clnt_adr, &clnt_adr_sz);

            printf("ACK: %d\n\n", seq_check);
            if(seq_check == serv_pkt.seq) {
                serv_pkt.seq++;
                break;
            }
            loss++;
        }
    } 
    fclose(fp);

    end = clock();
    duration = (double)(end-start_time);
    printf("Throughput(byte/ms): %f\n", total_data_len/duration);
    printf("time(ms): %f\n",duration/10000);
    printf("\nLoss count: %d\n", loss);

    //close()
    close(serv_sock);
    return 0;
}

void error_handling(char *msg){
    printf("%s error\n", msg);
    exit(1);
}