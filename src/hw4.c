#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define PORT1 2201
#define PORT2 2202
#define BUFFER_SIZE 1024

typedef struct {
    int width;
    int height;
    int *player1_board; 
    int *player2_board; 
    int player1_ships_remaining;
    int player2_ships_remaining;
} GameState;

GameState game_state;
int client1_fd;
int client2_fd;

pthread_mutex_t turn_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t turn_condition = PTHREAD_COND_INITIALIZER;
int num_of_player_turn = 1;

int pieces[7][4][4][2] =  
{
    // Yellow-piece (index 0)
    {
        {{0, 0}, {0, 1}, {1, 0}, {1, 1}},  
        {{0, 0}, {0, 1}, {1, 0}, {1, 1}},   
        {{0, 0}, {0, 1}, {1, 0}, {1, 1}},   
        {{0, 0}, {0, 1}, {1, 0}, {1, 1}} 
    },
    // I-piece (index 1)
    {
        {{0, 0}, {1, 0}, {2, 0}, {3, 0}}, 
        {{0, 0}, {0, 1}, {0, 2}, {0, 3}}, 
        {{0, 0}, {1, 0}, {2, 0}, {3, 0}}, 
        {{0, 0}, {0, 1}, {0, 2}, {0, 3}}  
    },
    // Red-piece (index 2)
    {
        {{1, 0}, {1, 1}, {0, 1}, {0, 2}}, 
        {{0, 0}, {1, 0}, {1, 1}, {2, 1}},
        {{1, 0}, {1, 1}, {0, 1}, {0, 2}}, 
        {{0, 0}, {1, 0}, {1, 1}, {2, 1}}
    },
    // L-piece (index 3)
    {
        {{0, 0}, {1, 0}, {2, 0}, {2, 1}}, 
        {{0, 0}, {0, 1}, {0, 2}, {1, 0}}, 
        {{0, 0}, {0, 1}, {1, 1}, {2, 1}},
        {{1, 0}, {1, 1}, {1, 2}, {0, 2}}  
    },
    // Z-piece (index 4)
    {
        {{0, 0}, {0, 1}, {1, 1}, {1, 2}},
        {{1, 0}, {2, 0}, {1, 1}, {0, 1}}, 
        {{0, 0}, {0, 1}, {1, 1}, {1, 2}},
        {{1, 0}, {2, 0}, {1, 1}, {0, 1}}  
    },
    // Pink-piece (index 5)
    {
        {{2, 0}, {2, 1}, {1, 1}, {0, 1}},
        {{0, 0}, {1, 0}, {1, 1}, {1, 2}}, 
        {{0, 0}, {0, 1}, {1, 0}, {2, 0}},
        {{0, 0}, {0, 1}, {0, 2}, {1, 2}}  
    },
    // T-piece (index 6)
    {
        {{0, 0}, {0, 1}, {0, 2}, {1, 1}},
        {{1, 0}, {1, 1}, {0, 1}, {2, 1}}, 
        {{1, 0}, {1, 1}, {0, 1}, {1, 2}},
        {{0, 0}, {1, 0}, {2, 0}, {1, 1}}  
    }
};

void send_error(int client_fd, int errorNum)
{
    char errorMSG[BUFFER_SIZE];
    char* ext = NULL;

    if(errorNum == 100)
    {
        ext = "100: Invalid packet type (Expected Begin packet)";
    }
    if(errorNum == 101)
    {
        ext = "101: Invalid packet type (Expected Initialize packet)";
    }
    if(errorNum == 102)
    {
        ext = "102: Invalid packet type (Expected Shoot/Query/Forfeit)";
    }

    if(errorNum == 200)
    {
        ext = "200: Invalid Begin packet (invalid number of parameters or parameter out of range)";
    }
    if(errorNum == 201)
    {
        ext = "201: Invalid Initialize packet (invalid number of parameters)";
    }
    if(errorNum == 202)
    {
        ext = "202: Invalid Shoot packet (invalid number of parameters)";
    }

    if(errorNum == 300)
    {
        ext = "300: Invalid Initialize packet (shape out of range)";
    }
    if(errorNum == 301)
    {
        ext = "301: Invalid Initialize packet (rotation out of range)";
    }
    if(errorNum == 302)
    {
        ext = "302: Invalid Initialize packet (ship doesn't fit in game board)";
    }
    if(errorNum == 303)
    {
        ext = "303: Invalid Initialize packet (ships overlap)";
    }

    if(errorNum == 400)
    {
        ext = "400: Invalid Shoot packet (cell not in game board)";
    }
    if(errorNum == 401)
    {
        ext = "401: Invalid Shoot packet (cell already guessed)";
    }

    snprintf(errorMSG, sizeof(errorMSG), "Error: %s\n", ext);
    send(client_fd, errorMSG, strlen(errorMSG), 0);
    
}

