#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <windows.h>
#include <iostream>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

#define SERVERPORT 9000
#define BUFSIZE    1024

int main()
{
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(SERVERPORT);

    bind(listenSock, (sockaddr*)&serveraddr, sizeof(serveraddr));
    listen(listenSock, 1);

    cout << "서버 대기 중...\n";

    sockaddr_in clientaddr;
    int addrlen = sizeof(clientaddr);
    SOCKET clientSock = accept(listenSock, (sockaddr*)&clientaddr, &addrlen);

    cout << "클라이언트 접속!\n";

    char buf[BUFSIZE];

    while (1)
    {
        // 클라이언트로부터 명령 받기
        int recvLen = recv(clientSock, buf, BUFSIZE - 1, 0);
        if (recvLen <= 0) {
            cout << "클라이언트 종료\n";
            break;
        }
        buf[recvLen] = '\0';

        cout << "받은 명령: " << buf;

        // 1) list 명령
        if (strncmp(buf, "list", 4) == 0)
        {
            // 현재 폴더에서 *.* 다 뒤지고, .txt .jpg .jpeg만 보내기
            WIN32_FIND_DATAA data;
            HANDLE hFind = FindFirstFileA("*.*", &data);

            if (hFind != INVALID_HANDLE_VALUE) {
                do {
                    // 디렉토리면 패스
                    if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                        continue;

                    const char* name = data.cFileName;
                    const char* dot = strrchr(name, '.'); // 마지막 '.' 위치
                    if (dot) {
                        // txt, jpg, jpeg만 보내기 (대소문자 구분 없이)
                        if (_stricmp(dot, ".txt") == 0 ||
                            _stricmp(dot, ".jpg") == 0 ||
                            _stricmp(dot, ".jpeg") == 0)
                        {
                            char line[BUFSIZE];
                            sprintf_s(line, BUFSIZE, "%s\n", name);
                            send(clientSock, line, (int)strlen(line), 0);
                        }
                    }
                } while (FindNextFileA(hFind, &data));

                FindClose(hFind);
            }

            // 목록 끝 표시
            send(clientSock, "END\n", 4, 0);
        }
        // 2) get 명령
        else if (strncmp(buf, "get ", 4) == 0)
        {
            // "get 파일이름\n" 에서 파일 이름만 뽑기
            char filename[260];
            int i = 4; // 'get ' 뒤부터
            int j = 0;
            while (buf[i] != '\0' && buf[i] != '\n' && buf[i] != '\r' && j < 259) {
                filename[j++] = buf[i++];
            }
            filename[j] = '\0';

            if (filename[0] == '\0') {
                send(clientSock, "ERR NOFILE\n", 11, 0);
                continue;
            }

            // 파일 열기 (바이너리, fopen_s 사용)
            FILE* fp = nullptr;
            fopen_s(&fp, filename, "rb");
            if (!fp) {
                cout << "파일 없음: " << filename << endl;
                send(clientSock, "ERR NOFILE\n", 11, 0);
                continue;
            }

            // 파일 크기 구하기
            fseek(fp, 0, SEEK_END);
            long size = ftell(fp);
            fseek(fp, 0, SEEK_SET);

            // "OK 크기\n" 먼저 보내기 (sprintf_s 사용)
            char header[64];
            sprintf_s(header, sizeof(header), "OK %ld\n", size);
            send(clientSock, header, (int)strlen(header), 0);

            // 파일 내용 보내기
            long sent = 0;
            while (sent < size) {
                int n = (int)fread(buf, 1, BUFSIZE, fp);
                if (n <= 0) break;
                send(clientSock, buf, n, 0);
                sent += n;
            }

            cout << "전송 완료: " << filename << " (" << sent << " bytes)\n";
            fclose(fp);
        }
        // 그 외 명령
        else {
            send(clientSock, "ERR UNKNOWN\n", 12, 0);
        }
    }

    closesocket(clientSock);
    closesocket(listenSock);
    WSACleanup();

    return 0;
}
