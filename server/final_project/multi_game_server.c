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

// Initialize
typedef struct{
    // int flag; // 현재 보내진 데이터 패킷이 어떤 데이터인지 flag가 정확히 뭐를 의미하는지?

    int player_cnt;
    int player_id;

    int *grid_size;
    int panel_cnt;
    int panel_pos[BUF_SIZE]; // panel_pos[panel_num] (실제 판의 위치에 대한 정보) num은 어떻게 할당할 것인지?
    int game_time;
    
}client_init;

// Game Info
typedef struct{
    int flag; // 데이터가 어떤 event를 발생시켰는지 판단(이동, 뒤집기)
    int panel_data; // 판의 대한 정보만 보내도 된다. 이미 초기값을 통해, 전체 grid 정보는 넘어갔으니
    int player_pos[BUF_SIZE];
    int left_time;
}game_data;

// Game Over
typedef struct{
    int socore; // winner 판단은 client가 계산 -> 이거는 구현하면서 서버, clien 둘 중 편한 부분에 구현 
}game_result;


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