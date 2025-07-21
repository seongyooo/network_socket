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
#include <libgen.h> // for dirname

#define BUF_SIZE 1024
#define EPOLL_SIZE 1024
#define PATH_MAX 4096

// --- 구조체 정의 (기존과 동일) ---
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
    int is_dir;      // 이 변수는 클라이언트측의 논리 오류 가능성이 있어 서버측 상태에 의존하는 것이 더 안정적입니다.
    int dir_index;   // 'cd ..' 기능에 사용
} message;


// [수정 1] 각 클라이언트의 상태를 저장할 컨텍스트 구조체 정의
typedef struct {
    int fd;
    char current_path[PATH_MAX];
    file_information file_info; // 각 클라이언트별 파일 정보
    int is_initial; // 첫 요청인지 확인하는 플래그
} client_context;


void error_handling(char *msg);
// [수정 2] 중복 코드를 제거하기 위해 디렉토리 목록 전송 함수 정의
void send_directory_list(int sock, client_context* ctx);

int main(int argc, char *argv[])
{
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_addr, clnt_addr;
    socklen_t clnt_addr_size;

    struct epoll_event *ep_events;
    struct epoll_event event;
    int epfd, event_cnt;
    
    // [수정 3] 클라이언트 컨텍스트 배열 선언 및 초기화
    client_context contexts[EPOLL_SIZE];
    for(int i=0; i<EPOLL_SIZE; i++){
        contexts[i].fd = -1;
    }

    if (argc != 2)
    {
        printf("Usage %s <port>\n", argv[0]);
        exit(1);
    }

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1) error_handling("socket() error");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[1]));

    if (bind(serv_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("bind() error");

    if (listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    epfd = epoll_create(EPOLL_SIZE);
    ep_events = malloc(sizeof(struct epoll_event) * EPOLL_SIZE);

    event.events = EPOLLIN;
    event.data.fd = serv_sock;
    epoll_ctl(epfd, EPOLL_CTL_ADD, serv_sock, &event);

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
            if (ep_events[i].data.fd == serv_sock) // Connection request
            {
                clnt_addr_size = sizeof(clnt_addr);
                clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_addr, &clnt_addr_size);
                
                event.events = EPOLLIN;
                event.data.fd = clnt_sock;
                epoll_ctl(epfd, EPOLL_CTL_ADD, clnt_sock, &event);
                
                // [수정 4] 새 클라이언트에 대한 컨텍스트 초기화
                contexts[clnt_sock].fd = clnt_sock;
                strcpy(contexts[clnt_sock].current_path, ".");
                contexts[clnt_sock].is_initial = 1;

                printf("connected client: %d \n", clnt_sock);
            }
            else // Data from client
            {
                int client_fd = ep_events[i].data.fd;
                client_context* ctx = &contexts[client_fd];

                // [수정 5] 'while(1){}' 무한 루프 제거!

                // [수정 6] 첫 요청과 이후 요청을 컨텍스트 플래그로 구분
                if (ctx->is_initial) {
                    int start_signal;
                    int read_len = read(client_fd, &start_signal, sizeof(int));
                    if (read_len == 0) { // 클라이언트 비정상 종료 처리
                        epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, NULL);
                        close(client_fd);
                        ctx->fd = -1; // 컨텍스트 초기화
                        printf("Closed client (unexpected): %d\n", client_fd);
                        continue;
                    }
                    printf("Start signal from client %d: %d\n", client_fd, start_signal);
                    if (start_signal == 1) {
                        send_directory_list(client_fd, ctx);
                        ctx->is_initial = 0; // 첫 요청 처리 완료
                    }
                } else {
                    message check_msg;
                    int read_len = read(client_fd, &check_msg, sizeof(message));
                    
                    if (read_len == 0) { // 클라이언트 비정상 종료 처리
                        epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, NULL);
                        close(client_fd);
                        ctx->fd = -1;
                        printf("Closed client (unexpected): %d\n", client_fd);
                        continue;
                    }
                    
                    // 디렉토리 선택 ('B')
                    if (check_msg.msg == 'B') {
                        if (check_msg.dir_index == -1) { // 상위 디렉토리로 이동
                            if (strcmp(ctx->current_path, ".") != 0) {
                                // 경로 문자열을 복사해서 사용 (dirname은 원본을 수정할 수 있음)
                                char path_copy[PATH_MAX];
                                strncpy(path_copy, ctx->current_path, PATH_MAX);
                                strcpy(ctx->current_path, dirname(path_copy));
                            }
                        } else { // 하위 디렉토리로 이동
                            char new_path[PATH_MAX];
                            snprintf(new_path, PATH_MAX, "%s/%s", ctx->current_path, ctx->file_info.dir_name[check_msg.index - 1]);
                            strcpy(ctx->current_path, new_path);
                        }
                        printf("Client %d moved to directory: %s\n", client_fd, ctx->current_path);
                        send_directory_list(client_fd, ctx);
                    }
                    // 파일 다운로드 요청 ('D')
                    else if (check_msg.msg == 'D') {
                        char file_path[PATH_MAX];
                        char buf[BUF_SIZE];
                        int read_cnt;
                        
                        // 서버의 컨텍스트를 기준으로 정확한 파일 경로 생성
                        snprintf(file_path, PATH_MAX, "%s/%s", ctx->current_path, ctx->file_info.file_name[check_msg.index - 1]);
                        
                        FILE* fp = fopen(file_path, "rb");
                        if (fp == NULL) {
                            perror("fopen error on server");
                            // 클라이언트에게 에러 알림 로직 추가 가능
                            continue;
                        }

                        printf("Sending file %s to client %d\n", file_path, client_fd);
                        while ((read_cnt = fread((void *)buf, 1, BUF_SIZE, fp)) > 0) {
                            write(client_fd, buf, read_cnt);
                        }
                        fclose(fp);
                    }
                    // 연결 종료 요청 ('E')
                    else if (check_msg.msg == 'E') {
                        epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, NULL);
                        close(client_fd);
                        ctx->fd = -1; // 컨텍스트 초기화
                        printf("Closed client (requested): %d\n", client_fd);
                    }
                    // 파일 업로드 요청 ('F')
                    else if (check_msg.msg == 'F') {
                         char file_name[100];
                         int file_len;
                         long file_size;

                         read(client_fd, &file_len, sizeof(int));
                         read(client_fd, file_name, file_len);
                         read(client_fd, &file_size, sizeof(long));
                         file_name[file_len] = '\0';
                         
                         char file_path[PATH_MAX];
                         snprintf(file_path, PATH_MAX, "%s/%s", ctx->current_path, file_name);

                         FILE* fp = fopen(file_path, "wb");
                         if(fp == NULL) {
                            perror("Upload fopen error on server");
                            continue;
                         }

                         printf("Receiving file %s from client %d\n", file_path, client_fd);
                         long total = 0;
                         int read_cnt = 0;
                         char buf[BUF_SIZE];
                         while (total < file_size) {
                             read_cnt = read(client_fd, buf, BUF_SIZE);
                             fwrite((void *)buf, 1, read_cnt, fp);
                             total += read_cnt;
                         }
                         fclose(fp);
                         printf("File upload complete: %s\n", file_path);
                    }
                }
            }
        }
    }
    close(serv_sock);
    close(epfd);
    return 0;
}

