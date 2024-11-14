#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 50000
#define BUFFER_SIZE 1024

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};

    // ソケット作成
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n ソケット作成エラー \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // サーバーIPアドレスを設定
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\n 無効なアドレス/アドレスはサポートされていません \n");
        return -1;
    }

    // サーバーに接続
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\n 接続失敗 \n");
        return -1;
    }

    printf("サーバーに接続しました。コマンドを入力してください（例: attack, defend）\n");

    while (1) {
        char command[BUFFER_SIZE];
        printf("コマンドを入力: ");
        fgets(command, BUFFER_SIZE, stdin);
        command[strcspn(command, "\n")] = '\0'; // 改行を削除

        // コマンドをサーバーに送信
        send(sock, command, strlen(command), 0);

        // サーバーからの応答を受信
        int bytes_read = read(sock, buffer, BUFFER_SIZE);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0'; // 受信したデータを文字列にする
            printf("サーバーからの応答: %s\n", buffer);
        }

        // HPが0になった場合は終了
        if (strstr(buffer, "Your health: 0") != NULL) {
            printf("あなたは敗北しました！\n");
            break;
        } else if (strstr(buffer, "Enemy health: 0") != NULL) {
            printf("あなたは勝利しました！\n");
            break;
        }
    }

    close(sock);
    return 0;
}