void print_board(int *board, int width, int height) 
{
    printf("\nGame Board:\n");
    for (int row = 0; row < height; row++) 
    {
        for (int col = 0; col < width; col++) 
        {
            int cell = board[row * width + col];
            printf("%d ", cell);  
        }
        printf("\n");
    }
    printf("\n");
}

//handling begin packet
void handle_begin_packet(int client_fd, char* buffer)
{
    int width;
    int height;

    if (sscanf(buffer, "B %d %d", &width, &height) == 2) 
    {
        // Player 1 sent the dimensions & adjusting w and h in game_state
        printf("[Server] Player 1 set board dimensions: %d x %d\n", width, height);
        game_state.width = width;
        game_state.height = height;

        // Allocate memory for player boards (initialize them to 0)
        game_state.player1_board = (int*)malloc(width * height * sizeof(int));
        game_state.player2_board = (int*)malloc(width * height * sizeof(int));

        if (!game_state.player1_board || !game_state.player2_board) 
        {
            perror("Failed to allocate memory for game boards.");
            send_error(client_fd, 200);
            exit(EXIT_FAILURE);
        }
        memset(game_state.player1_board, 0, width * height * sizeof(int)); // Initialize Player 1's board to 0
        memset(game_state.player2_board, 0, width * height * sizeof(int)); // Initialize Player 2's board to 0

        game_state.player1_ships_remaining = 5; 
        game_state.player2_ships_remaining = 5;  

        // Send a response to Player 1 confirming the board dimensions were set
        snprintf(buffer, BUFFER_SIZE, "ACK Board dimensions set to %dx%d", width, height);
        send(client_fd, buffer, strlen(buffer), 0);
    }
    else 
    {
        // Player 2 just wants to join, no dimensions, so confirm join
        printf("[Server] Player 2 joined the game.\n");

        // Send acknowledgment to Player 2
        snprintf(buffer, BUFFER_SIZE, "ACK You have joined the game");
        send(client_fd, buffer, strlen(buffer), 0);
    }
}

void send_halt_packet(int client_fd, int player) 
{
    char halt_packet[] = "H";  // Halt packet
    int nbytes = send(client_fd, halt_packet, strlen(halt_packet), 0);
    if (nbytes == -1) 
    {
        perror("Error sending Halt packet");
    } 
    else 
    {
        printf("[Server] Sent Halt packet to client %d\n", player);
    }
}

void handle_forfeit_packet(int forfeiting_client_fd, int player) 
{
    printf("[Server] Player forfeited, sending Halt packet to both clients.\n");

    send_halt_packet(client1_fd, player);
    send_halt_packet(client2_fd, player);

    close(client1_fd);
    close(client2_fd);

    printf("[Server] Both connections closed due to forfeit.\n");
}




int is_valid_piece(int type, int rotation, int col, int row, int player_id, int* player_board) 
{
    int *board = (player_id == 1) ? game_state.player1_board : game_state.player2_board;

    for (int i = 0; i < 4; i++) 
    {
        // printf("piece_shape[i][0] : %d\n", pieces[type][rotation][i][0]);
        // printf("piece_shape[i][1] : %d\n", pieces[type][rotation][i][1]);
        int x = row + pieces[type][rotation][i][0];
        int y = col + pieces[type][rotation][i][1];

        // Checking for out of bounds
        if (x < 0 || x >= game_state.width || y < 0 || y >= game_state.width) 
        {
            printf("Out of bounds: X -> %d Y -> %d\n", x, y);
            if(player_id == 1)
            {
                memset(game_state.player1_board, 0, game_state.width * game_state.height * sizeof(int));
            }
            if(player_id == 2)
            {
                memset(game_state.player2_board, 0, game_state.width * game_state.height * sizeof(int));
            }
            return 0;
        }

        int pos = x * game_state.width + y;
        // Checking for overlap
        if (board[pos] != 0) 
        {
            printf("[Debug] Invalid piece placement: Overlap at position %d\n", pos);
            if(player_id == 1)
            {
                memset(game_state.player1_board, 0, game_state.width * game_state.height * sizeof(int));
            }
            if(player_id == 2)
            {
                memset(game_state.player2_board, 0, game_state.width * game_state.height * sizeof(int));
            }
            return 0;
        }
    }
    return 1;
}

