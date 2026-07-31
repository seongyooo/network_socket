# UDP 기반 실시간 멀티플레이어 네트워크 게임

C 언어만으로 터미널 UI, 소켓 통신, 멀티스레드 구조를 직접 설계·구현한 실시간 멀티플레이어 게임입니다.
외부 라이브러리 없이 POSIX 소켓 API와 pthread만 사용했습니다.

> **기간** 2025.07 ~ 2025.08 · **소속** MCNL 연구실 · **인원** 개인 프로젝트

---

## 최종 프로젝트: 멀티플레이어 격자 점령 게임

여러 플레이어가 같은 격자판에 동시에 접속해 칸을 차지하는 실시간 대전 게임입니다.

### 해결한 문제

**1. Race Condition**

여러 플레이어가 동시에 같은 격자판을 수정하면서 판 상태가 비결정적으로 망가지는 문제가 발생했습니다.
문제가 재현되는 상황을 먼저 만들어 원인을 확인한 뒤, `pthread_mutex`로 공유 게임 상태에 대한
접근을 직렬화해 해결했습니다.

**2. 프로토콜 분리 설계**

데이터 성격에 따라 전송 방식을 나눴습니다.

| 데이터 | 프로토콜 | 이유 |
|---|---|---|
| 플레이어 이동·게임 상태 브로드캐스트 | UDP 멀티캐스트 (`224.1.1.2`) | 실시간성이 중요하고 일부 손실이 허용됨 |
| 접속·점수 처리 | TCP | 유실되면 안 되는 신뢰성 요구 데이터 |

### 구현 방식

- **서버**: 클라이언트별 `pthread` 스레드로 접속을 처리하고, 격자 상태를 전역 구조체(`game_information`)로 관리
- **클라이언트**: 멀티캐스트 수신 스레드와 입력 처리를 분리해 동시에 동작
- **상태 공유**: 격자 상태·남은 시간·플레이어별 위치를 구조체에 담아 멀티캐스트로 일괄 전송

---

## 빌드 및 실행

```bash
# 서버
cd server/final_project
gcc multi_game_server.c -o server -lpthread
./server <player> <grid size> <panel> <time> <port>

# 클라이언트
cd client/final_project
gcc multi_game_client.c -o client -lpthread
./client <GROUP_IP> <port>
```

예시:

```bash
./server 2 10 20 60 9190      # 2인 / 10x10 격자 / 패널 20개 / 60초 / 포트 9190
./client 224.1.1.2 9190
```

- 격자 크기의 제곱이 패널 수보다 크거나 같아야 합니다.
- Linux 환경에서 테스트했습니다.

---

## 저장소 구조

```
.
├── client/
│   ├── day1/ ~ day4/             # 소켓 프로그래밍 실습 과제
│   └── final_project/
│       └── multi_game_client.c   # 최종 프로젝트 (477 lines)
│
└── server/
    ├── day1/ ~ day4/             # 소켓 프로그래밍 실습 과제
    └── final_project/
        └── multi_game_server.c   # 최종 프로젝트 (313 lines)
```

`day1` ~ `day4`는 최종 프로젝트에 앞서 진행한 소켓 프로그래밍 실습 과제입니다.
파일 전송, `select()` 기반 I/O 멀티플렉싱, 에코 서버 등 단계별로 기능을 익힌 기록입니다.

## 사용 기술

`C` `POSIX Socket` `pthread` `UDP Multicast` `TCP` `Linux`
