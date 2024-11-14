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


// typedef struct {
//     int coordinates[4][2];  // Array to hold up to 4 coordinates (row, col)
// } PieceShape;

// // Define all pieces' shapes and their rotations
// PieceShape pieces[7][4] = 
// {
//     // O-piece (index 0)
//     {
//         {{0, 0}, {0, 1}, {1, 0}, {1, 1}},   // Rotation 1
//         {{0, 0}, {0, 1}, {1, 0}, {1, 1}},   // Rotation 2 (same as 1)
//         {{0, 0}, {0, 1}, {1, 0}, {1, 1}},   // Rotation 3 (same as 1)
//         {{0, 0}, {0, 1}, {1, 0}, {1, 1}}  // Rotation 3 (same as Rotation 2)
//     },
//     // I-piece (index 1)
//     {
//         {{0, 0}, {1, 0}, {2, 0}, {3, 0}}, // Rotation 0 (vertical)
//         {{0, 0}, {0, 1}, {0, 2}, {0, 3}}, // Rotation 1 (horizontal)
//         {{0, 0}, {1, 0}, {2, 0}, {3, 0}}, // Rotation 2 (same as Rotation 0)
//         {{0, 0}, {0, 1}, {0, 2}, {0, 3}}  // Rotation 3 (same as Rotation 1)
//     },
//     // Red-piece (index 2)
//     {
//         {{1, 0}, {1, 1}, {0, 1}, {0, 2}}, 
//         {{0, 0}, {1, 0}, {1, 1}, {2, 1}},
//         {{1, 0}, {1, 1}, {0, 1}, {0, 2}}, 
//         {{0, 0}, {1, 0}, {1, 1}, {2, 1}}
//     },
//     // L-piece (index 3)
//     {
//         {{0, 0}, {1, 0}, {2, 0}, {2, 1}}, // Rotation 0
//         {{0, 0}, {0, 1}, {0, 2}, {1, 0}}, // Rotation 1
//         {{0, 0}, {0, 1}, {1, 1}, {2, 1}}, // Rotation 2
//         {{1, 0}, {1, 1}, {1, 2}, {0, 2}}  // Rotation 3
//     },
//     // Green-piece (index 4)
//     {
//         {{0, 0}, {0, 1}, {1, 1}, {1, 2}},
//         {{1, 0}, {2, 0}, {1, 1}, {0, 1}}, 
//         {{0, 0}, {0, 1}, {1, 1}, {1, 2}},
//         {{1, 0}, {2, 0}, {1, 1}, {0, 1}}  
//     },
//     // Pink-piece (index 5)
//     {
//         {{2, 0}, {2, 1}, {1, 1}, {0, 1}},
//         {{0, 0}, {1, 0}, {1, 1}, {1, 2}}, 
//         {{0, 0}, {0, 1}, {1, 0}, {2, 0}},
//         {{0, 0}, {0, 1}, {0, 2}, {1, 2}}  
//     },
//     // Purple-piece (index 6)
//     {
//         {{0, 0}, {0, 1}, {0, 2}, {1, 1}},
//         {{1, 0}, {1, 1}, {0, 1}, {2, 1}}, 
//         {{1, 0}, {1, 1}, {0, 1}, {1, 2}},
//         {{0, 0}, {1, 0}, {2, 0}, {1, 1}}  
//     }
// };

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
        {{0, 0}, {1, 0}, {2, 0}, {3, 0}}, // Rotation 0 (vertical)
        {{0, 0}, {0, 1}, {0, 2}, {0, 3}}, // Rotation 1 (horizontal)
        {{0, 0}, {1, 0}, {2, 0}, {3, 0}}, // Rotation 2 (same as Rotation 0)
        {{0, 0}, {0, 1}, {0, 2}, {0, 3}}  // Rotation 3 (same as Rotation 1)
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
        {{0, 0}, {1, 0}, {2, 0}, {2, 1}}, // Rotation 0
        {{0, 0}, {0, 1}, {0, 2}, {1, 0}}, // Rotation 1
        {{0, 0}, {0, 1}, {1, 1}, {2, 1}}, // Rotation 2
        {{1, 0}, {1, 1}, {1, 2}, {0, 2}}  // Rotation 3
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








void* handle_client(void* sockFD) 
{
    int client_fd = *((int*)sockFD);
    char buffer[BUFFER_SIZE] = {0};

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
        //sscanf
        

        // Placeholder response for testing
        snprintf(buffer, BUFFER_SIZE, "ACK");
        send(client_fd, buffer, strlen(buffer), 0);
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
    listen(server_fd1, 1);

    // Setup socket for client 2
    server_fd2 = socket(AF_INET, SOCK_STREAM, 0);
    address2.sin_family = AF_INET;
    address2.sin_addr.s_addr = INADDR_ANY;
    address2.sin_port = htons(PORT2);

    bind(server_fd2, (struct sockaddr*)&address2, sizeof(address2));
    listen(server_fd2, 1);

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