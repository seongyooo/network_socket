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
    int is_dir;
    int dir_index;
} message;

void error_handling(char *msg);

int main(int argc, char *argv[])
{
    int sock;
    struct sockaddr_in serv_addr;

    int start = 0;

    FILE *fp;
    DIR *dp;
    struct dirent *file;
    struct stat sb;
    file_information file_info;
    file_information client_info;
    pkt data_index;
    message check_msg;

    int dir_idx = 0, file_idx = 0;
    int total, read_cnt;

    char input[BUF_SIZE];

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

    // 멀티플렉싱 테스트 검증
    while (1)
    {
        fputs("Start? input S: \n", stdout);
        fgets(input, BUF_SIZE, stdin);

        if (!strcmp(input, "S\n"))
            break;
    }

    start = 1;
    write(sock, &start, sizeof(int));

    // 디렉토리 개수, 파일개수 먼저 받기
    read(sock, &data_index, sizeof(pkt));

    for (int i = 0; i < data_index.dir_index; i++)
    {
        read(sock, &data_index.name_len, sizeof(int));
        read(sock, file_info.dir_name[i], data_index.name_len);
        file_info.dir_name[i][data_index.name_len] = '\0';
    }

    for (int i = 0; i < data_index.file_index; i++)
    {
        read(sock, &data_index.name_len, sizeof(int));
        read(sock, file_info.file_name[i], data_index.name_len);
        read(sock, &file_info.file_size[i], sizeof(long));
        file_info.file_name[i][data_index.name_len] = '\0';
    }

    // Interface
    int dir_num, file_num;
    char char_num[BUF_SIZE];
    int is_dir=0;
    while (1)
    {
        fputs("MENU\n", stdout);
        fputs("-------------------------------\n", stdout);
        fputs("Input: A (print file list)\n", stdout);
        fputs("Input: B (select directory)\n", stdout);
        fputs("Input: D (select file)\n", stdout);
        fputs("Input: E (closed connet & quit)\n", stdout);
        fputs("Input: F (upload server)\n", stdout);

        fgets(input, BUF_SIZE, stdin);

        if (!strcmp(input, "A\n"))
        {
            for (int i = 0; i < data_index.dir_index; i++)
            {
                printf("%d. dir_name: %s\n\n", i + 1, file_info.dir_name[i]);
            }

            printf("-------------------------------\n");
            for (int i = 0; i < data_index.file_index; i++)
            {
                printf("%d. file_name: %s\n", i + 1, file_info.file_name[i]);
                printf("file_size: %ld\n\n", file_info.file_size[i]);
            }
        }

        if (!strcmp(input, "B\n"))
        {
            is_dir=1;
            fputs("Input directory number( -1 -> cd ..): ", stdout);
            fgets(char_num, BUF_SIZE, stdin);
            dir_num = atoi(char_num);

            check_msg.msg = 'B';
            check_msg.index = dir_num;
            check_msg.dir_index = dir_num;
            write(sock, &check_msg, sizeof(message));

            // 디렉토리 개수, 파일개수 먼저 받기
            read(sock, &data_index, sizeof(pkt));

            for (int i = 0; i < data_index.dir_index; i++)
            {
                read(sock, &data_index.name_len, sizeof(int));
                read(sock, file_info.dir_name[i], data_index.name_len);
                file_info.dir_name[i][data_index.name_len] = '\0';
            }

            for (int i = 0; i < data_index.file_index; i++)
            {
                read(sock, &data_index.name_len, sizeof(int));
                read(sock, file_info.file_name[i], data_index.name_len);
                read(sock, &file_info.file_size[i], sizeof(long));
                file_info.file_name[i][data_index.name_len] = '\0';
            }
        }

        if (!strcmp(input, "D\n"))
        {
            fputs("Input file number: ", stdout);
            fgets(char_num, BUF_SIZE, stdin);
            file_num = atoi(char_num);

            check_msg.msg = 'D';
            check_msg.index = file_num;
            check_msg.is_dir = is_dir;
            write(sock, &check_msg, sizeof(message));

            fp = fopen(file_info.file_name[file_num - 1], "wb");

            char buf[BUF_SIZE];
            read_cnt=0;
            total=0;
            while (total < file_info.file_size[file_num - 1])
            {
                read_cnt = read(sock, buf, sizeof(buf));

                fwrite((void *)buf, 1, read_cnt, fp);
                total += read_cnt;
            }
            fclose(fp);
        }

        if (!strcmp(input, "E\n"))
        {
            check_msg.msg = 'E';
            write(sock, &check_msg, sizeof(message));

            break;
        }

        if (!strcmp(input, "F\n"))
        {
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
                    strcpy(client_info.dir_name[dir_idx++], file->d_name);
                }
                else if (S_ISREG(sb.st_mode))
                {
                    strcpy(client_info.file_name[file_idx], file->d_name);
                    client_info.file_size[file_idx++] = sb.st_size;
                }
            }

            for (int i = 0; i < dir_idx; i++)
            {
                printf("%d. dir_name: %s\n\n", i + 1, client_info.dir_name[i]);
            }

            for (int i = 0; i < file_idx; i++)
            {
                printf("%d. file_name: %s\n", i + 1, client_info.file_name[i]);
                printf("file_size: %ld\n\n", client_info.file_size[i]);
            }

            fputs("Input file number: ", stdout);
            fgets(char_num, BUF_SIZE, stdin);
            file_num = atoi(char_num);

            check_msg.msg = 'F';
            check_msg.index = file_num;
            write(sock, &check_msg, sizeof(message));

            data_index.name_len = strlen(client_info.file_name[file_num-1]);
            write(sock, &data_index.name_len, sizeof(int));
            write(sock, client_info.file_name[file_num-1], data_index.name_len);
            write(sock, &client_info.file_size[file_num-1], sizeof(long));

            int read_cnt;
            char buf[BUF_SIZE];
            fp = fopen(client_info.file_name[check_msg.index - 1], "rb");
            while (1)
            {
                read_cnt = fread((void *)buf, 1, BUF_SIZE, fp);

                if (read_cnt < BUF_SIZE)
                {
                    write(sock, buf, read_cnt);
                    break;
                }

                write(sock, buf, read_cnt);
            }
            fclose(fp);

        }
    }

    close(sock);
    return 0;
}

void error_handling(char *msg)
{
    printf("%s", msg);
    exit(1);
}