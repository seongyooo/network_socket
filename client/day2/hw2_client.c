#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
// #include <fcntl.h>

#define BUF_SIZE 1024
void error_handling(char *msg);

typedef struct{
    unsigned int seq;
    unsigned int data_len;
    unsigned long file_size;
    unsigned char data[BUF_SIZE];
}pkt_t;

int main(int argc, char *argv[]){
    int sock;
    socklen_t adr_sz;

    pkt_t clnt_pkt;

    struct sockaddr_in serv_adr, from_adr;
    if (argc != 3) {
        printf("Usage : %s <IP> <port>\n", argv[0]);
        exit(1);
    }

    //socket()
    sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (sock == -1)
        error_handling("socket() error");

    // int flag = fcntl(sock, F_GETFL, 0);
    // fcntl(sock, F_SETFL, flag | O_NONBLOCK);


    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family=AF_INET;
    serv_adr.sin_addr.s_addr=inet_addr(argv[1]);
    serv_adr.sin_port=htons(atoi(argv[2]));

    int start=1;
    char file_name[BUF_SIZE];
    long file_size;

    FILE *fp;
    int total;
    int read_cnt;
    int seq_check=1; // 혹시 모르니 확인 
    clnt_pkt.seq=0;
    int num=1;

    // 시작하기 위한 send하기
    sendto(sock, &start, sizeof(int), 0, (struct sockaddr*) &serv_adr, sizeof(serv_adr));

    // 구조체 왔는지 확인
    while(num != clnt_pkt.seq){
        recvfrom(sock, &clnt_pkt, sizeof(pkt_t), 0, (struct sockaddr*) &from_adr, &adr_sz);
        seq_check=clnt_pkt.seq;
        sendto(sock, &seq_check, sizeof(int), 0, (struct sockaddr*) &serv_adr, sizeof(serv_adr));
        
        printf("SEQ: %d\n", clnt_pkt.seq);
    }
    num=seq_check;
    num++;

    // client에 seq가 먼저 증가하고 나서 sendto를 하는 상황 발생.
    // 파일 이름 크기, 이름, 파일 사이즈 받기
    while(num != clnt_pkt.seq){
        recvfrom(sock, &clnt_pkt, sizeof(pkt_t), 0, (struct sockaddr*) &from_adr, &adr_sz);
        seq_check=clnt_pkt.seq;
        sendto(sock, &seq_check, sizeof(int), 0, (struct sockaddr*) &serv_adr, sizeof(serv_adr));
        
        printf("SEQ: %d\n", clnt_pkt.seq);
    }
    num=seq_check;
    num++;

    file_size = clnt_pkt.file_size;
    strcpy(file_name, clnt_pkt.data);

    printf("\nseq: %d\n", clnt_pkt.seq);
    printf("data_len: %d\n", clnt_pkt.data_len);
    printf("data: %s\n", clnt_pkt.data);
    printf("file_size: %ld\n\n", clnt_pkt.file_size);

    // 파일 내용 받기
    fp=fopen(file_name, "wb");
    total=0;

    while(total < file_size){
        while(num != clnt_pkt.seq){
            recvfrom(sock, &clnt_pkt, sizeof(pkt_t), 0, (struct sockaddr*) &from_adr, &adr_sz);
            seq_check=clnt_pkt.seq;
            sendto(sock, &seq_check, sizeof(int), 0, (struct sockaddr*) &serv_adr, sizeof(serv_adr));

            printf("SEQ: %d\n", clnt_pkt.seq);
        }
        num=seq_check;
        num++;          

        read_cnt = clnt_pkt.data_len;

        fwrite((void*)clnt_pkt.data, 1, read_cnt, fp);
        total+=read_cnt;
    }
    fclose(fp);


    // close()
    close(sock);
    return 0;
}

void error_handling(char *msg){
    printf("%s error\n", msg);
    exit(1);
}