void error_handling(char *msg) {
    perror(msg);
    exit(1);
}

// 디렉토리 목록을 읽고 클라이언트에게 전송하는 함수
void send_directory_list(int sock, client_context* ctx) {
    DIR *dp = opendir(ctx->current_path);
    if (dp == NULL) {
        perror("opendir error");
        // 상위 디렉토리로 이동 시도
        if (strcmp(ctx->current_path, ".") != 0) {
            char path_copy[PATH_MAX];
            strncpy(path_copy, ctx->current_path, PATH_MAX);
            strcpy(ctx->current_path, dirname(path_copy));
            dp = opendir(ctx->current_path);
            if (dp == NULL) {
                 // 여기서도 실패하면 더 이상 진행 불가
                return;
            }
        } else {
            return;
        }
    }
    
    struct dirent *entry;
    struct stat sb;
    pkt data_index;
    int dir_idx = 0;
    int file_idx = 0;

    // file_info 초기화
    memset(&ctx->file_info, 0, sizeof(file_information));

    while ((entry = readdir(dp)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[PATH_MAX];
        snprintf(full_path, PATH_MAX, "%s/%s", ctx->current_path, entry->d_name);

        if (stat(full_path, &sb) == -1) {
            perror("stat() error");
            continue;
        }

        if (S_ISDIR(sb.st_mode)) {
            strcpy(ctx->file_info.dir_name[dir_idx++], entry->d_name);
        } else if (S_ISREG(sb.st_mode)) {
            strcpy(ctx->file_info.file_name[file_idx], entry->d_name);
            ctx->file_info.file_size[file_idx++] = sb.st_size;
        }
    }
    closedir(dp);

    data_index.dir_index = dir_idx;
    data_index.file_index = file_idx;
    write(sock, &data_index, sizeof(pkt));

    for (int j = 0; j < dir_idx; j++) {
        data_index.name_len = strlen(ctx->file_info.dir_name[j]);
        write(sock, &data_index.name_len, sizeof(int));
        write(sock, ctx->file_info.dir_name[j], data_index.name_len);
    }

    for (int j = 0; j < file_idx; j++) {
        data_index.name_len = strlen(ctx->file_info.file_name[j]);
        write(sock, &data_index.name_len, sizeof(int));
        write(sock, ctx->file_info.file_name[j], data_index.name_len);
        write(sock, &ctx->file_info.file_size[j], sizeof(long));
    }
}