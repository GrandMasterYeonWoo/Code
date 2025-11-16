#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <windows.h>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <string>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

#define SERVERPORT 9000
#define BUFSIZE    1024

int main()
{
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr("127.0.0.1"); // 같은 PC
    serveraddr.sin_port = htons(SERVERPORT);

    if (connect(sock, (sockaddr*)&serveraddr, sizeof(serveraddr)) == SOCKET_ERROR) {
        cout << "connect 실패\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    cout << "서버 접속 성공\n";

    char buf[BUFSIZE];

    // 1) list 요청 보내기
    strcpy_s(buf, BUFSIZE, "list\n");
    send(sock, buf, (int)strlen(buf), 0);

    cout << "=== 서버 파일 목록 (txt, jpg/jpeg) ===\n";

    bool doneList = false;
    while (!doneList) {
        int len = recv(sock, buf, BUFSIZE - 1, 0);
        if (len <= 0) {
            cout << "연결 끊김\n";
            closesocket(sock);
            WSACleanup();
            return 1;
        }
        buf[len] = '\0';

        // 줄 단위로 자르기
        char* ctx = nullptr;
        char* line = strtok_s(buf, "\n", &ctx);
        while (line != NULL) {
            if (strcmp(line, "END") == 0) {
                doneList = true;      // END 봤다!
                break;                // 안쪽 while 탈출
            }
            cout << line << endl;     // 파일 이름 출력
            line = strtok_s(NULL, "\n", &ctx);
        }
        // END를 본 경우에만 바깥 while 탈출
        // 아니면 recv 더 해서 나머지 목록 이어서 읽기
    }

    cout << "===============================\n";

    // 2) 다운로드할 파일 이름 입력
    cout << "받을 파일 이름 입력 (예: test.jpg): ";
    string filename;
    getline(cin, filename);

    if (filename.empty()) {
        cout << "입력 없음, 종료\n";
        closesocket(sock);
        WSACleanup();
        return 0;
    }

    // 3) get 명령 보내기
    string cmd = "get " + filename + "\n";
    send(sock, cmd.c_str(), (int)cmd.size(), 0);

    // 4) 서버의 첫 줄 응답 받기 (OK 또는 ERR)
    int len = recv(sock, buf, BUFSIZE - 1, 0);
    if (len <= 0) {
        cout << "서버 응답 없음\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }
    buf[len] = '\0';
    cout << "서버 응답: " << buf;

    if (strncmp(buf, "ERR", 3) == 0) {
        cout << "파일 없음 또는 에러\n";
        closesocket(sock);
        WSACleanup();
        return 0;
    }

    if (strncmp(buf, "OK ", 3) != 0) {
        cout << "알 수 없는 응답\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    // "OK 크기\n" 에서 크기만 뽑기
    long filesize = atol(buf + 3);
    cout << "받을 파일 크기: " << filesize << " bytes\n";

    // 5) 로컬 파일 열기 (바이너리, fopen_s 사용)
    FILE* fp = nullptr;
    fopen_s(&fp, filename.c_str(), "wb");
    if (!fp) {
        cout << "로컬 파일 생성 실패\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    // 6) 파일 데이터 받기
    long received = 0;
    while (received < filesize) {
        int toRecv = BUFSIZE;
        long remain = filesize - received;
        if (remain < toRecv) toRecv = (int)remain;

        int n = recv(sock, buf, toRecv, 0);
        if (n <= 0) {
            cout << "수신 중 에러\n";
            break;
        }

        fwrite(buf, 1, n, fp);
        received += n;
    }

    fclose(fp);

    cout << "파일 수신 완료: " << received << " / " << filesize << " bytes\n";

    closesocket(sock);
    WSACleanup();
    return 0;
}
