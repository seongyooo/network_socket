#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>
#include <termios.h>
#include <fcntl.h>

#define BUF_SIZE 1024
#define SERV_IP "203.252.112.31"
#define MAX_CLNT 256

#define RED "\x1b[41m"
#define BLUE "\x1b[44m"
#define RESET "\x1b[0m"

#define P_RED "\x1b[31m"
#define P_BLUE "\x1b[34m"

pthread_mutex_t mutx;

void *recv_data(void *arg);
void *screen_data();
void *screen_print();
void error_handling(char *msg);

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

// client to server - Game Info
typedef struct
{
    int player_id;
    int pos;
    int flag;
} client_data;

typedef struct
{
    int grid[100 * 100 + 1];
    int same_cnt;
    int left_time;
    client_data clnt_data[MAX_CLNT];
} game_information;

client_init clnt_init;
client_data clnt_data;
game_information game_info;

int start;

int getch()
{
    int c = 0;
    struct termios oldattr, newattr;

    tcgetattr(STDIN_FILENO, &oldattr);
    newattr = oldattr;
    newattr.c_lflag &= ~(ICANON | ECHO);
    newattr.c_cc[VMIN] = 1;
    newattr.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &newattr);
    c = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldattr);

    return c;
}

int kbhit(void)
{
    struct termios oldt, newt;
    int ch = 0;
    int oldf = 0;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);

    ch = getch();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if (ch != EOF)
    {
        ungetc(ch, stdin);
        return 1;
    }

    return 0;
}

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
    clnt_data.player_id = clnt_init.player_id;
    printf("Grid size num: %d\n", clnt_init.grid_num);
    // printf("Panel pos: %d\n", clnt_init.panel_pos[0]);
    for (int i = 0; i < clnt_init.grid_num * clnt_init.grid_num; i++)
    {
        printf("%d, ", grid_size[i]);
    }
    printf("\n");
    printf("Time: %d\n", clnt_init.game_time);
    printf("-----------------------------\n");

    pthread_t send_thread, screen_thread, sceen_print_thread;
    void *thread_return;
    pthread_create(&screen_thread, NULL, screen_data, (void *)&t_sock);

    int count = 0;
    start = 0;
    // 정보를 계속 전달하는 main thread 아니면 보내는 스레드에서 정보를 바로 전달할 것인지
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

    pthread_create(&sceen_print_thread, NULL, screen_print, NULL);
    // 키보드 입력값 계속 받는 스레드

    game_information temp;
    int str_len;
    while (1)
    {
        str_len = recvfrom(u_sock, &temp, sizeof(game_information), 0, NULL, 0);

        if (str_len > 0)
        {
            pthread_mutex_lock(&mutx);
            memcpy(&game_info, &temp, sizeof(game_information));
            pthread_mutex_unlock(&mutx);
        }
        // printf("%d: recv_flag: %d\n", u_sock, g_data.flag);
    }
    // main threa에서 정보를 받고 터미널로 출력해주는 스레드가 값을 받도록 한다(전역변수로 하든, 인자로 넘겨주든)
    // main에서 받자마자 바로 출력해버릴까??

    pthread_join(send_thread, &thread_return);
    pthread_join(screen_thread, &thread_return);
    pthread_join(sceen_print_thread, &thread_return);

    close(t_sock);
    close(u_sock);
    return 0;
}

void error_handling(char *msg)
{
    printf("%s error\n", msg);
    exit(1);
}

void *screen_print()
{
    int check;
    printf("time: %d\n", game_info.left_time);
    while (1)
    {
        // printf("\033[H\033[J");
        usleep(50000);
        // 여기다가 터미널의 출력하는 로직을 만듬. 일단 데이터가 제대로 출력되는지 테스트
        pthread_mutex_lock(&mutx);

        printf("\n\n------------------------------\n\n");
        for (int i = 0; i < clnt_init.grid_num * clnt_init.grid_num; i++)
        {
            check = 1;
            for (int j = 0; j < clnt_init.player_cnt; j++)
            {
                if (i == game_info.clnt_data[j].pos)
                {
                    if(j%2) printf(P_RED);
                    else printf(P_BLUE);
    
                    printf("%d ", game_info.clnt_data[j].player_id);
                    printf(RESET);
                    check = 0;
                }
            }

            if (check)
            {
                printf("%d ", game_info.grid[i]);
            }

            if ((i + 1) % clnt_init.grid_num == 0)
                printf("\n");
        }
        pthread_mutex_unlock(&mutx);
    }
    return NULL;
}

void *screen_data(void *arg)
{
    int sock = *((int *)arg);
    int ch;
    int standard = clnt_init.grid_num;
    // ch = getch();

    clnt_data.pos = 0;
    while (1)
    {
        if (kbhit())
        {
            ch = getch();

            pthread_mutex_lock(&mutx);
            if (ch == '[')
            {
                ch = getch();
                if (ch == 'A')
                {
                    // printf("up\n");
                    if (clnt_data.pos - standard >= 0)
                    {
                        clnt_data.pos -= standard;
                    }
                }
                else if (ch == 'C')
                {
                    // printf("right\n");
                    if ((clnt_data.pos + 1) % standard != 0)
                    {
                        clnt_data.pos++;
                    }
                }
                else if (ch == 'B')
                {
                    // printf("down\n");
                    if (clnt_data.pos + standard < standard * standard)
                    {
                        clnt_data.pos += standard;
                    }
                }
                else if (ch == 'D')
                {
                    // printf("left\n");
                    if ((clnt_data.pos) % standard != 0)
                    {
                        clnt_data.pos--;
                    }
                }
                else if (ch == '\r')
                {
                    // printf("Enter\n");
                    clnt_data.flag = 5;
                }
                else
                {
                    // printf("Nono\n");
                }
            }
            pthread_mutex_unlock(&mutx);
            write(sock, &clnt_data, sizeof(client_data));

            if (clnt_data.flag == 5)
            {
                clnt_data.flag = 0;
            }
        }
        usleep(10000);
    }
    return NULL;
}