void place_piece(int type, int rotation, int col, int row, int* player_board, int shipNum) 
{
    //int (*piece_shape)[4][2] = pieces[type][rotation];
    for (int i = 0; i < 4; i++) 
    {
        int x = row + pieces[type][rotation][i][0];
        int y = col + pieces[type][rotation][i][1];
        int pos = x * game_state.width + y;


        printf("Pos: %d\n", pos);
        player_board[pos] = shipNum; 
        //printf("[Debug] Placed piece segment at board[%d] (%d, %d)\n", pos, x, y);
    }
}

void handle_initialize_packet(int client_fd, char *buffer, int* player_board) 
{
    int shipInitializeData[20];  
    int num_values = sscanf(buffer, "I %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
    &shipInitializeData[0], &shipInitializeData[1], &shipInitializeData[2], &shipInitializeData[3], &shipInitializeData[4], &shipInitializeData[5], &shipInitializeData[6], &shipInitializeData[7],
    &shipInitializeData[8], &shipInitializeData[9], &shipInitializeData[10], &shipInitializeData[11], &shipInitializeData[12], &shipInitializeData[13], &shipInitializeData[14], &shipInitializeData[15],
    &shipInitializeData[16], &shipInitializeData[17], &shipInitializeData[18], &shipInitializeData[19]);

    if (num_values != 20) 
    {
        printf("[Server] Invalid initialize packet format.\n");
        return;
    }

    int player_id = (client_fd == client1_fd) ? 1 : 2;

    for (int i = 0; i < 5; i++) 
    {
        int type = shipInitializeData[i * 4];
        int rotation = shipInitializeData[i * 4 + 1];
        int col = shipInitializeData[i * 4 + 2];
        int row = shipInitializeData[i * 4 + 3];
        printf("Type : %d Rot: %d Col: %d Row: %d", type, rotation, col, row);

        // Validate piece placement
        if (!is_valid_piece(type, rotation, col, row, player_id, player_board)) 
        {
            printf("[Server] Invalid piece placement for player %d at piece %d.\n", player_id, i + 1);
            return;
        }

        // Place the piece on the player's board
        place_piece(type, rotation, col, row, player_board, (i+1));
    }
    printf("[Server] Player %d's pieces initialized successfully.\n", player_id);
}

void handle_shoot_packet(int client_fd, char *buffer, int* opponents_board, int player)
{
    int row;
    int col;
    printf("Buffer is: %s", buffer);
    sscanf(buffer, "S %d %d", &row, &col);

    int player_id = (client_fd == client1_fd) ? 1 : 2;

    if(row >= game_state.height || col >= game_state.width || row < 0 || col < 0)
    {
        printf("[Server] Player %d shot out of bounds\n", player_id);
        send_error(client_fd, 400);
        return;
    }

    int pos = row * game_state.width + col;
    if(opponents_board[pos] > 0)
    {
        int curr_ship_num = opponents_board[pos];
        opponents_board[pos] = -2;
        printf("[Server] Player %d hit opponents board at postion (%d,%d)\n", player, row, col);
        int sunk = 1;
        for (int i = 0; i < game_state.width * game_state.height; i++)
        {
            if (opponents_board[i] == curr_ship_num) 
            {
                sunk = 0; 
                break;
            }
        }

        if(sunk == 1)
        {
            printf("[Server] Player %d sunk opponent's ship\n", player);
            if(player == 1)
            {
                game_state.player2_ships_remaining--;
            }
            else
            {
                game_state.player1_ships_remaining--;
            }
        }
    }
    else if(opponents_board[pos] == 0)
    {
        opponents_board[pos] = -1;
        printf("[Server] Player %d missed opponents board at postion (%d,%d)\n", player, row, col);
    }
    else
    {
        printf("[Server] Player %d already hit postion (%d,%d)\n", player, row, col);
        send_error(client_fd, 401);
    }

}

