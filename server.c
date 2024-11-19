#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <stdarg.h>
#include <time.h>

#define PORT 50000
#define MAX_EVENTS 3
#define BUFFER_SIZE 1024
#define MAP_WIDTH 20
#define MAP_HEIGHT 15
#define MAX_PLAYERS 4
#define MAX_ITEMS 10
#define MAX_LOG_LINES 5
#define INITIAL_HP 100
#define DAMAGE_ON_COLLISION 10

typedef struct
{
    int x;
    int y;
    char type; // 'H':HP, 'A':攻撃力, 'S':素早さ, 'D':防御力
    int value;
} Item;

typedef struct
{
    int health;
    int socket;
    int x;
    int y;
    char symbol;
    int attack;
    int defense;
    int speed;
} Player;

typedef struct
{
    char grid[MAP_HEIGHT][MAP_WIDTH];
    Player players[MAX_PLAYERS];
    Item items[MAX_ITEMS];
    int player_count;
    int item_count;
    char log[MAX_LOG_LINES][100];
    int log_count;
} GameMap;

void init_map(GameMap *map)
{
    // グリッドの初期化
    for (int y = 0; y < MAP_HEIGHT; y++)
    {
        for (int x = 0; x < MAP_WIDTH; x++)
        {
            if (y == 0 || y == MAP_HEIGHT - 1 || x == 0 || x == MAP_WIDTH - 1)
            {
                map->grid[y][x] = '#';
            }
            else
            {
                map->grid[y][x] = '.';
            }
        }
    }

    // プレイヤー情報の初期化
    map->player_count = 0;

    // アイテム情報の初期化
    map->item_count = 0;

    // ログの初期化
    for (int i = 0; i < MAX_LOG_LINES; i++)
    {
        map->log[i][0] = '\0';
    }
}

void spawn_random_item(GameMap *map)
{
    if (map->item_count >= MAX_ITEMS)
        return;

    int x, y;
    do
    {
        x = rand() % (MAP_WIDTH - 2) + 1;
        y = rand() % (MAP_HEIGHT - 2) + 1;
    } while (map->grid[y][x] != '.' ||
             (map->items[map->item_count].x == x && map->items[map->item_count].y == y));

    Item item = {
        .x = x,
        .y = y,
        .type = "HASD"[rand() % 4],
        .value = 10 + rand() % 20};

    map->items[map->item_count++] = item;
}

void add_log(GameMap *map, const char *format, ...)
{
    va_list args;
    va_start(args, format);

    // ログをずらす
    for (int i = 0; i < MAX_LOG_LINES - 1; i++)
    {
        strcpy(map->log[i], map->log[i + 1]);
    }

    // 新しいログを追加
    vsnprintf(map->log[MAX_LOG_LINES - 1], 100, format, args);
    va_end(args);
}

void update_player_position(GameMap *map, int player_id, char direction)
{
    Player *p = &map->players[player_id];
    int new_x = p->x;
    int new_y = p->y;

    switch (direction)
    {
    case 'w':
        new_y--;
        break;
    case 's':
        new_y++;
        break;
    case 'a':
        new_x--;
        break;
    case 'd':
        new_x++;
        break;
    }

    // 壁との衝突チェック
    if (map->grid[new_y][new_x] == '#')
        return;

    // プレイヤーとの衝突チェック
    for (int i = 0; i < map->player_count; i++)
    {
        if (i != player_id && map->players[i].x == new_x && map->players[i].y == new_y)
        {
            int damage = DAMAGE_ON_COLLISION;
            map->players[i].health -= damage;
            add_log(map, "Player %c attacked Player %c for %d damage!",
                    p->symbol, map->players[i].symbol, damage);
            return;
        }
    }

    // アイテム取得チェック
    for (int i = 0; i < map->item_count; i++)
    {
        if (map->items[i].x == new_x && map->items[i].y == new_y)
        {
            switch (map->items[i].type)
            {
            case 'H':
                p->health += map->items[i].value;
                add_log(map, "Player %c got HP+%d!", p->symbol, map->items[i].value);
                break;
            case 'A':
                p->attack += map->items[i].value;
                add_log(map, "Player %c got ATK+%d!", p->symbol, map->items[i].value);
                break;
            case 'S':
                p->speed += map->items[i].value;
                add_log(map, "Player %c got SPD+%d!", p->symbol, map->items[i].value);
                break;
            case 'D':
                p->defense += map->items[i].value;
                add_log(map, "Player %c got DEF+%d!", p->symbol, map->items[i].value);
                break;
            }
            // アイテムを削除
            for (int j = i; j < map->item_count - 1; j++)
            {
                map->items[j] = map->items[j + 1];
            }
            map->item_count--;
            spawn_random_item(map);
            break;
        }
    }

    p->x = new_x;
    p->y = new_y;
}

