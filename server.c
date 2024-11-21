#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <stdarg.h>
#include <time.h>
#include <sys/timerfd.h>

#define PORT 50000
#define MAX_EVENTS 3
#define BUFFER_SIZE 1024
#define MAP_WIDTH 40
#define MAP_HEIGHT 15
#define MAX_PLAYERS 4
#define MAX_ITEMS 10
#define ITEM_SPAWN_INTERVAL 5
#define MAX_LOG_LINES 5
#define INITIAL_HP 100

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
    int isAlive;
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
    int used_symbols[MAX_PLAYERS];
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
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        map->players[i].isAlive = 0;
        map->players[i].health = 0;
    }
    map->player_count = 0;

    // アイテム情報の初期化
    map->item_count = 0;

    // ログの初期化
    for (int i = 0; i < MAX_LOG_LINES; i++)
    {
        map->log[i][0] = '\0';
    }

    // シンボル使用状態の初期化
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        map->used_symbols[i] = 0; // 0: 未使用, 1: 使用中
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
    } while (map->grid[y][x] != '.');

    Item item = {
        .x = x,
        .y = y,
        .type = "HASD"[rand() % 4],
        .value = 1 + rand() % 10};

    // items[0]に一個目のアイテムが入るので、インクリメントはこれでよい
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

void update_game_map(GameMap *map, int player_id, char direction)
{
    Player *p = &map->players[player_id];
    int new_x = p->x;
    int new_y = p->y;


    // HPが0以下の場合は移動不可
    if (p->health <= 0)
    {
        if (p->isAlive)
            p->isAlive = 0;
        
        if(direction){
            p->health = INITIAL_HP;
            int x, y;
            do
            {
                x = rand() % (MAP_WIDTH - 2) + 1;
                y = rand() % (MAP_HEIGHT - 2) + 1;
            } while (map->grid[y][x] != '.');
            p->x = x;
            p->y = y;
            p->isAlive = 1;
            p->attack = 10;
            p->defense = 5;
            p->speed = 5;
        }
        return;
    }

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

    

    if (map->grid[new_y][new_x] == '#')
        return;

    // プレイヤーとの衝突チェック
    for (int i = 0; i < map->player_count; i++)
    {
        // 衝突するとき
        if (i != player_id && map->players[i].isAlive && map->players[i].x == new_x && map->players[i].y == new_y)
        {
            // 命中率計算
            int hit_chance = 75 + (((float)p->speed - (float)map->players[i].speed));
            if (hit_chance > 100)
                hit_chance = 100;
            if (hit_chance < 25)
                hit_chance = 25;

            // 乱数で命中判定
            if ((rand() % 100) < hit_chance)
            {
                // 攻撃処理
                int damage = p->attack - map->players[i].defense;
                if (damage < 0)
                    damage = 1;
                map->players[i].health -= damage;
                add_log(map, "Player %c attacked Player %c for %d damage! (Hit: %d%%)",
                        p->symbol, map->players[i].symbol, damage, hit_chance);
            }
            else
            {
                add_log(map, "Player %c missed attack on Player %c! (Hit: %d%%)",
                        p->symbol, map->players[i].symbol, hit_chance);
            }

            // 反撃処理も同様に命中率を計算
            hit_chance = 75 + (((float)map->players[i].speed - (float)p->speed));
            if (hit_chance > 100)
                hit_chance = 100;
            if (hit_chance < 25)
                hit_chance = 25;
            if ((rand() % 100) < hit_chance)
            {
                int counter_damage = map->players[i].attack - p->defense;
                if (counter_damage < 0)
                    counter_damage = 1;
                p->health -= counter_damage;
                add_log(map, "Player %c counter-attacked Player %c for %d damage! (Hit: %d%%)",
                        map->players[i].symbol, p->symbol, counter_damage, hit_chance);
            }
            else
            {
                add_log(map, "Player %c missed counter-attack on Player %c! (Hit: %d%%)",
                        map->players[i].symbol, p->symbol, hit_chance);
            }

            // 自分のhealthが0以下になったら死亡
            if(p->health <= 0){
                p->isAlive = 0;
            }

            //自分以外の誰かのhealthが0以下になったら死亡
            if(map->players[i].health <= 0){
                map->players[i].isAlive = 0;
            }
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
                // HPが最大値を超えないようにする
                if (p->health + map->items[i].value > INITIAL_HP)
                {
                    p->health = INITIAL_HP;
                    add_log(map, "Player %c got HP+%d but it was full!", p->symbol, map->items[i].value);
                }
                else
                {
                    p->health += map->items[i].value;
                    add_log(map, "Player %c got HP+%d!", p->symbol, map->items[i].value);
                }
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
            break;
        }
    }

    p->x = new_x;
    p->y = new_y;
}

