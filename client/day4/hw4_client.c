#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <pthread.h>

#define BUF_SIZE 1024
#define NAME_SIZE 30
#define DATA_SIZE 1024

#define GREEN "\x1b[32m"
#define YELLOW "\x1b[33m"
#define RESET "\x1b[0m"


pthread_mutex_t display_mutex;

void gotoxy(int x, int y)
{
    printf("%c[%d;%df", 0x1B, y, x);
}

int getch()
{
    int c = 0;
    struct termios oldattr, newattr;

    tcgetattr(STDIN_FILENO, &oldattr);
    newattr = oldattr;
    newattr.c_lflag &= ~(ICANON);
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

void *recv_data(void *arg);
void *send_msg(void *arg);
void error_handling(char *msg);

char msg[NAME_SIZE];
int len;
char name_msg[NAME_SIZE + BUF_SIZE] = "";

typedef struct
{
    char data_name[BUF_SIZE];
    int data_size;
} data_info;

int main(int argc, char *argv[])
{
    int sock;
    struct sockaddr_in serv_addr;

    pthread_mutex_init(&display_mutex, NULL);

    void *thread_return;

    // ./client 203.252.112.31 1234
    if (argc != 3)
    {
        printf("Usage %s <IP> <port> \n", argv[0]);
        exit(1);
    }

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        error_handling("socket() error");
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    {
        error_handling("connect() error");
    }

    pthread_t snd_thread, rcv_thread;

    printf("\033[2J"); // 화면을 지우고 커서를 왼쪽 상단 모서리(0행, 0열)로 이동
    gotoxy(1, 1);
    printf(GREEN);
    printf("Search word: ");
    printf(RESET);
    gotoxy(1, 2);
    printf("--------------------------\n");
    fflush(stdout);

    pthread_create(&snd_thread, NULL, send_msg, (void *)&sock);
    pthread_create(&rcv_thread, NULL, recv_data, (void *)&sock);
    pthread_join(snd_thread, &thread_return);
    pthread_join(rcv_thread, &thread_return);
    close(sock);
}

void error_handling(char *msg)
{
    printf("%s\n", msg);
    exit(1);
}

void *send_msg(void *arg)
{
    int sock = *((int *)arg);
    int str_len;
    int m;
    while (1)
    {
        pthread_mutex_lock(&display_mutex);
        gotoxy(13 + len, 1);
        fflush(stdout);
        pthread_mutex_unlock(&display_mutex);

        m = getch();

        if (m == 127)
        {
            if (len < 1)
            {
                name_msg[1] = '\0';
                for (int i = 0; i < 10; i++)
                {
                    gotoxy(1, 3 + i);
                    printf("\033[2K");
                    fflush(stdout);
                }
                continue;
            }
            len--;
            name_msg[len] = '\0';

            pthread_mutex_lock(&display_mutex);
            gotoxy(13 + len, 1);
            printf(" ");
            fflush(stdout);
            gotoxy(13 + len, 1);
            fflush(stdout);
            pthread_mutex_unlock(&display_mutex);
        }
        else
        {
            name_msg[len++] = m;
            name_msg[len] = '\0';

            pthread_mutex_lock(&display_mutex);
            gotoxy(13, 1);
            printf("%s", name_msg);
            fflush(stdout);
            pthread_mutex_unlock(&display_mutex);
        }

        if (!strcmp(name_msg, "q\n"))
        {
            close(sock);
            break;
        }

        str_len = strlen(name_msg);
        write(sock, &str_len, sizeof(int));
        write(sock, name_msg, strlen(name_msg));
    }
    return NULL;
}

void *recv_data(void *arg)
{

    int sock = *((int *)arg);
    char data_msg[DATA_SIZE + BUF_SIZE];
    int str_len;
    int count;
    char *ptr;
    int index;

    char list[BUF_SIZE];
    while (1)
    {
        read(sock, &count, sizeof(int));

        pthread_mutex_lock(&display_mutex);
        for (int i = 0; i < 10; i++)
        {
            gotoxy(1, 3 + i);
            printf("\033[2K"); //	커서 위치에서 현재 줄의 끝까지 지우기
            fflush(stdout);
        }

        int total = 0;
        for (int i = 0; i < count; i++)
        {

            read(sock, &str_len, sizeof(int));
            read(sock, list, str_len);

            list[str_len] = '\0';

            gotoxy(1, 3 + i);

            int j;

            if ((ptr = strstr(list, name_msg)) != NULL)
            {
                index = ptr - list;

                for (int k = 0; k < index; k++)
                {
                    printf("%c", list[k]);
                }

                for (int k = index; k < index + strlen(name_msg); k++)
                {
                    printf(YELLOW);
                    printf("%c", list[k]);
                    printf(RESET);
                }

                for (int k = index + strlen(name_msg); k < str_len; k++)
                {
                    printf("%c", list[k]);
                }
            }
        }
        // gotoxy(13, 1);
        fflush(stdout);
        pthread_mutex_unlock(&display_mutex);
    }
    return NULL;
}