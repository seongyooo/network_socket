#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <pthread.h>

#define BUF_SIZE 1024

void error_handling(char *msg);
int load_data(char *file_name);
void *handle_clnt(void *arg);
int compare(const void *first, const void *second);

pthread_mutex_t mutx;

typedef struct
{
    char data_name[BUF_SIZE];
    int data_size;
} data_info;

typedef struct
{
    data_info data_info;
    int index;
    int data_len;
} data_cnt;

data_cnt *serv_cnt;
int file_cnt = 0;

int main(int argc, char *argv[])
{
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_addr, clnt_addr;
    int clnt_adr_sz;

    pthread_t t_id;
    if (argc != 3)
    {
        printf("Usage %s <port> <file>\n", argv[0]);
        exit(1);
    }

    serv_cnt = malloc(sizeof(data_cnt) * BUF_SIZE);

    pthread_mutex_init(&mutx, NULL);
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

    file_cnt = load_data(argv[2]);

    for (int i = 0; i < file_cnt; i++)
    {
        //printf("%s: %d\n", serv_cnt[i].data_info.data_name, serv_cnt[i].data_info.data_size);
    }

    while (1)
    {
        clnt_adr_sz = sizeof(clnt_addr);
        clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_addr, &clnt_adr_sz);

        printf("Connect client: %d\n", clnt_sock);
        pthread_t t_id;
        if ((pthread_create(&t_id, NULL, handle_clnt, (void *)&clnt_sock)) != 0)
        {
            error_handling("pthread_create() error");
        }
        pthread_detach(t_id);
    }

    close(serv_sock);
    return 0;
}

void error_handling(char *msg)
{
    printf("%s\n", msg);
    exit(1);
}

int load_data(char *file_name)
{
    FILE *fp;
    char buf[BUF_SIZE];
    char empty[] = " ";
    char *result;
    char total[BUF_SIZE];
    char read_cnt[BUF_SIZE];

    char *cnt;
    int file_cnt = 0;

    fp = fopen(file_name, "r");

    while (fgets(read_cnt, sizeof(read_cnt), fp))
    {
        read_cnt[strcspn(read_cnt, "\n")] = '\0';
        cnt = strrchr(read_cnt, ' ');

        *cnt = '\0';

        strcpy(serv_cnt[file_cnt].data_info.data_name, read_cnt);
        serv_cnt[file_cnt].data_info.data_size = atoi(cnt + 1);

        file_cnt++;
    }
    fclose(fp);

    return file_cnt;
}

void *handle_clnt(void *arg)
{
    int clnt_sock = *((int *)arg);
    int str_len = 0, i, read_cnt;
    char msg[BUF_SIZE];
    data_cnt search_list[BUF_SIZE];

    while (1)
    {
        read(clnt_sock, &str_len, sizeof(int));
        read(clnt_sock, msg, str_len);

        msg[str_len-1] = '\0';
        printf("recv msg: %s\n", msg);

        int count = 0;
        printf("file_cnt: %d\n", file_cnt);
        for (int i = 0; i < file_cnt; i++)
        {
            if (strstr(serv_cnt[i].data_info.data_name, msg) != NULL)
            {
                printf("test\n");
                search_list[count++] = serv_cnt[i];
            }
        }

        printf("count: %d\n", count);
        if(count > 10) count = 10;
        write(clnt_sock, &count, sizeof(int));

        qsort(search_list, count, sizeof(data_cnt), compare);
        for (int i = 0; i < count; i++)
        {
            str_len = strlen(search_list[i].data_info.data_name);
            printf("test: %s, len: %d\n", search_list[i].data_info.data_name, str_len);
            write(clnt_sock, &str_len, sizeof(int));
            write(clnt_sock, search_list[i].data_info.data_name, str_len);
        }
    }

    close(clnt_sock);
    return NULL;
}

int compare(const void *first, const void *second)
{
    data_cnt *ptr_first = (data_cnt *)first;
    data_cnt *ptr_second = (data_cnt *)second;

    if (ptr_first->data_info.data_size < ptr_second->data_info.data_size)
    {
        return -1;
    }
    else if (ptr_first->data_info.data_size > ptr_second->data_info.data_size)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}