void create_send_string(GameMap *map, char *buffer, int viewer_id)
{
    char temp_grid[MAP_HEIGHT][MAP_WIDTH];
    memcpy(temp_grid, map->grid, sizeof(temp_grid));

    // プレイヤーのHP状態に応じてエスケープシークエンスで画面色を変更
    Player *viewer = &map->players[viewer_id];
    char screen_color[10] = "\033[0m";
    if (viewer->health <= 0)
        strcpy(screen_color, "\033[31m"); // 赤（ゲームオーバー）
    else if (viewer->health <= INITIAL_HP * 0.2)
        strcpy(screen_color, "\033[31m"); // 赤
    else if (viewer->health <= INITIAL_HP * 0.4)
        strcpy(screen_color, "\033[33m"); // 黄

    int pos = 0;
    pos += sprintf(&buffer[pos], "%s", screen_color);

    // アイテムを配置
    for (int i = 0; i < map->item_count; i++)
    {
        temp_grid[map->items[i].y][map->items[i].x] = map->items[i].type;
    }

    // プレイヤーを配置
    for (int i = 0; i < map->player_count; i++)
    {
        if(map->players[i].isAlive){
             Player *p = &map->players[i];
            temp_grid[p->y][p->x] = p->symbol;
        }
    }

    // マップの描画
    for (int y = 0; y < MAP_HEIGHT; y++)
    {
        for (int x = 0; x < MAP_WIDTH; x++)
        {
            buffer[pos++] = temp_grid[y][x];
        }
        buffer[pos++] = '\n';
    }

    // 自分のステータスのみ表示
    Player *p = &map->players[viewer_id];
    char hp_color[10];
    if (p->health <= 0)
    {
        pos += sprintf(&buffer[pos], "\nGAME OVER! Press ANY Key to Restart\n");
    }
    else
    {
        if (p->health <= INITIAL_HP * 0.2)
            strcpy(hp_color, "\033[31m");
        else if (p->health <= INITIAL_HP * 0.4)
            strcpy(hp_color, "\033[33m");
        else
            strcpy(hp_color, "\033[0m");

        pos += sprintf(&buffer[pos], "Player %c: %sHP:%d ATK:%d DEF:%d SPD:%d\033[0m\n",
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

    pos += sprintf(&buffer[pos], "\033[0m"); // 色をリセット
    buffer[pos] = '\0';
}

int main()
{
    srand(time(NULL)); // 乱数の初期化

    int server_fd, new_socket, epoll_fd;
    struct sockaddr_in address;
    struct epoll_event event, events[MAX_EVENTS];
    char buffer[BUFFER_SIZE];

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

    // リッスン
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

    // メインループ
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

        // サーバーに来たイベントを処理
        for (int i = 0; i < num_fds; i++)
        {
            // サーバーに接続した場合
            if (events[i].data.fd == server_fd)
            {
                socklen_t addrlen = sizeof(struct sockaddr_in);
                if ((new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0)
                {
                    perror("accept failed");
                    continue;
                }

                // プレイヤー情報を初期化
                int p_num = -1;
                for (int i = 0; i < MAX_PLAYERS; i++)
                {
                    if (!game_map.players[i].isAlive) // used_symbolsではなく、isAliveで判定
                    {
                        p_num = i;
                        break;
                    }
                }

                if (p_num == -1)
                {
                    printf("Server is full\n");
                    char *full_msg = "Server is full. Please try again later.\n";
                    send(new_socket, full_msg, strlen(full_msg), 0);
                    close(new_socket);
                    continue;
                }

                printf("Player %d connected (symbol: %c)\n", p_num + 1, '1' + p_num);

                // プレイヤーの初期化
                game_map.players[p_num].health = INITIAL_HP;
                game_map.players[p_num].socket = new_socket;
                int x, y;
                do
                {
                    x = rand() % (MAP_WIDTH - 2) + 1;
                    y = rand() % (MAP_HEIGHT - 2) + 1;
                } while (game_map.grid[y][x] != '.');

                game_map.players[p_num].x = x;
                game_map.players[p_num].y = y;
                game_map.players[p_num].isAlive = 1;
                game_map.players[p_num].symbol = '1' + p_num; // シンボルとプレイヤー番号を統一
                game_map.players[p_num].attack = 10;
                game_map.players[p_num].defense = 5;
                game_map.players[p_num].speed = 5;

                if (p_num >= game_map.player_count)
                {
                    game_map.player_count = p_num + 1;
                }

                // まず新規プレイヤーにマップを送信
                char map_str[BUFFER_SIZE];
                create_send_string(&game_map, map_str, p_num);
                send(new_socket, map_str, strlen(map_str), 0);

                // 次に他のプレイヤーに更新されたマップを送信
                for (int j = 0; j < game_map.player_count; j++)
                {
                    if (j != p_num && game_map.players[j].isAlive)
                    {
                        create_send_string(&game_map, map_str, j);
                        send(game_map.players[j].socket, map_str, strlen(map_str), 0);
                    }
                }

                event.events = EPOLLIN;
                event.data.fd = new_socket;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_socket, &event) == -1)
                {
                    perror("epoll_ctl failed");
                    exit(EXIT_FAILURE);
                }

                connected_players++;
            }
            // クライアントからデータを受信した場合
            else
            {
                // クライアントからデータを受信
                int client_socket = events[i].data.fd;
                int bytes_read = read(client_socket, buffer, BUFFER_SIZE);
                if (bytes_read <= 0)
                {
                    printf("Player disconnected\n");

                    // 切断したプレイヤーを特定して削除
                    for (int j = 0; j < MAX_PLAYERS; j++)
                    {
                        if (game_map.players[j].socket == client_socket)
                        {
                            // シンボルを解放
                            int symbol_index = game_map.players[j].symbol - '1';
                            game_map.used_symbols[symbol_index] = 0;

                            // プレイヤー情報をリセット
                            game_map.players[j].isAlive = 0;
                            game_map.players[j].health = 0;
                            game_map.players[j].socket = 0;
                            game_map.players[j].symbol = 0;

                            // プレイヤーカウントの更新
                            connected_players--;

                            // 最大のプレイヤー番号を探して player_count を更新
                            game_map.player_count = 0;
                            for (int k = 0; k < MAX_PLAYERS; k++)
                            {
                                if (game_map.players[k].isAlive)
                                {
                                    game_map.player_count = k + 1;
                                }
                            }
                            break;
                        }
                    }

                    close(client_socket);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_socket, NULL);

                    // 残りのプレイヤーに更新されたマップを送信
                    char map_str[BUFFER_SIZE];
                    for (int j = 0; j < game_map.player_count; j++)
                    {
                        create_send_string(&game_map, map_str, j);
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
                    update_game_map(&game_map, player_id, buffer[0]);

                    // 全プレイヤーに更新されたマップを送信
                    char map_str[BUFFER_SIZE];
                    for (int j = 0; j < game_map.player_count; j++)
                    {
                        create_send_string(&game_map, map_str, j);
                        send(game_map.players[j].socket, map_str, strlen(map_str), 0);
                    }
                }
            }
        }

        // ITEM_SPAWN_INTERVAL秒ごとにアイテムを生成
        time_t current_time = time(NULL);
        if (current_time - last_item_spawn >= ITEM_SPAWN_INTERVAL)
        {
            spawn_random_item(&game_map);
            last_item_spawn = current_time;

            // 全プレイヤーに更新されたマップを送信
            char map_str[BUFFER_SIZE];
            for (int i = 0; i < game_map.player_count; i++)
            {
                create_send_string(&game_map, map_str, i);
                send(game_map.players[i].socket, map_str, strlen(map_str), 0);
            }
        }
    }

    close(server_fd);
    return 0;
}