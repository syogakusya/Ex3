#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <termios.h>
#include <fcntl.h>

#define PORT 50000
#define BUFFER_SIZE 1024

// 生モードを有効化する関数
void enableRawMode()
{
    struct termios raw;
    if (tcgetattr(STDIN_FILENO, &raw) == -1)
    {
        perror("tcgetattr failed");
        exit(EXIT_FAILURE);
    }
    raw.c_lflag &= ~(ECHO | ICANON);
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    {
        perror("tcsetattr failed");
        exit(EXIT_FAILURE);
    }
    // ノンブロッキングモードを設定
    if (fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK) == -1)
    {
        perror("fcntl failed");
        exit(EXIT_FAILURE);
    }
}

int main()
{
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};

    // ソケット作成
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("ソケット作成エラー");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // サーバーIPアドレスを設定（必要に応じて変更）
    if (inet_pton(AF_INET, "172.28.34.67", &serv_addr.sin_addr) <= 0)
    {
        perror("無効なアドレス/アドレスはサポートされていません");
        return -1;
    }

    // サーバーに接続
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("接続失敗");
        return -1;
    }

    enableRawMode();
    printf("\033[2J\033[H"); // 画面クリア

    while (1)
    {
        char c;
        // キー入力を非ブロッキングで読み取る
        if (read(STDIN_FILENO, &c, 1) == 1)
        {
            if (c == 'q')
                break;
            if (strchr("wasd", c) != NULL)
            {
                if (send(sock, &c, 1, 0) == -1)
                {
                    perror("send failed");
                }
            }
        }

        // サーバーからのメッセージを非ブロッキングで受信
        int bytes_read = recv(sock, buffer, BUFFER_SIZE - 1, MSG_DONTWAIT);
        if (bytes_read > 0)
        {
            buffer[bytes_read] = '\0';
            // サーバー満員メッセージのチェック
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
