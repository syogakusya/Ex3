#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <termios.h>
#include <fcntl.h>

#define PORT 50000
#define BUFFER_SIZE 1024

void enableRawMode()
{
    struct termios raw;
    tcgetattr(STDIN_FILENO, &raw);
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    // ノンブロッキングモードでサーバーからのメッセージを受信できるように
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
}

int main()
{
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};

    // ソケット作成
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n ソケット作成エラー \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // サーバーIPアドレスを設定
    if (inet_pton(AF_INET, "172.28.34.67", &serv_addr.sin_addr) <= 0)
    {
        printf("\n 無効なアドレス/アドレスはサポートされていません \n");
        return -1;
    }

    // サーバーに接続
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("\n 接続失敗 \n");
        return -1;
    }

    enableRawMode();
    printf("\033[2J\033[H"); // 画面クリア

    while (1)
    {
        char c;
        if (read(STDIN_FILENO, &c, 1) == 1)
        {
            if (c == 'q')
                break;
            if (strchr("wasd", c) != NULL)
            {
                send(sock, &c, 1, 0);
            }
        }

        char buffer[BUFFER_SIZE];
        int bytes_read = recv(sock, buffer, BUFFER_SIZE, MSG_DONTWAIT);
        if (bytes_read > 0)
        {
            buffer[bytes_read] = '\0';
            // サーバーフルメッセージのチェックを追加
            if (strstr(buffer, "Server is full") != NULL)
            {
                printf("\033[2J\033[H"); // 画面クリア
                printf("%s", buffer);
                fflush(stdout);
                sleep(2); // メッセージを2秒間表示
                break;    // メインループを抜ける
            }
            printf("\033[2J\033[H"); // 画面クリア
            printf("%s", buffer);
            fflush(stdout);
        }
    }

    printf("\033[2J\033[H\033[0m");
    close(sock);
    return 0;
}
