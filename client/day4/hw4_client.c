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

void gotoxy(int x, int y)
{
    printf("%c[%d;%df", 0x1B, y, x);
}

void clear_lines(int start_line, int count)
{
    for(int i = 0; i < count; i++) {
        gotoxy(1, start_line + i);
        printf("\033[K"); // 현재 커서 위치부터 줄 끝까지 지우기
    }
    fflush(stdout);
}

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

void *recv_data(void *arg);
void *send_msg(void *arg);
void error_handling(char *msg);

char msg[NAME_SIZE];

typedef struct
{
    char data_name[BUF_SIZE];
    int data_size;
} data_info;

int main(int argc, char *argv[])
{
    /* int i;
    char msg[BUF_SIZE];

    // 엔터를 코드상에서 입력하게 해서, 계속 msg 값을 터미널 상에서 출력하도록 하는 것.
    // 아니면 터미널에서 엔터 입력 없이도, 문자열만 쳐도, 계속 저장이 되도록
    fputs("Search word: ", stdout);
    while(1){
        fgets(msg, BUF_SIZE, stdin);
        msg[strlen(msg)]='\n';
        printf ("\x1B[%ld;%dH", strlen(msg)+15, -1);
        fflush(stdout);
        sleep(1);
    } */

    int sock;
    struct sockaddr_in serv_addr;

    void *thread_return;

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
    /* while (1)
    {
        pthread_create(&snd_thread, NULL, send_msg, (void *)&sock);
        pthread_create(&rcv_thread, NULL, recv_data, (void *)&sock);
        pthread_detach(snd_thread);
        pthread_detach(rcv_thread);
    } */

    printf("\033[2J");
    gotoxy(1, 1);
    printf("Search word: ");
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
    int len = 0;
    int m;
    char name_msg[NAME_SIZE + BUF_SIZE];
    while (1)
    {

        gotoxy(13 + len, 1);
        fflush(stdout);
        m = getch();

        name_msg[len++] = m;
        name_msg[len] = '\0';

        gotoxy(13+len, 1);
        fflush(stdout);

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

    char list[NAME_SIZE];

    int total = 0;
    for (int i = 0; i < 10; i++)
    {
        clear_lines(3, 10);

        read(sock, &str_len, sizeof(int));
        read(sock, list, str_len);

        list[str_len] = '\0';

        gotoxy(1, 3 + i);
        printf("%s\n", list);
        fflush(stdout);
    }

    return NULL;
}