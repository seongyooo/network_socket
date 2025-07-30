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
#define MAX 300
#define GRID_SIZE 10001

#define RED "\x1b[41m"
#define BLUE "\x1b[44m"
#define RESET "\x1b[0m"
#define WHITE "\x1b[47m"

#define P_RED "\x1b[31m"
#define P_BLUE "\x1b[34m"

// Initialize
typedef struct
{
    int player_cnt;
    int player_id;
    int grid_num;
    int panel_cnt;
    int game_time;

} client_init;

// client to server - Game Info
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
client_data clnt_data;
game_information game_info;

int *panel_pos;

void *recv_data(void *arg);
void *screen_data(void *arg);
void *screen_print();
void error_handling(char *msg);

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

pthread_mutex_t mutx;

int start;

int main(int argc, char *argv[])
{
    int u_sock, t_sock;
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

    int panel_num;
    read(t_sock, &panel_num, sizeof(int));
    panel_pos = (int *)malloc(sizeof(int) * panel_num);
    read(t_sock, panel_pos, sizeof(int) * panel_num);

    read(t_sock, &clnt_init, sizeof(client_init));

    printf("-----------------------------\n");
    printf("Player count: %d\n", clnt_init.player_cnt);
    printf("Game ID: %d\n", clnt_init.player_id);
    clnt_data.player_id = clnt_init.player_id;
    printf("Grid size: %d\n", clnt_init.grid_num);
    printf("Panel count: %d\n", clnt_init.grid_num);
    printf("Time: %d\n", clnt_init.game_time);
    printf("-----------------------------\n");

    int count = 0;
    start = 0;
    read(t_sock, &start, sizeof(int));
    printf("start: %d\n", start);

    /* count=5;
    while(1){
        printf("Count: %d\n", count--);
        sleep(1);
        if(count == 0) break;
    } */

    pthread_t screen_thread, sceen_print_thread;
    void *thread_return;
    pthread_create(&screen_thread, NULL, screen_data, (void *)&t_sock);

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

    game_information temp;
    int str_len;
    while (start)
    {
        str_len = recvfrom(u_sock, &temp, sizeof(game_information), 0, NULL, 0);

        if (game_info.left_time >= clnt_init.game_time)
        {
            start = 0;
            break;
        }

        if (str_len > 0)
        {
            pthread_mutex_lock(&mutx);
            memcpy(&game_info, &temp, sizeof(game_information));
            pthread_mutex_unlock(&mutx);
        }
    }

    pthread_join(screen_thread, &thread_return);
    pthread_join(sceen_print_thread, &thread_return);

    int result = 0;
    for (int i = 0; i < clnt_init.panel_cnt; i++)
    {
        if (game_info.grid[panel_pos[i]] == 0)
            result++;
    }

    if ((result - clnt_init.panel_cnt / 2) == 0)
    {
        printf("Draw!\n");
    }
    else if ((result - clnt_init.panel_cnt / 2) > 0)
    {
        printf("Winner: Red!\n");
    }
    else
    {
        printf("Winner: Blue!\n");
    }

    free(panel_pos);
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
    int same_k;
    int red_cnt = clnt_init.panel_cnt / 2;
    int blue_cnt = clnt_init.panel_cnt / 2;

    int same[clnt_init.player_cnt];
    int index;
    int same_cnt[clnt_init.grid_num * clnt_init.grid_num];
    while (start)
    {
        printf("\033[H\033[J");
        usleep(7000);
        // usleep(100000);
        // usleep(1000);
        // sleep(1);
        pthread_mutex_lock(&mutx);
        printf("Time: %f\n", (double)clnt_init.game_time - game_info.left_time);
        printf("Red: %d  vs  Blue: %d", red_cnt, blue_cnt);
        printf("\n------------------------------\n\n");

        // for문으로 출력하기에, grid size가 커지면 화면 깜박임이 심해짐. sprintf()로 한번에 입력받았다가 출력?
        for (int i = 0; i < clnt_init.grid_num * clnt_init.grid_num; i++)
        {
            check = 1;
            same_k = -1;

            for (int j = 0; j < clnt_init.player_cnt; j++)
            {
                same[j] = -1;
            }
            index = 0;
            for (int j = 0; j < clnt_init.player_cnt; j++)
            {
                // 플레이어끼리 겹쳤을때
                int test = 0;
                if (i == game_info.clnt_data[j].pos)
                {
                    for (int k = 0; k < index; k++)
                        if (j == same[k])
                        {
                            test = 1;
                            break;
                        }

                    if (test)
                        continue;

                    for (int k = j + 1; k < clnt_init.player_cnt; k++)
                    {
                        if (i == game_info.clnt_data[k].pos)
                        {
                            same[index++] = k;
                        }
                    }

                    if (j % 2)
                    {
                        printf(P_RED);
                    }
                    else
                    {
                        printf(P_BLUE);
                    }

                    if (game_info.grid[i] == 0)
                    {
                        printf(RED);

                        printf(" %d ", game_info.clnt_data[j].player_id);
                        printf(RESET);
                    }
                    else if (game_info.grid[i] == 1)
                    {
                        printf(BLUE);

                        printf(" %d ", game_info.clnt_data[j].player_id);
                        printf(RESET);
                    }
                    else
                    {
                        printf(WHITE);

                        printf(" %d ", game_info.clnt_data[j].player_id);
                        printf(RESET);
                    }

                    check = 0;
                }
            }

            if (check)
            {
                if (game_info.grid[i] == -1)
                { // 배경 흰색
                    printf(WHITE);
                    printf("   ");
                    printf(RESET);
                }
                else if (game_info.grid[i] == 0)
                {
                    printf(RED);
                    printf("   ");
                    printf(RESET);
                }
                else if (game_info.grid[i] == 1)
                {
                    printf(BLUE);
                    printf("   ");
                    printf(RESET);
                }
            }

            red_cnt = 0;
            blue_cnt = clnt_init.panel_cnt;
            for (int i = 0; i < clnt_init.grid_num * clnt_init.grid_num; i++)
            {
                if (game_info.grid[i] == 0)
                {
                    red_cnt++;
                    blue_cnt--;
                }
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

    clnt_data.pos = 0;
    // 게임 시작 전에 버퍼에 남아 있는 문자열로 인해서 플레이어가 미리 움직이는 예외 상황 처리

    /* while (1)
    {
        ch = getch();
        if (start == 1 && ch == NULL)
        {
            break;
        }
    } */

    while (start)
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
                else
                {
                    // printf("Nono\n");
                }
            }

            if (ch == '\n')
            {
                // printf("Enter\n");
                clnt_data.flag = 5;
            }
            pthread_mutex_unlock(&mutx);
            write(sock, &clnt_data, sizeof(client_data));

            if (clnt_data.flag == 5)
            {
                clnt_data.flag = 0;
            }
        }
        // usleep(10000);
    }
    return NULL;
}
