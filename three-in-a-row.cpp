/*
 * C++ Three In A Throw game
 * Socket, Multi-threaded
 * To complie: g++ -std=c++0x server.cpp -o server -pthread
 */

#include <iostream>
#include <vector>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <memory>
#include <string.h>
using namespace std;

int BOARD_SIZE = 5;
vector<vector<string>> GAMEBOARD;

volatile fd_set threads_fd;
pthread_mutex_t fdmutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t boardmutex = PTHREAD_MUTEX_INITIALIZER;

uint16_t _PORT = 12000;
int MAX_THREADS_NUM = 10;

void newgame(){
    pthread_mutex_lock(&boardmutex);
    for(int i = 0; i < BOARD_SIZE; i++){
        for(int j = 0; j < BOARD_SIZE; j ++){
            GAMEBOARD[i][j] = "X"; //empty space
        }
    }
    pthread_mutex_unlock(&boardmutex);
}


int server_listen(){
    auto sk = socket(AF_INET, SOCK_STREAM, 0);
    if(sk < 0){
        cout << "Unable to create server socket..." << endl;
        return -1;
    }
    
    auto opt = 1;
    if(setsockopt(sk, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0){
        cout << "Unable to set REUSE_ADDR on server socket..." << endl;
        return -1;
    }

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    if(bind(sk, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0){
        cout << "Unable to bind server socket" << endl;
        return -1;
    }
    if(listen(sk, 16) < 0){
        cout << "Unable to listen on server socket" << endl;
        return -1;
    }
    return sk;
}

int on_client_connection(int server_fd){
    cout << "Client accessing...";
    sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    memset(&client_addr, 0, sizeof(client_addr));
    auto client_sk = accept(server_fd, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);
    if (client_sk < 0) {
        cout << "Unable to accept connection";
        return -1;
    }
    return client_sk;
}    



void start_service(int server_fd){
    string info = "Commands:\n'0' to show the board\n'1' to reset game\nType board coordinates to place an piece. For example:\n'b1a' places a black piece at 1a, 'w2b' places a white piece at 2b\n";
    pthread_t threads[MAX_THREADS_NUM]; 
    FD_ZERO(&threads_fd); // clear the file descriptor set
    while(true){
        cout << info << endl;
        int client_fd = on_client_connection(server_fd);
        cout <<"client fd is " <<client_fd << endl;
        if(client_fd){
            cout << "Client connected. Using file desciptor " << client_fd << endl;
            pthread_mutex_lock(&fdmutex);
            FD_SET(client_fd, &threads_fd); // push client fd into fd set
            pthread_mutex_unlock(&fdmutex);
            
        }	        
    }

}


int main(){
    //create a 5*5 board
    GAMEBOARD.resize(BOARD_SIZE);
    for(int i = 0; i < BOARD_SIZE; i++)
	GAMEBOARD[i].resize(BOARD_SIZE);
    newgame();
    cout << "Starting server..." << endl;
    int server_fd = server_listen();    
    if(server_fd == -1){
        cout << "Can\'t start server..." << endl;
        return 1; 
    }
    start_service(server_fd);
 
    return 0;
}





