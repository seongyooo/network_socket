#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <fcntl.h>
#include <pthread.h>
#include <netinet/in.h>

#define BUF_SIZE 1024
#define GROUP_IP "224.1.1.2"
#define TTL 64
#define MAX 300
#define GRID_SIZE 10001

typedef struct
{
    int sock;
    int player_id;
} thread_args;

// Initialize
typedef struct
{
    int player_cnt;
    int player_id;
    int grid_num;
    int panel_cnt;
    int game_time;

} client_init;

// Game Info
typedef struct
{
    int player_id;
    int pos;
    int flag;
} client_data;

typedef struct
{
    int grid[GRID_SIZE];
    int same_cnt;
    double left_time;
    client_data clnt_data[MAX];
} game_information;

client_init clnt_init;
game_information game_info;

int *panel_pos;

void error_handling(char *msg);
void *recv_data(void *arg);
void *handle_clnt(void *arg);

pthread_mutex_t mutx;
struct sockaddr_in serv_adr, send_adr, clnt_adr;

int clnt_socks[MAX];
int clnt_cnt = 0;
int test = 0;
int start = 0;

int main(int argc, char *argv[])
{
    int t_serv_sock, u_serv_sock;
    socklen_t clnt_adr_sz;
    struct ip_mreq join_adr;
    int time_live = TTL;
    struct timeval startTime, endTime;
    double total;

    pthread_mutex_init(&mutx, NULL);

    // ./server 4 8 16 30 1234
    if (argc != 6)
    {
        printf("Usage: %s <player> <grid size> <panel> <time> <port>\n", argv[0]);
        exit(1);
    }

    // tcp socket 생성
    t_serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (t_serv_sock == -1)
    {
        error_handling("socket1");
    }

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[5]));
    if (bind(t_serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
    {
        error_handling("bind()1");
    }

    // listen()
    if (listen(t_serv_sock, 5) == -1)
    {
        error_handling("listen() error");
    }

    // cmd 인자로 초기값 세팅
    srand((unsigned)time(NULL));
    int gird_num = atoi(argv[2]) * atoi(argv[2]);
    clnt_init.grid_num = atoi(argv[2]);

    clnt_init.player_cnt = atoi(argv[1]);
    clnt_init.player_id = 0;

    clnt_init.panel_cnt = atoi(argv[3]);
    panel_pos = (int *)malloc(sizeof(int) * clnt_init.panel_cnt);
    clnt_init.game_time = atoi(argv[4]);

    // game 상태 초기화
    memset(&game_info.grid, -1, sizeof(game_information));

    // 난수로 panel 위치 할당
    int random = 0;
    int check = 0;
    for (int i = 0; i < clnt_init.panel_cnt; i++)
    {
        random = (rand() % gird_num);
        panel_pos[i] = random; // panel_pos에는 해당 숫자만 담김

        check = 0;
        for (int j = 0; j < i; j++)
        {
            if (random == panel_pos[j])
            {
                check = 1;
                i--;
            }
        }

        if (check)
            continue;

        if ((i % 2) == 0) // 빨간색
        {
            game_info.grid[random] = 0;
        }
        else if ((i % 2) == 1) // 파랑색
        {
            game_info.grid[random] = 1;
        }
    }

    pthread_t t_id, send_thread;

    start = 1;
    while (1)
    {
        thread_args *args = (thread_args *)malloc(sizeof(thread_args));

        clnt_adr_sz = sizeof(clnt_adr);
        args->sock = accept(t_serv_sock, (struct sockaddr *)&clnt_adr, &clnt_adr_sz);

        pthread_mutex_lock(&mutx);
        clnt_socks[clnt_cnt++] = args->sock;
        args->player_id = clnt_cnt;
        pthread_mutex_unlock(&mutx);

        pthread_create(&t_id, NULL, handle_clnt, (void *)args);

        pthread_detach(t_id);
        printf("Connected client: %d \n", args->sock);

        if (clnt_cnt == clnt_init.player_cnt)
            break;
    }
    close(t_serv_sock);

    while (1)
    {
        if (test == 0)
        {
            printf("Input num 1: ");
            scanf("%d", &test);
        }

        if (test == 1)
            break;
    }

    // udp socket 생성
    u_serv_sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (u_serv_sock == -1)
    {
        error_handling("socket1");
    }

    // 멀티캐스트를 위한 초기화
    memset(&send_adr, 0, sizeof(send_adr));
    send_adr.sin_family = AF_INET;
    send_adr.sin_addr.s_addr = inet_addr(GROUP_IP);
    send_adr.sin_port = htons(atoi(argv[5]));

    setsockopt(u_serv_sock, IPPROTO_IP, IP_MULTICAST_TTL, (void *)&time_live, sizeof(time_live));

    gettimeofday(&startTime, NULL);
    total=0;
    // 시간 종료되면 종료
    int count = 0;
    while (1)
    {
        gettimeofday(&endTime, NULL);
        total = ((endTime.tv_sec - startTime.tv_sec) + ((endTime.tv_usec - startTime.tv_usec) / 1000000) * 1.0);
        if (total > clnt_init.game_time)
        {
            start = 0;
            break;
        }
        game_info.left_time = total;
        sendto(u_serv_sock, &game_info, sizeof(game_information), 0, (struct sockaddr *)&send_adr, sizeof(send_adr));
        usleep(30000); // 0.03
    }

    free(panel_pos);
    close(u_serv_sock);
    close(t_serv_sock);
    return 0;
}

void error_handling(char *msg)
{
    printf("%s error\n", msg);
    exit(1);
}

// 초기 게임 정보 보내는 스레드 함수 이후 클라이언트로 부터 중간 게임 받는 스레드로 전환 결과까지
void *handle_clnt(void *arg)
{
    thread_args args = *((thread_args *)arg);
    free(arg);

    int clnt_sock = args.sock;
    int player_id = args.player_id;

    int str_len = 0;

    client_init player_init;
    player_init.player_id = player_id;
    player_init.player_cnt = clnt_init.player_cnt;
    player_init.grid_num = clnt_init.grid_num;
    player_init.panel_cnt = clnt_init.panel_cnt;
    player_init.game_time = clnt_init.game_time;

    client_data player_data;
    player_data.flag = 0;

    write(clnt_sock, &clnt_init.panel_cnt, sizeof(int));
    write(clnt_sock, panel_pos, sizeof(int) * clnt_init.panel_cnt);

    write(clnt_sock, &player_init, sizeof(client_init));

    while (1)
    {
        if (test)
        {
            write(clnt_sock, &start, sizeof(start));
            break;
        }
    }

    while (start)
    {
        str_len = read(clnt_sock, &player_data, sizeof(client_data));

        if (str_len <= 0)
            break;

        if (player_data.player_id < 1)
        {
            error_handling("index error");
        }
        pthread_mutex_lock(&mutx);
        game_info.clnt_data[player_data.player_id - 1].player_id = player_data.player_id;
        game_info.clnt_data[player_data.player_id - 1].pos = player_data.pos;

        if (player_data.flag == 5)
        {
            for (int i = 0; i < clnt_init.panel_cnt; i++)
            {
                if (panel_pos[i] == player_data.pos)
                    game_info.grid[player_data.pos] = !game_info.grid[player_data.pos]; // 반전 0->1, 1->0
            }
        }
        pthread_mutex_unlock(&mutx);
    }
    
    close(clnt_sock);
    return NULL;
}