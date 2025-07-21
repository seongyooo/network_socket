#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>

#define BUF_SIZE 1024
#define EPOLL_SIZE 1024
// 203.252.112.31
typedef struct
{
    char dir_name[BUF_SIZE][100];
    char file_name[BUF_SIZE][100];
    long file_size[BUF_SIZE];

} file_information;

typedef struct
{
    int dir_index;
    int file_index;
    int name_len;
} pkt;

typedef struct
{
    char msg;
    int index;
} message;

void error_handling(char *msg);

int main(int argc, char *argv[])
{

    int serv_sock, clnt_sock;
    struct sockaddr_in serv_addr, clnt_addr;
    socklen_t clnt_addr_size;

    struct epoll_event *ep_events;
    struct epoll_event event;
    int epfd, event_cnt;

    int start = 0;
    int str_len;

    FILE *fp;
    DIR *dp;
    struct dirent *file;
    struct stat sb;
    file_information file_info;
    file_information serv_info;
    pkt data_index;
    message check_msg;

    int dir_idx = 0, file_idx = 0;

    if (argc != 2)
    {
        printf("Usage %s <port>\n", argv[0]);
        exit(1);
    }

    // 소켓 생성
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1)
    {
        error_handling("socket() error");
    }

    // bind()
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[1]));

    if (bind(serv_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    {
        error_handling("bind() error");
    }

    // listen()
    if (listen(serv_sock, 5) == -1)
    {
        error_handling("listen() error");
    }

    // epoll create()
    epfd = epoll_create(EPOLL_SIZE);
    ep_events = malloc(sizeof(struct epoll_event) * EPOLL_SIZE);

    // epoll ctl()
    event.events = EPOLLIN;
    event.data.fd = serv_sock;
    epoll_ctl(epfd, EPOLL_CTL_ADD, serv_sock, &event);

    int check = 1;
    int num;
    int is_dir = 1;
    char msg;
    char file_path[100];

    while (1)
    {
        event_cnt = epoll_wait(epfd, ep_events, EPOLL_SIZE, -1);
        if (event_cnt == -1)
        {
            puts("epoll_wait() error");
            break;
        }

        for (int i = 0; i < event_cnt; i++)
        {
            if (ep_events[i].data.fd == serv_sock)
            {
                clnt_addr_size = sizeof(clnt_sock);
                clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_addr, &clnt_addr_size);

                event.events = EPOLLIN;
                event.data.fd = clnt_sock;
                epoll_ctl(epfd, EPOLL_CTL_ADD, clnt_sock, &event);
                printf("connected client: %d \n", clnt_sock);
            }
            else
            {
                // event가 read를 감지하고 있기에
                if (check)
                {
                    read(ep_events[i].data.fd, &start, sizeof(int));
                    printf("Start: %d\n", start);
                    check = 0;

                    dp = opendir(".");

                    dir_idx = 0;
                    file_idx = 0;
                    while ((file = readdir(dp)) != NULL)
                    {
                        if (strcmp(file->d_name, ".") == 0 || strcmp(file->d_name, "..") == 0)
                        {
                            continue;
                        }

                        if (stat(file->d_name, &sb) == -1)
                        {
                            error_handling("stat(), error");
                        }

                        if (S_ISDIR(sb.st_mode))
                        {
                            strcpy(file_info.dir_name[dir_idx++], file->d_name);
                        }
                        else if (S_ISREG(sb.st_mode))
                        {
                            strcpy(file_info.file_name[file_idx], file->d_name);
                            file_info.file_size[file_idx++] = sb.st_size;
                        }
                    }
                    // 디렉토리 개수, 파일개수 먼저 보내기
                    data_index.dir_index = dir_idx;
                    data_index.file_index = file_idx;
                    write(ep_events[i].data.fd, &data_index, sizeof(pkt));

                    // 디렉토리 이름 & 파일 이름, 사이즈 보내기
                    for (int j = 0; j < dir_idx; j++)
                    {
                        data_index.name_len = strlen(file_info.dir_name[j]);
                        write(ep_events[i].data.fd, &data_index.name_len, sizeof(int));
                        write(ep_events[i].data.fd, file_info.dir_name[j], data_index.name_len);
                    }

                    for (int j = 0; j < file_idx; j++)
                    {
                        data_index.name_len = strlen(file_info.file_name[j]);
                        write(ep_events[i].data.fd, &data_index.name_len, sizeof(int));
                        write(ep_events[i].data.fd, file_info.file_name[j], data_index.name_len);
                        write(ep_events[i].data.fd, &file_info.file_size[j], sizeof(long));
                    }

                    closedir(dp);
                }
                else
                {
                    read(ep_events[i].data.fd, &check_msg, sizeof(message));
                }

                // num이 처음에는 start(2) 값을 받고, 그 이후에는 파일 & 디렉토리 번호를 받음
                if (check_msg.msg == 'B')
                {
                    dp = opendir(file_info.dir_name[check_msg.index - 1]);
                    printf("dir_name: %s\n", file_info.dir_name[check_msg.index - 1]);

                    dir_idx = 0;
                    file_idx = 0;
                    while ((file = readdir(dp)) != NULL)
                    {
                        if (strcmp(file->d_name, ".") == 0 || strcmp(file->d_name, "..") == 0)
                        {
                            continue;
                        }
                        strcpy(file_path, "./");
                        strcat(file_path, file_info.dir_name[check_msg.index - 1]);
                        strcat(file_path, "/");
                        strcat(file_path, file->d_name);
                        if (stat(file_path, &sb) == -1)
                        {
                            error_handling("stat() error");
                        }

                        if (S_ISDIR(sb.st_mode))
                        {
                            strcpy(file_info.dir_name[dir_idx++], file->d_name);
                        }
                        else if (S_ISREG(sb.st_mode))
                        {
                            strcpy(file_info.file_name[file_idx], file->d_name);
                            file_info.file_size[file_idx++] = sb.st_size;
                        }
                    }
                    // 디렉토리 개수, 파일개수 먼저 보내기
                    data_index.dir_index = dir_idx;
                    data_index.file_index = file_idx;
                    write(ep_events[i].data.fd, &data_index, sizeof(pkt));

                    // 디렉토리 이름 & 파일 이름, 사이즈 보내기
                    for (int j = 0; j < dir_idx; j++)
                    {
                        data_index.name_len = strlen(file_info.dir_name[j]);
                        write(ep_events[i].data.fd, &data_index.name_len, sizeof(int));
                        write(ep_events[i].data.fd, file_info.dir_name[j], data_index.name_len);
                    }

                    for (int j = 0; j < file_idx; j++)
                    {
                        data_index.name_len = strlen(file_info.file_name[j]);
                        write(ep_events[i].data.fd, &data_index.name_len, sizeof(int));
                        write(ep_events[i].data.fd, file_info.file_name[j], data_index.name_len);
                        write(ep_events[i].data.fd, &file_info.file_size[j], sizeof(long));
                    }

                    closedir(dp);
                }

                // 파일 내용 전송
                if (check_msg.msg == 'D')
                {
                    int read_cnt;
                    char buf[BUF_SIZE];
                    fp = fopen(file_info.file_name[check_msg.index - 1], "rb");
                    while ((read_cnt = fread((void *)buf, 1, BUF_SIZE, fp)) > 0)
                    {
                        write(ep_events[i].data.fd, buf, read_cnt);
                    }
                    fclose(fp);
                }

                // 클라이언트 소켓 종료
                if (check_msg.msg == 'E')
                {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, ep_events[i].data.fd, NULL); // 리눅스 2.6.9 이전에는 NULL 전달 X(비록 전달된 값은 무시됨)
                    close(ep_events[i].data.fd);
                    printf("Closed client: %d\n", ep_events[i].data.fd);
                }

                if (check_msg.msg == 'F')
                {
                    char file_name[100];
                    int file_len;
                    long file_size;

                    read(ep_events[i].data.fd, &file_len, sizeof(int));
                    read(ep_events[i].data.fd, file_name, file_len);
                    read(ep_events[i].data.fd, &file_size, sizeof(long));
                    file_name[file_len] = '\0';

                    fp = fopen(file_name, "wb");
                    int total=0;
                    int read_cnt=0;
                    char buf[BUF_SIZE];
                    while (total < file_size)
                    {
                        read_cnt = read(ep_events[i].data.fd, buf, sizeof(buf));

                        fwrite((void *)buf, 1, read_cnt, fp);
                        total += read_cnt;
                    }
                    fclose(fp);
                }
            }
        }
    }

    close(serv_sock);
    return 0;
}

void error_handling(char *msg)
{
    printf("%s\n", msg);
    exit(1);
}