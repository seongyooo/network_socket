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

// Game Info
typedef struct
{
    int flag;       // 데이터가 어떤 event를 발생시켰는지 판단(이동, 뒤집기)
    int panel_data; // 판의 대한 정보만 보내도 된다. 이미 초기값을 통해, 전체 grid 정보는 넘어갔으니
    int player_pos[BUF_SIZE];
    int left_time;
} game_data;

typedef struct
{
    int pos;
    int flag;
} client_data;

// Game Over
typedef struct
{
    int socore; // winner 판단은 client가 계산 -> 이거는 구현하면서 서버, clien 둘 중 편한 부분에 구현
} game_result;

void *recv_data(void *arg);
void *send_data(void *arg);

struct sockaddr_in serv_adr, send_adr, recv_adr, clnt_adr;

client_init clnt_init;
game_data g_data;
client_data clnt_data;
game_result g_result;

int clnt_socks[MAX_CLNT];
int clnt_cnt = 0;
char start_msg[30] = "";
int test = 0;

int main(int argc, char *argv[])
{
    int serv_sock, clnt_sock;
    int t_serv_sock;
    int u_serv_sock;
    socklen_t clnt_adr_sz;

    struct ip_mreq join_adr;

    int time_live = TTL;

    pthread_mutex_init(&mutx, NULL);

    // ./server 2 4 4 30 1234
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

    // serv_adr은 같은 변수 써도 될듯 memset으로 초기화를 해서.
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

    // init 데이터 처리
    // 나중에 grid 배열 0,1 값 난수로 넣기
    // 세팅을 한번에 해서 보내는게 좋을 것 같음.
    srand((unsigned)time(NULL));

    int gird_num = atoi(argv[2]) * atoi(argv[2]);
    clnt_init.grid_num = atoi(argv[2]);

    clnt_init.player_cnt = atoi(argv[1]);
    clnt_init.player_id = 0;
    grid_size = (int *)malloc(sizeof(int) * gird_num);
    memset(grid_size, -1, gird_num * sizeof(int)); // -1로 초기화
    clnt_init.panel_cnt = atoi(argv[3]);
    panel_pos = (int *)malloc(sizeof(int) * clnt_init.panel_cnt);
    clnt_init.game_time = atoi(argv[4]);

    // 난수로 panel 위치 할당
    int random = 0;
    for (int i = 0; i < clnt_init.panel_cnt; i++)
    { 
        random = (rand() % gird_num);
        panel_pos[i] = random; // panel_pos에는 해당 숫자만 담김

        for (int j = 0; j < i; j++)
        {
            if (panel_pos[i] == panel_pos[j])
                i--;
        }

        if ((i % 2) == 0) // 빨간색
        {
            printf("Red check: %d\n", random);
            grid_size[random] = 0;
        }
        else if((i%2) == 1) // 파랑색
        {
            printf("Blue check: %d\n", random);
            grid_size[random] = 1;
        }
    }

    for(int i=0; i<clnt_init.grid_num * clnt_init.grid_num; i++){
        printf("%d, ", grid_size[i]);
    }
    printf("\n");

    // client init
    clnt_data.flag = 0;

    pthread_t t_id, send_thread;

    // 여기서 스레드를 만드는 것이 옳은가? 아니면 tcp로 각 클라이언트에게 초기정보만 주고, udp에서 스레드를 만드는 것이 나은가?
    while (1)
    {
        clnt_adr_sz = sizeof(clnt_sock);
        clnt_sock = accept(t_serv_sock, (struct sockaddr *)&clnt_adr, &clnt_adr_sz);

        pthread_mutex_lock(&mutx);
        clnt_socks[clnt_cnt++] = clnt_sock;
        clnt_init.player_id = clnt_cnt;
        pthread_mutex_unlock(&mutx);

        // 스레드 만들자마자 바로 init를 보내기 때문에 그 전에 처리해서 보내야 한다.
        pthread_create(&t_id, NULL, handle_clnt, (void *)&clnt_sock);
        printf("Connected client: %d \n", clnt_sock);

        if (clnt_cnt == clnt_init.player_cnt)
            break;
    }
    close(t_serv_sock);
    // 현재 각 클라이언트 스레드는 계속 돌아가고 있는 상황.
    // 이제 udp soket을 만들어서 서버에서 각 클라이언트에 event를 받고 처리해서,
    // 각 클라이언트들에게 보내주는 send()를 구현 하면 됨. -> 스레드로.
    // pthread_join 이나 detach는 나중에 종료 소켓 만들기 전에 구현

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

    // 그냥 main에서 보내기?
    /* typedef struct
    {
        int flag;       // 데이터가 어떤 event를 발생시켰는지 판단(이동, 뒤집기)
        int panel_data; // 판의 대한 정보만 보내도 된다. 이미 초기값을 통해, 전체 grid 정보는 넘어갔으니
        int player_pos[BUF_SIZE];
        int left_time;
    } game_data; */

    // 시간 종료되면 종료
    int count = 0;
    while (1)
    {
        g_data.flag = count++;
        sleep(1); // 일정주기만큼 보낼것인가? while문마다 보낼 것인가?
        // printf("%d: send_flag: %d\n", u_serv_sock, clnt_data.flag);
        sendto(u_serv_sock, &g_data, sizeof(game_data), 0, (struct sockaddr *)&send_adr, sizeof(send_adr));
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

/* void *recv_data(void *arg)
{
    int sock = *((int *)arg); // 이게 udp 소켓이여야 함.

    int serv_start = 0;
    // 받는 거는 그냥 누구로부터 받았는지만 알면되니깐, 그거는 스레드가 나눠져 있어서 알 수 있지 않나?
    while (1)
    {
        recvfrom(sock, &serv_start, sizeof(int), 0, NULL, 0);
        if (serv_start)
            break;
    }
    return NULL;
} */

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
    int clnt_sock = *((int *)arg);
    int str_len = 0, i;
    int start = 1;

    // 각 클라이언트로부터 각 스레드가 만들어지기 때문에, player_data를 만들어서 구분하고, 이 구분된 데이터를 합쳐서
    // clnt_data로 한번에 관리하기 때문에, clnt_data는 하나만 써서 각 클라이언트에게 보내주면 된다
    client_data player_data;
    player_data.flag = 0;

    // grid 동적할당을 위해서 사이즈 먼저 보내서 할당
    int grid_num =clnt_init.grid_num*clnt_init.grid_num;
    printf("grid_num: %d\n", grid_num);
    write(clnt_sock, &grid_num, sizeof(int));
    write(clnt_sock, grid_size, sizeof(int)*grid_num);

    write(clnt_sock, &clnt_init, sizeof(client_init));

    while (1)
    {
        // printf("test2: %d\n", test);
        sleep(2);
        if (test)
        {
            write(clnt_sock, &start, sizeof(start)); // 예외처리같은거 x 무조건 어떤 시작값을 보내면 되기에.
            break;
        }
    }

    while (1)
    {
        str_len = read(clnt_sock, &player_data, sizeof(client_data));

        if (str_len <= 0)
            break;

        clnt_data = player_data; // 이런식으로 받은 데이터를 처리해서 clnt_data에 저장(mutex 필요)
        // printf("%d: recv_flag: %d\n", clnt_sock, player_data.flag);
    }

    // 이 스레드를 종료시키지 말고 각 클라이언트로부터 데이터를 받아들이는 스레드 함수로 발전시키면 어떨까?
    // 즉, 게임 중반 데이터들을 계속 받는 스레드로 전환, 아니면 이 스레드는 tcp니깐, 초기정보 + 종료정보만 포함
    // 각 클라이언트로부터 계속 값을 받는거임.

    // 근데 여기에는 tcp로만 했던 sock 정보만 있으므로 udp 소켓은 따로 함수를 만들어서 관리해야함
    // 그럼 클라이언트 측에서 보낸 udp 패킷이 어떤 스레드 패킷인지는 어떻게 아느냐?
    // if(player_id)

    // recvfrom() 하고 그 구조체 데이터를 여기서 바로 업데이트 시키면, 전역 구조체에서 값이 변경되서
    // sendto()로 보낼 수 있나? -> 뮤텍스로 보호!

    return NULL;
}