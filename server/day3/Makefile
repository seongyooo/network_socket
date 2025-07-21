# 컴파일러 지정
CC = gcc

# 컴파일 옵션
# -Wall : 대부분의 경고 메시지를 출력
CFLAGS = -Wall 

# 최종적으로 생성할 실행 파일 이름
TARGETS = server

# make 명령어를 입력했을 때 기본으로 실행될 목표
all: $(TARGETS)

server: hw3_server.c
	$(CC) $(CFLAGS) -o server hw3_server.c

# 클라이언트 프로그램 컴파일 규칙
ser: hw3_client.c
	$(CC) $(CFLAGS) -o client hw3_client.c

# 컴파일된 파일들을 삭제하는 규칙
clean:
	rm -f $(TARGETS)

# 전부 삭제하고 다시 컴파일하는 규칙
re: clean all