void create_map_string(GameMap *map, char *buffer)
{
    char temp_grid[MAP_HEIGHT][MAP_WIDTH];
    memcpy(temp_grid, map->grid, sizeof(temp_grid));

    // アイテムを配置
    for (int i = 0; i < map->item_count; i++)
    {
        temp_grid[map->items[i].y][map->items[i].x] = map->items[i].type;
    }

    // プレイヤーを配置
    for (int i = 0; i < map->player_count; i++)
    {
        Player *p = &map->players[i];
        temp_grid[p->y][p->x] = p->symbol;
    }

    int pos = 0;
    // マップの描画
    for (int y = 0; y < MAP_HEIGHT; y++)
    {
        for (int x = 0; x < MAP_WIDTH; x++)
        {
            buffer[pos++] = temp_grid[y][x];
        }
        buffer[pos++] = '\n';
    }

    // ステータス情報の追加
    for (int i = 0; i < map->player_count; i++)
    {
        Player *p = &map->players[i];
        char hp_color[10];
        if (p->health <= INITIAL_HP * 0.2)
            strcpy(hp_color, "\033[31m"); // 赤
        else if (p->health <= INITIAL_HP * 0.4)
            strcpy(hp_color, "\033[33m"); // 黄
        else
            strcpy(hp_color, "\033[0m"); // デフォルト

        pos += sprintf(&buffer[pos], "Player %c: %sHP:%d\033[0m ATK:%d DEF:%d SPD:%d\n",
                       p->symbol, hp_color, p->health, p->attack, p->defense, p->speed);
    }

    // ログの追加
    pos += sprintf(&buffer[pos], "\n--- Log ---\n");
    for (int i = 0; i < MAX_LOG_LINES; i++)
    {
        if (map->log[i][0] != '\0')
        {
            pos += sprintf(&buffer[pos], "%s\n", map->log[i]);
        }
    }

    buffer[pos] = '\0';
}

