#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>

#define BUF_SIZE 1024
#define SERV_IP "203.252.112.31"

pthread_mutex_t mutx;

void *recv_data(void *arg);
void *send_data(void *arg);
void error_handling(char *msg);

// Initialize
typedef struct
{
    // int flag; // 현재 보내진 데이터 패킷이 어떤 데이터인지 flag가 정확히 뭐를 의미하는지?

    int player_cnt;
    int player_id;
    int grid_num;
    int panel_cnt;
    int game_time;

} client_init;

int *grid_size;
int *panel_pos; // panel_pos[panel_num] (실제 판의 위치에 대한 정보) num은 어떻게 할당할 것인지?

// client to server - Game Info
typedef struct
{
    int pos;
    int flag;
} client_data;

typedef struct
{
    int flag;       // 데이터가 어떤 event를 발생시켰는지 판단(이동, 뒤집기)
    int panel_data; // 판의 대한 정보만 보내도 된다. 이미 초기값을 통해, 전체 grid 정보는 넘어갔으니
    int player_pos[BUF_SIZE];
    int left_time;
} game_data;

client_init clnt_init;
client_data clnt_data;
game_data g_data;

int start;

int main(int argc, char *argv[])
{
    int u_sock;
    int t_sock;
    socklen_t adr_sz;

    struct sockaddr_in serv_adr, from_adr;
    struct ip_mreq join_adr;

    pthread_mutex_init(&mutx, NULL);

    // ./client 224.1.1.2 1234
    if (argc != 3)
    {
        printf("Usage : %s <GROUP_IP> <port>\n", argv[0]);
        exit(1);
    }

    // tcp로 소켓 생성
    t_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (t_sock == -1)
    {
        error_handling("socket() error");
    }

    // client init
    clnt_data.flag = 0;

    // sever로 데이터 보내기 위한 것(이것도 마찬가지 tcp, udp 둘다 같이 쓸 수 있을듯)
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = inet_addr(SERV_IP);
    serv_adr.sin_port = htons(atoi(argv[2]));

    if (connect(t_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
    {
        error_handling("connect() error");
    }


    // 초기정보 받기
    int grid_num;
    read(t_sock, &grid_num, sizeof(int));
    grid_size = (int *)malloc(sizeof(int) * grid_num);

    read(t_sock, grid_size, sizeof(int) * grid_num);

    read(t_sock, &clnt_init, sizeof(client_init));

    printf("-----------------------------\n");
    printf("Player count: %d\n", clnt_init.player_cnt);
    printf("Game init ID: %d\n", clnt_init.player_id);
    printf("Grid size num: %d\n", clnt_init.grid_num);
    // printf("Panel pos: %d\n", clnt_init.panel_pos[0]);
    for(int i=0; i<clnt_init.grid_num * clnt_init.grid_num; i++){
        printf("%d, ", grid_size[i]);
    }
    printf("\n");
    printf("Time: %d\n", clnt_init.game_time);
    printf("-----------------------------\n");

    pthread_t send_thread, recv_thread;
    void *thread_return;
    pthread_create(&send_thread, NULL, send_data, (void *)&t_sock);

    int count = 0;
    start = 0;
    // 정보를 계속 전달하는 main thread 아니면 보내는 스레드에서 정보를 바로 전달할 것인지
    // start가 1이 들어오면, send_data 스레드에서 flag값 보내기 시작함.(현재는 test용으로 10개만)
    read(t_sock, &start, sizeof(int));
    printf("start: %d\n", start);

    // udp socket()
    u_sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (u_sock == -1)
        error_handling("socket() error");

    int optvalue = 1;
    if (setsockopt(u_sock, SOL_SOCKET, SO_REUSEADDR, &optvalue, sizeof(optvalue)) < 0)
    {
        error_handling("reuse");
    }

    memset(&from_adr, 0, sizeof(from_adr));
    from_adr.sin_family = AF_INET;
    from_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    from_adr.sin_port = htons(atoi(argv[2]));

    if (bind(u_sock, (struct sockaddr *)&from_adr, sizeof(from_adr)) == -1)
    {
        error_handling("bind()2");
    }

    join_adr.imr_multiaddr.s_addr = inet_addr(argv[1]);
    join_adr.imr_interface.s_addr = htonl(INADDR_ANY);

    setsockopt(u_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void *)&join_adr, sizeof(join_adr));

    while(1){
        recvfrom(u_sock, &g_data, sizeof(game_data), 0, NULL, 0);
        // printf("%d: recv_flag: %d\n", u_sock, g_data.flag);
    }
    // main threa에서 정보를 받고 터미널로 출력해주는 스레드가 값을 받도록 한다(전역변수로 하든, 인자로 넘겨주든)
    // main에서 받자마자 바로 출력해버릴까??

    pthread_join(send_thread, &thread_return);

    close(t_sock);
    close(u_sock);
    return 0;
}

void error_handling(char *msg)
{
    printf("%s error\n", msg);
    exit(1);
}

void *recv_data(void *arg)
{
    int sock = *((int *)arg);

    /* read(sock, &clnt_init, sizeof(client_init));

    printf("game init ID: %d\n", clnt_init.player_id);
 */
    // 이후는 이제 게임 중반 데이터들을 계속 받는 스레드로 전환

    return NULL;
}

void *send_data(void *arg)
{
    int sock = *((int *)arg);

    // 계속 보내는 거를 어떻게 구현을 할지 나중에는 저 while문을 time으로 설정.

    while (1)
    {
        if (start)
        {
            sleep(1);
            pthread_mutex_lock(&mutx);
            clnt_data.flag = g_data.flag;
            write(sock, &clnt_data, sizeof(client_data));
            // printf("%d: send_flag: %d\n", sock, clnt_data.flag);
            pthread_mutex_unlock(&mutx);
        }
    }

    return NULL;
}
