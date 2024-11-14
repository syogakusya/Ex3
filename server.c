#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>

#define PORT 50000
#define MAX_EVENTS 3
#define BUFFER_SIZE 1024

typedef struct {
    int health;
    int socket;
} Player;

// SA → HA → C　→ SA
void handle_battle(Player *player1, Player *player2, char * cmd1, char *cmd2){
    printf("Player 1: %s, Player 2: %s\n", cmd1, cmd2);
    if(strcmp(cmd1, "SA") == 0){
        if(strcmp(cmd2, "SA") == 0){
            // SA = SA
            player1->health -= 5;
            player2->health -= 5;
        }else if(strcmp(cmd2, "HA") == 0){
            // SA > HA
            player2->health -= 10;
        }else{ // cmd2 == C
            // SA < C
            player1->health -= 10;
        }
    }else if(strcmp(cmd1, "HA") == 0){
        if(strcmp(cmd2, "SA") == 0){
            // HA < SA
            player1->health -= 10;
        }else if(strcmp(cmd2, "HA") == 0){
            // HA = HA
            player2->health -= 10;
            player1->health -= 10;
        }else{ // cmd2 == C
            // HA > C
            player2->health -= 15;
        }
    }else{// cmd1 == C
        if(strcmp(cmd2, "SA") == 0){
            // C > SA
            player2->health -= 10;
        }else if(strcmp(cmd2, "HA") == 0){
            // C < HA
            player1->health -= 15;
        }else{ // cmd2 == C
            // C = C
        }
    }  

    char response[BUFFER_SIZE];
    snprintf(response, BUFFER_SIZE, "Your health: %d, Enemy health: %d\n", player1->health, player2->health);
    send(player1->socket, response, strlen(response), 0);
    snprintf(response, BUFFER_SIZE, "Your health: %d, Enemy health: %d\n", player2->health, player1->health);
    send(player2->socket, response, strlen(response), 0);
}

int main(){
    int server_fd, new_socket, epoll_fd;
    struct sockaddr_in address;
    struct epoll_event event, events[MAX_EVENTS];
    char buffer[BUFFER_SIZE];

    Player players[MAX_EVENTS];
    int connected_players = 0;

    // ソケット作成
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(1);
    }

    // ソケットオプション設定
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        close(server_fd);
        exit(1);
    }

    //アドレス作成
    memset(&address, 0, sizeof(struct sockaddr_in));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(PORT);

    // ソケットをバインド
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(struct sockaddr_in)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(1);
    }

    if (listen(server_fd, 5) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(1);
    }

    // epoll作成
    if ((epoll_fd = epoll_create1(0)) == -1) {
        perror("epoll_create1 failed");
        exit(EXIT_FAILURE);
    }

    event.events = EPOLLIN;
    event.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1) {
        perror("epoll_ctl failed");
        exit(EXIT_FAILURE);
    }

    printf("server is opened at PORT : %d \n", PORT);
    printf("Waiting for players...\n");

    while (1) {
        int num_fds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (num_fds == -1) {
            perror("epoll_wait failed");
            close(server_fd);
            close(epoll_fd);
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < num_fds; i++) {
            if (events[i].data.fd == server_fd) {
                // 新しいクライアントを受け入れる
                if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
                    perror("accept failed");
                    continue;
                }

                printf("Player connected\n");

                // プレイヤー情報を初期化
                players[connected_players].health = 100;
                players[connected_players].socket = new_socket;
                connected_players++;

                event.events = EPOLLIN;
                event.data.fd = new_socket;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_socket, &event) == -1) {
                    perror("epoll_ctl failed");
                    exit(EXIT_FAILURE);
                }

                if (connected_players == 2) {
                    printf("Both players connected, starting the battle!\n");
                }
            } else {
                // クライアントからデータを受信
                int client_socket = events[i].data.fd;
                int bytes_read = read(client_socket, buffer, BUFFER_SIZE);
                if (bytes_read <= 0) {
                    perror("read failed or client disconnected");
                    close(client_socket);
                    continue;
                }

                buffer[bytes_read] = '\0'; // 受信したデータを文字列にする

                if (connected_players == 2) {
                    // 両方のプレイヤーがコマンドを送信するまで待機
                    static char cmd1[BUFFER_SIZE], cmd2[BUFFER_SIZE];
                    static int cmd_count = 0;

                    if (client_socket == players[0].socket) {
                        strncpy(cmd1, buffer, BUFFER_SIZE);
                        cmd_count++;
                    } else if (client_socket == players[1].socket) {
                        strncpy(cmd2, buffer, BUFFER_SIZE);
                        cmd_count++;
                    }

                    if (cmd_count == 2) {
                        // 両方のコマンドが揃ったらバトルを処理
                        handle_battle(&players[0], &players[1], cmd1, cmd2);
                        cmd_count = 0;
                    }
                }
            }
        }
    }

    close(server_fd);
    return 0;
}