int main()
{
    srand(time(NULL)); // 乱数の初期化

    int server_fd, new_socket, epoll_fd;
    struct sockaddr_in address;
    struct epoll_event event, events[MAX_EVENTS];
    char buffer[BUFFER_SIZE];

    Player players[MAX_EVENTS];
    int connected_players = 0;

    GameMap game_map;
    init_map(&game_map);

    // ソケット作成
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket failed");
        exit(1);
    }

    // ソケットオプション設定
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
    {
        perror("setsockopt failed");
        close(server_fd);
        exit(1);
    }

    // アドレス作成
    memset(&address, 0, sizeof(struct sockaddr_in));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(PORT);

    // ソケットをバインド
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(struct sockaddr_in)) < 0)
    {
        perror("bind failed");
        close(server_fd);
        exit(1);
    }

    if (listen(server_fd, 5) < 0)
    {
        perror("listen failed");
        close(server_fd);
        exit(1);
    }

    // epoll作成
    if ((epoll_fd = epoll_create1(0)) == -1)
    {
        perror("epoll_create1 failed");
        exit(EXIT_FAILURE);
    }

    event.events = EPOLLIN;
    event.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1)
    {
        perror("epoll_ctl failed");
        exit(EXIT_FAILURE);
    }

    printf("server is opened at PORT : %d \n", PORT);
    printf("Waiting for players...\n");

    time_t last_item_spawn = time(NULL);

    while (1)
    {
        int num_fds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (num_fds == -1)
        {
            perror("epoll_wait failed");
            close(server_fd);
            close(epoll_fd);
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < num_fds; i++)
        {
            if (events[i].data.fd == server_fd)
            {
                socklen_t addrlen = sizeof(struct sockaddr_in);
                if ((new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0)
                {
                    perror("accept failed");
                    continue;
                }

                printf("Player %d connected\n", connected_players + 1);

                // プレイヤー情報を初期化
                players[connected_players].health = INITIAL_HP;
                players[connected_players].socket = new_socket;
                players[connected_players].x = 1 + connected_players * 2;
                players[connected_players].y = 1;
                players[connected_players].symbol = '1' + connected_players;
                players[connected_players].attack = 10; // 初期攻撃力
                players[connected_players].defense = 5; // 初期防御力
                players[connected_players].speed = 5;   // 初期素早さ
                game_map.players[connected_players] = players[connected_players];
                game_map.player_count++;

                // 新しいプレイヤーに現在のマップを送信
                char map_str[BUFFER_SIZE];
                create_map_string(&game_map, map_str);
                send(new_socket, map_str, strlen(map_str), 0);

                event.events = EPOLLIN;
                event.data.fd = new_socket;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_socket, &event) == -1)
                {
                    perror("epoll_ctl failed");
                    exit(EXIT_FAILURE);
                }

                connected_players++;
            }
            else
            {
                // クライアントからデータを受信
                int client_socket = events[i].data.fd;
                int bytes_read = read(client_socket, buffer, BUFFER_SIZE);
                if (bytes_read <= 0)
                {
                    printf("Player disconnected\n");

                    // 切断したプレイヤーを特定して削除
                    for (int j = 0; j < game_map.player_count; j++)
                    {
                        if (game_map.players[j].socket == client_socket)
                        {
                            // プレイヤーを配列から削除し、後ろのプレイヤーを前に詰める
                            for (int k = j; k < game_map.player_count - 1; k++)
                            {
                                game_map.players[k] = game_map.players[k + 1];
                            }
                            game_map.player_count--;
                            connected_players--;
                            break;
                        }
                    }

                    close(client_socket);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_socket, NULL);

                    // 残りのプレイヤーに更新されたマップを送信
                    char map_str[BUFFER_SIZE];
                    create_map_string(&game_map, map_str);
                    for (int j = 0; j < game_map.player_count; j++)
                    {
                        send(game_map.players[j].socket, map_str, strlen(map_str), 0);
                    }
                    continue;
                }

                buffer[bytes_read] = '\0';

                // プレイヤーIDを特定
                int player_id = -1;
                for (int j = 0; j < game_map.player_count; j++)
                {
                    if (game_map.players[j].socket == client_socket)
                    {
                        player_id = j;
                        break;
                    }
                }

                if (player_id >= 0)
                {
                    update_player_position(&game_map, player_id, buffer[0]);

                    // 全プレイヤーに更新されたマップを送信
                    char map_str[BUFFER_SIZE];
                    create_map_string(&game_map, map_str);
                    for (int j = 0; j < game_map.player_count; j++)
                    {
                        send(game_map.players[j].socket, map_str, strlen(map_str), 0);
                    }
                }
            }
        }

        // 10秒ごとにアイテムを生成
        time_t current_time = time(NULL);
        if (current_time - last_item_spawn >= 10)
        {
            spawn_random_item(&game_map);
            last_item_spawn = current_time;
        }
    }

    close(server_fd);
    return 0;
}