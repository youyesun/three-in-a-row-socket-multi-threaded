/*
 * C++ Three In A Throw game
 * Sockets, Multi-threads
 * To complie: g++ -std=c++0x server.cpp -o server -pthread
 */

#include <vector>
#include <sys/socket.h>
using namespace std;

int BOARD_SIZE = 5;
vector<vector<string>> GAMEBOARD;
pthread_mutex_t boardmutex = PTHREAD_MUTEX_INITIALIZER;


void newgame(){
    pthread_mutex_lock(&boardmutex);
    for(int i = 0; i < BOARD_SIZE; i++){
        for(int j = 0; j < BOARD_SIZE; j ++){
            GAMEBOARD[i][j] = "X"; //empty space
        }
    }
    pthread_mutex_unlock(&boardmutex);
}


int main(){
    //create a 5*5 board
    GAMEBOARD.resize(BOARD_SIZE);
    for(int i = 0; i < BOARD_SIZE; i++)
	GAMEBOARD[i].resize(BOARD_SIZE);
    newgame();
    cout << "Starting server..." << endl;
    
}





