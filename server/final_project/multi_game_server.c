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
#include <pthread.h>
#include <netinet/in.h>

#define BUF_SIZE 1024
#define GROUP_IP "224.1.1.2"
#define TTL 64
#define MAX_CLNT 256

pthread_mutex_t mutx;

void error_handling(char *msg);
void *recv_data(void *arg);
void *send_data(void *arg);
void *handle_clnt(void *arg);

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

int *grid_size;
int *panel_pos;

// Game Info

typedef struct
{
    int player_id;
    int pos;
    int flag;
} client_data;

typedef struct
{
    int grid[10001];
    int same_cnt;
    double left_time;
    client_data clnt_data[MAX_CLNT];
} game_information;

// Game Over
typedef struct
{
    int socore; // winner 판단은 client가 계산 -> 이거는 구현하면서 서버, clien 둘 중 편한 부분에 구현 근데 서버에서 판단 하고 보내는 방식이 맞는듯
} game_result;

void *recv_data(void *arg);
void *send_data(void *arg);

struct sockaddr_in serv_adr, send_adr, recv_adr, clnt_adr;

client_init clnt_init;
game_result g_result;
game_information game_info;

int clnt_socks[MAX_CLNT];
int clnt_cnt = 0;
char start_msg[30] = "";
int test = 0;
int start = 0;

int main(int argc, char *argv[])
{
    int serv_sock;
    int t_serv_sock;
    int u_serv_sock;
    socklen_t clnt_adr_sz;

    struct ip_mreq join_adr;

    int time_live = TTL;

    struct timeval startTime, endTime;
    double total;

    pthread_mutex_init(&mutx, NULL);

    // ./server 2 8 16 30 1234
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

    srand((unsigned)time(NULL));

    int gird_num = atoi(argv[2]) * atoi(argv[2]);
    clnt_init.grid_num = atoi(argv[2]);

    clnt_init.player_cnt = atoi(argv[1]);
    clnt_init.player_id = 0;
    /* grid_size = (int *)malloc(sizeof(int) * gird_num);
    memset(grid_size, 0, gird_num * sizeof(int)); // -1로 초기화 */

    clnt_init.panel_cnt = atoi(argv[3]);
    panel_pos = (int *)malloc(sizeof(int) * clnt_init.panel_cnt);
    clnt_init.game_time = atoi(argv[4]);

    // game 상태 초기화
    memset(&game_info.grid, -1, sizeof(game_information));
    /* for (int i = 0; i < clnt_init.grid_num * clnt_init.grid_num; i++)
    {
        printf("%d, ", grid_size[i]);
    }
    printf("\n"); */

    // 난수로 panel 위치 할당
    int random = 0;
    int check = 0;
    for (int i = 0; i < clnt_init.panel_cnt; i++)
    {
        random = (rand() % gird_num);
        panel_pos[i] = random; // panel_pos에는 해당 숫자만 담김

        check=0;
        for (int j = 0; j < i; j++)
        {
            if (random == panel_pos[j])
            {
                check=1;
                i--;
            }
        }

        if(check) continue;

        if ((i % 2) == 0) // 빨간색
        {
            printf("Red check: %d\n", random);
            game_info.grid[random] = 0;
        }
        else if ((i % 2) == 1) // 파랑색
        {
            printf("Blue check: %d\n", random);
            game_info.grid[random] = 1;
        }
    }

    // 여기서 game info 값 설정 잘하면 됨.

    // game_info.grid = grid_size;
    /* memcpy(game_info.grid, grid_size, gird_num * sizeof(int));
    for (int i = 0; i < clnt_init.grid_num * clnt_init.grid_num; i++)
    {
        printf("%d, ",game_info.grid[i]);
    }
    printf("\n"); */

    // client init
    // clnt_data.flag = 0;

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

    // 시작을 의미하는 카운트 다운은 나중에 구현

    // 나중에 시간 설정해서 해당 시간 설정 끝나면 main thread 종료되게
    while (1)
    {
        // printf("test1: %d\n", test);
        sleep(2);
        if (test == 0)
        {
            printf("Input num 1: ");
            scanf("%d", &test);
        }
        // clnt_data.flag는 또다른 배열이나 로직을 만들어서 각 클라이언트로부터 받은 flag를 처리
        // if(clnt_data.flag == 9) break;

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
    total = startTime.tv_sec + (startTime.tv_usec / 1000000);
    // 시간 종료되면 종료
    int count = 0;
    while (1)
    {
        gettimeofday(&endTime, NULL);
        total = ((endTime.tv_sec - startTime.tv_sec) + ((endTime.tv_usec - startTime.tv_usec) / 1000000)*1.0);
        if (total > clnt_init.game_time)
        {
            start = 0;
            break;
        }
        game_info.left_time = total;
        // sleep(1); // 일정주기만큼 보낼것인가? while문마다 보낼 것인가?
        sendto(u_serv_sock, &game_info, sizeof(game_information), 0, (struct sockaddr *)&send_adr, sizeof(send_adr));
        usleep(30000);
    }

    // ptread 생성
    void *thread_return;

    // free(clnt_init.grid_size)

    // close(clnt_sock); // 각 클라이언트 수만큼 제거
    close(u_serv_sock);
    close(t_serv_sock);
    return 0;
}

void error_handling(char *msg)
{
    printf("%s error\n", msg);
    exit(1);
}

void *send_data(void *arg)
{
    int sock = *((int *)arg);
    int send_start = 1;
    char char_num[30];

    return NULL;
}

// 초기 게임 정보 보내는 스레드 함수 이후 클라이언트로 부터 중간 게임 받는 스레드로 전환 결과까지
void *handle_clnt(void *arg)
{
    thread_args args = *((thread_args *)arg);
    free(arg);

    int clnt_sock = args.sock;
    int player_id = args.player_id;

    int str_len = 0, i;

    client_init player_init;
    player_init.player_id = player_id;
    player_init.player_cnt = clnt_init.player_cnt;
    player_init.grid_num = clnt_init.grid_num;
    player_init.panel_cnt = clnt_init.panel_cnt;
    player_init.game_time = clnt_init.game_time;

    // 각 클라이언트로부터 각 스레드가 만들어지기 때문에, player_data를 만들어서 구분하고, 이 구분된 데이터를 합쳐서
    // clnt_data로 한번에 관리하기 때문에, clnt_data는 하나만 써서 각 클라이언트에게 보내주면 된다
    client_data player_data;
    player_data.flag = 0;

    // grid 동적할당을 위해서 사이즈 먼저 보내서 할당
    int grid_num = clnt_init.grid_num * clnt_init.grid_num;
    /* printf("grid_num: %d\n", grid_num);
    write(clnt_sock, &grid_num, sizeof(int));
    write(clnt_sock, grid_size, sizeof(int) * grid_num); */

    write(clnt_sock, &clnt_init.panel_cnt, sizeof(int));
    write(clnt_sock, panel_pos, sizeof(int) * clnt_init.panel_cnt);

    write(clnt_sock, &player_init, sizeof(client_init));

    while (1)
    {
        // printf("test2: %d\n", test);
        sleep(1);
        if (test)
        {
            write(clnt_sock, &start, sizeof(start)); // 예외처리같은거 x 무조건 어떤 시작값을 보내면 되기에.
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
                    game_info.grid[player_data.pos] = !game_info.grid[player_data.pos];
                ; // 반전 0->1, 1->0
            }
        }
        pthread_mutex_unlock(&mutx);
        // (mutex 필요)
        // printf("%d: recv_flag: %d\n", clnt_sock, player_data.flag);
    }
    return NULL;
}