void handle_query_packet(int client_fd, int* opponents_board, int player)
{
    char* query[BUFFER_SIZE] = {0};
    pthread_mutex_lock(&turn_mutex);
    int len = game_state.width * game_state.height;

    for(int i = 0; i < len; i++)
    {
        int xPos = i / game_state.width;
        int yPos = i % game_state.width;
        if(opponents_board[i] == -1)
        {
            snprintf(query + strlen(query), BUFFER_SIZE - strlen(query), "Miss at (%d,%d)\n", xPos, yPos);
            printf("Query: %s", query);
        }
        if(opponents_board[i] == -2)
        {
            snprintf(query + strlen(query), BUFFER_SIZE - strlen(query), "Hit at (%d,%d)\n", xPos, yPos);
            printf("Query: %s", query);
        }
    }

    if(player == 1)
    {
        snprintf(query + strlen(query), BUFFER_SIZE - strlen(query), "Opponent's Ships Remaining: %d\n", game_state.player2_ships_remaining);

    }
    if(player == 2)
    {
        snprintf(query + strlen(query), BUFFER_SIZE - strlen(query), "Opponent's Ships Remaining: %d\n", game_state.player1_ships_remaining);

    }
    pthread_mutex_unlock(&turn_mutex);

    send(client_fd, query, strlen(query), 0);
    printf("[Server] Sent query response to Player %d:\n%s", player, query);
    printf("Query: %s", query);

}

void* handle_client(void* sockFD) 
{
    int client_fd = *((int*)sockFD);
    char buffer[BUFFER_SIZE] = {0};

    int player = (client_fd == client1_fd) ? 1 : 2;
    int* player_board = (player == 1) ? game_state.player1_board : game_state.player2_board;

    while(1) 
    {
        // Wait for this player's turn
        pthread_mutex_lock(&turn_mutex);
        while (num_of_player_turn != player) 
        {
            pthread_cond_wait(&turn_condition, &turn_mutex);
        }
        pthread_mutex_unlock(&turn_mutex);

        memset(buffer, 0, BUFFER_SIZE);
        int nbytes = read(client_fd, buffer, BUFFER_SIZE);
        if (nbytes <= 0) {
            perror("[Server] read() failed or client disconnected.");
            break;
        }

        printf("[Server] Received from client: %s\n", buffer);

        if (strncmp(buffer, "B", 1) == 0) 
        {
            handle_begin_packet(client_fd, buffer);
        }
         else if (strncmp(buffer, "I", 1) == 0) 
        {
            // Call the initialize packet handler for placing pieces
            printf("[Server] Initialize packet received from client %d\n", client_fd);
            //handle_initialize_packet(client_fd, buffer, player_board);
            //printf("Player#: %d", player);
            
            if(player == 1)
            {
                handle_initialize_packet(client_fd, buffer, game_state.player1_board);
            }
            if(player == 2)
            {
                handle_initialize_packet(client_fd, buffer, game_state.player2_board);
            }
        }
        else if(strncmp(buffer, "S", 1) == 0)
        {
            if(player == 1)
            {
                handle_shoot_packet(client_fd, buffer, game_state.player2_board, player);
            }
            if(player == 2)
            {
                handle_shoot_packet(client_fd, buffer, game_state.player1_board, player);
            }
        }
        else if (strncmp(buffer, "Q", 1) == 0)
        {
            if(player == 1)
            {
                handle_query_packet(client_fd, game_state.player2_board, player);
                memset(buffer, 0, BUFFER_SIZE);
            }
            if(player == 2)
            {
                handle_query_packet(client_fd, game_state.player1_board, player);
                memset(buffer, 0, BUFFER_SIZE);
            }
        }
        else if (strcmp(buffer, "F") == 0) 
        {
            printf("[Server] Forfeit packet received from client %d\n", client_fd);
            handle_forfeit_packet(client_fd, player);  
            break;
        }
        //sscanf
        
        printf("[Server] Player 1 Board:");
        print_board(game_state.player1_board, game_state.width, game_state.height);
        printf("[Server] Player 2 Board:");
        print_board(game_state.player2_board, game_state.width, game_state.height);

        snprintf(buffer, BUFFER_SIZE, "A CKNOWLEDGE");
        send(client_fd, buffer, strlen(buffer), 0);

         // Checking the win condition after each packet
        if (game_state.player1_ships_remaining == 0) 
        {
            send(client1_fd, "H 0", 4, 0);
            send(client2_fd, "H 1", 4, 0);
            printf("[Server] Player 1 has lost!\n");
            break;
        } else if (game_state.player2_ships_remaining == 0) 
        {
            send(client2_fd, "H 0", 4, 0);
            send(client1_fd, "H 1", 4, 0);
            printf("[Server] Player 2 has lost!\n");
            break;
        }

        //Switches turns
        pthread_mutex_lock(&turn_mutex);
        num_of_player_turn = (player == 1) ? 2 : 1;
        pthread_cond_broadcast(&turn_condition);
        pthread_mutex_unlock(&turn_mutex);
    }

    close(client_fd);
    return NULL;
}

