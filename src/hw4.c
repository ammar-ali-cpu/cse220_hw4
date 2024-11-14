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

void send_halt_packet(int client_fd) 
{
    char halt_packet[] = "H";  // Halt packet
    int nbytes = send(client_fd, halt_packet, strlen(halt_packet), 0);
    if (nbytes == -1) 
    {
        perror("Error sending Halt packet");
    } 
    else 
    {
        printf("[Server] Sent Halt packet to client %d\n", client_fd);
    }
}

void handle_forfeit_packet(int forfeiting_client_fd) 
{
    // Send the Halt packet to both clients
    printf("[Server] Player forfeited, sending Halt packet to both clients.\n");

    // Send Halt to Player 1
    send_halt_packet(client1_fd);
    // Send Halt to Player 2
    send_halt_packet(client2_fd);

    // Close the connections to both clients
    close(client1_fd);
    close(client2_fd);

    printf("[Server] Both connections closed due to forfeit.\n");
}




int is_valid_piece(int type, int rotation, int col, int row, int player_id) 
{
    //int (*piece_shape)[4][2] = pieces[type][rotation];
    int *board = (player_id == 1) ? game_state.player1_board : game_state.player2_board;

    for (int i = 0; i < 4; i++) 
    {
        printf("piece_shape[i][0] : %d", pieces[type][rotation][i][0]); //this should not be 7 and 4 because that will only look at last piece, last rotation
        printf("piece_shape[i][1] : %d", pieces[type][rotation][i][1]);
        
        int x = row + pieces[7][4][i][0];
        int y = col + pieces[7][4][i][1];

        // Check board boundaries
        if (x < 0 || x >= game_state.width || y < 0 || y >= game_state.width) 
        {
            printf("Out of bounds: X -> %d Y -> %d", x, y);
            return 0;
        }

        // Calculate position in 1D array
        int pos = x * game_state.width + y;

        // Check for overlap
        if (board[pos] != 0) 
        {
            printf("[Debug] Invalid piece placement: Overlap at position %d\n", pos);
            return 0;
        }
    }
    return 1;
}


void place_piece_on_board(int type, int rotation, int col, int row, int* player_board) 
{
    //int (*piece_shape)[4][2] = pieces[type][rotation];
    for (int i = 0; i < 4; i++) 
    {
        int x = row + pieces[type][rotation][i][0];
        int y = col + pieces[type][rotation][i][1];
        int pos = x * game_state.width + y;

        // Mark the cell on the board with the piece's type
        printf("Pos: %d\n", pos);
        player_board[pos] = type + 1; //Issue: does not chnage ACTUAL boards, pass by value/reference error -> game_state.board1/2 not passed as parameters anywhere
        printf("[Debug] Placed piece segment at board[%d] (%d, %d)\n", pos, x, y);
    }
}

void handle_initialize_packet(int client_fd, char *buffer, int* player_board) 
{
    int piece_data[20];  // Array to hold 5 pieces * 4 attributes (type, rotation, col, row) = 20 integers
    int num_values = sscanf(buffer, "I %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
    &piece_data[0], &piece_data[1], &piece_data[2], &piece_data[3], &piece_data[4], &piece_data[5], &piece_data[6], &piece_data[7],
    &piece_data[8], &piece_data[9], &piece_data[10], &piece_data[11], &piece_data[12], &piece_data[13], &piece_data[14], &piece_data[15],
    &piece_data[16], &piece_data[17], &piece_data[18], &piece_data[19]);

    if (num_values != 20) 
    {
        printf("[Server] Invalid initialize packet format.\n");
        return;
    }

    // Assume game_state is already defined and initialized
    int player_id = (client_fd == client1_fd) ? 1 : 2;
    //int* player_board_to_edit = player_board;

    // Process each piece (5 pieces with 4 attributes each)
    for (int i = 0; i < 5; i++) 
    {
        int type = piece_data[i * 4];
        int rotation = piece_data[i * 4 + 1];
        int col = piece_data[i * 4 + 2];
        int row = piece_data[i * 4 + 3];
        printf("Type : %d Rot: %d Col: %d Row: %d", type, rotation, col, row);

        // Validate piece placement
        if (!is_valid_piece(type, rotation, col, row, player_id)) 
        {
            printf("[Server] Invalid piece placement for player %d at piece %d.\n", player_id, i + 1);
            return;
        }

        // Place the piece on the player's board
        place_piece_on_board(type, rotation, col, row, player_board);
    }

    printf("[Server] Player %d's pieces initialized successfully.\n", player_id);
}



void* handle_client(void* sockFD) 
{
    int client_fd = *((int*)sockFD);
    char buffer[BUFFER_SIZE] = {0};

    int player = (client_fd == client1_fd) ? 1 : 2;
    int* player_board = (player == 1) ? game_state.player1_board : game_state.player2_board;

    while(1) 
    {
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
            printf("Player#: %d", player);
            //if player == 1 then this else do it for 2
            handle_initialize_packet(client_fd, buffer, game_state.player1_board); // here is the problem, I need to find a good way to know whether to send player1board or player2, or maybe use player ID and if statements
        }
        else if (strcmp(buffer, "F") == 0) 
        {
            printf("[Server] Forfeit packet received from client %d\n", client_fd);
            handle_forfeit_packet(client_fd);  
            break;
        }
        //sscanf
        
        printf("[Server] Player 1 Board:");
        print_board(game_state.player1_board, game_state.width, game_state.height);
        printf("[Server] Player 2 Board:");
        print_board(game_state.player2_board, game_state.width, game_state.height);

        // Placeholder response for testing
        snprintf(buffer, BUFFER_SIZE, "ACK");
        send(client_fd, buffer, strlen(buffer), 0);

         // Check win condition after every packet
        if (game_state.player1_ships_remaining == 0) {
            send(client1_fd, "H 0", 4, 0);
            send(client2_fd, "H 1", 4, 0);
            printf("[Server] Player 1 has lost!\n");
            break;
        } else if (game_state.player2_ships_remaining == 0) {
            send(client2_fd, "H 0", 4, 0);
            send(client1_fd, "H 1", 4, 0);
            printf("[Server] Player 2 has lost!\n");
            break;
        }

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

    bind(server_fd1, (struct sockaddr*)&address1, sizeof(address1));
    listen(server_fd1, 3);

    // Setup socket for client 2
    server_fd2 = socket(AF_INET, SOCK_STREAM, 0);
    address2.sin_family = AF_INET;
    address2.sin_addr.s_addr = INADDR_ANY;
    address2.sin_port = htons(PORT2);

    bind(server_fd2, (struct sockaddr*)&address2, sizeof(address2));
    listen(server_fd2, 3);

    printf("[Server] Waiting for players...\n");

    // Accept connection from player 1
    client1_fd = accept(server_fd1, (struct sockaddr*)&address1, &addr_len);
    printf("[Server] Player 1 connected.\n");

    // Accept connection from player 2
    client2_fd = accept(server_fd2, (struct sockaddr*)&address2, &addr_len);
    printf("[Server] Player 2 connected.\n");

    // Create threads to handle each client
    pthread_t thread1, thread2;
    pthread_create(&thread1, NULL, handle_client, &client1_fd);
    pthread_create(&thread2, NULL, handle_client, &client2_fd);

    // Wait for threads to finish
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    free(game_state.player1_board);
    free(game_state.player2_board);

    printf("[Server] Shutting down.\n");
    close(server_fd1);
    close(server_fd2);
    return 0;
}