int main() 
{
    int server_fd1; 
    int server_fd2;
    struct sockaddr_in address1, address2;
    socklen_t addr_len = sizeof(struct sockaddr_in);

    // Initialize game state
    game_state.width = 10;
    game_state.height = 10;
    game_state.player1_ships_remaining = 5;
    game_state.player2_ships_remaining = 5;

    game_state.player1_board = (int*)malloc(game_state.width * game_state.height * sizeof(int));
    game_state.player2_board = (int*)malloc(game_state.width * game_state.height * sizeof(int));
    if (!game_state.player1_board || !game_state.player2_board) 
    {
        perror("Failed to allocate memory for game boards.");
        exit(EXIT_FAILURE);
    }

    memset(game_state.player1_board, 0, game_state.width * game_state.height * sizeof(int));
    memset(game_state.player2_board, 0, game_state.width * game_state.height * sizeof(int));


    // Setup socket for client 1
    server_fd1 = socket(AF_INET, SOCK_STREAM, 0);
    address1.sin_family = AF_INET;
    address1.sin_addr.s_addr = INADDR_ANY;
    address1.sin_port = htons(PORT1);

    // Setup socket for client 2
    server_fd2 = socket(AF_INET, SOCK_STREAM, 0);
    address2.sin_family = AF_INET;
    address2.sin_addr.s_addr = INADDR_ANY;
    address2.sin_port = htons(PORT2);

    //bind(server_fd1, (struct sockaddr*)&address1, sizeof(address1));
    //listen(server_fd1, 3);

    // // Set socket options
    // if (setsockopt(server_fd1, SOL_SOCKET, SO_REUSEADDR, 1, sizeof(1))) {
    //     perror("setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))");
    //     exit(EXIT_FAILURE);
    // }
    // if (setsockopt(server_fd1, SOL_SOCKET, SO_REUSEPORT, 1, sizeof(1))) {
    //     perror("setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt))");
    //     exit(EXIT_FAILURE);
    // }

    //bind 1 and 2
    if (bind(server_fd1, (struct sockaddr *)&address1, sizeof(address1)) < 0) 
    {
        perror("[Server] bind() failed");
        exit(EXIT_FAILURE);
    }
    if (bind(server_fd2, (struct sockaddr *)&address2, sizeof(address2)) < 0) 
    {
        perror("[Server] bind() failed");
        exit(EXIT_FAILURE);
    }

    //listen 1 and2
    if (listen(server_fd1, 3) < 0) 
    {
        perror("[Server] listen() failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd2, 3) < 0) 
    {
        perror("[Server] listen() failed");
        exit(EXIT_FAILURE);
    }
    

    // bind(server_fd2, (struct sockaddr*)&address2, sizeof(address2));
    // listen(server_fd2, 3);
    

    printf("[Server] Waiting for players...\n");

    // Accept connection from player 1
    client1_fd = accept(server_fd1, (struct sockaddr*)&address1, &addr_len);
    printf("[Server] Player 1 connected.\n");

    // Accept connection from player 2
    client2_fd = accept(server_fd2, (struct sockaddr*)&address2, &addr_len);
    printf("[Server] Player 2 connected.\n");

    // Creating threads for each client, runs the game
    pthread_t thread1, thread2;
    pthread_create(&thread1, NULL, handle_client, &client1_fd);
    pthread_create(&thread2, NULL, handle_client, &client2_fd);
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    free(game_state.player1_board);
    free(game_state.player2_board);

    printf("[Server] Shutting down.\n");
    close(server_fd1);
    close(server_fd2);
    return 0;
}