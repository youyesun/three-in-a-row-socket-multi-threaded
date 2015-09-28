/*
 * C++ Three In A Row game
 * Socket, Multi-threaded
 * To complie: g++ -std=c++11 three-in-a-row.cpp -o three-in-a-row -pthread
 */

#include <iostream>
#include <vector>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <memory>
#include <string.h>
#include <string>
#include <sys/time.h>
#include <cstdint>
#include <unistd.h>

//std::bind overlaps socket::bind in BSD Unix...
using std::cout;
using std::endl;
using std::string;
using std::vector;
using std::to_string;

int BOARD_SIZE = 5;
vector<vector<string>> gameboard;

//BSD Unix doesn't support volatile in FD_ISSET 
#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
fd_set threads_fd;
#else
volatile fd_set threads_fd;
#endif
pthread_mutex_t fdmutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t boardmutex = PTHREAD_MUTEX_INITIALIZER;

uint16_t _PORT = 12000;
int MAX_THREADS_NUM = 3;
int FD_OFFSET = 0;
int MAX_ACCEPTABLE_CLIENT_MSG_LENGTH = 1000;

const string color_green = "\e[0;32m";
const string color_red = "\e[0;31m";


void newgame(){
    pthread_mutex_lock(&boardmutex);
    for(int i = 0; i < BOARD_SIZE; i++){
        for(int j = 0; j < BOARD_SIZE; j ++){
            gameboard[i][j] = "\u25A1"; //empty space
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
        cout << "Unable to bind server socket..." << endl;
        return -1;
    }
    if(listen(sk, 16) < 0){
        cout << "Unable to listen on server socket..." << endl;
        return -1;
    }
    return sk;
}


int on_client_connection(int server_fd){
    sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    memset(&client_addr, 0, sizeof(client_addr));
    auto client_fd = accept(server_fd,
                            reinterpret_cast<sockaddr*>(&client_addr),
                            &addr_len);
    cout << "Client accessing..." << endl;
    if (client_fd < 0) {
        cout << "Unable to accept connection..." << endl;
        return -1;
    }
    return client_fd;
}


int server_write(const int sk, const char* buf, const size_t n){
    auto ptr = buf;
    while(ptr < buf + n){
        auto ret = send(sk, ptr, n - (ptr - buf), 0);
        if(ret <= 0){
            cout << "Unable to write to socket" << endl;
            return -1;
        }
        ptr += ret;
    }
    return 0;
}


/*
 * Set a read timeout
 */
bool server_read_timeout(const int sk){
    timeval tv;
    tv.tv_sec = 10;
    tv.tv_usec = 0;
    if(setsockopt(sk, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0){
        cout << "Unable to set read timeout..." << endl;
        return false;
    }
    return true;
}


int server_read(const int sk, char* buf, const size_t n){
    auto ptr = buf;
    while(ptr < buf + n){
        if(!server_read_timeout(sk))
            return -1;
        auto ret = recv(sk, ptr, n - (ptr - buf), 0);
        if(ret <= 0)
            break;
        ptr += ret;
    }
    if(!(ptr - buf))
        cout << "Unable to read on socket..." << endl;
    return ptr - buf;
}


void show_board(int client_fd){
    string cols = color_green + "  a  b  c  d  e\n";
    string pre_cols = "\n" + cols;
    server_write(client_fd, pre_cols.c_str(), strlen(pre_cols.c_str()));
    for(char i = '1'; i < '1' + BOARD_SIZE; i ++){
        string pstr = color_green + i;
        server_write(client_fd, pstr.c_str(), strlen(pstr.c_str()));
        for(int j = 0; j < BOARD_SIZE; j ++){
            string crossstr = " " + gameboard[i - '1'][j] + " ";
            server_write(client_fd, crossstr.c_str(), strlen(crossstr.c_str()));
        }
        pstr += "\n";
        server_write(client_fd, pstr.c_str(), strlen(pstr.c_str()));
    }
    cols += "\n" + color_red;
    server_write(client_fd, cols.c_str(), strlen(cols.c_str()));
}


void broadcast_message(int client_fd, string message){
    for(int c_fd = FD_OFFSET; c_fd < FD_OFFSET + MAX_THREADS_NUM; c_fd ++){
        if((c_fd != client_fd) && FD_ISSET(c_fd, &threads_fd)){
            string st = color_red + "From User " + to_string(client_fd) + ":\n" +
                        message + "\n";
            server_write(c_fd, st.c_str(), strlen(st.c_str()));
        }
    }
}


void print_all_boards(){
    for(int c_fd = FD_OFFSET; c_fd < FD_OFFSET + MAX_THREADS_NUM; c_fd ++){
        if(FD_ISSET(c_fd, &threads_fd)){
            show_board(c_fd);
        }
    }
}


void place_piece(const int client_fd, string place){
    auto i = place[1] - '1';
    auto j = place[2] - 'a';
    if(gameboard[i][j] == "\u25A1"){
        pthread_mutex_lock(&boardmutex);
        if(place[0] == 'o'){
            gameboard[i][j] = "O";
        }else{
            gameboard[i][j] = "X";
        }
        pthread_mutex_unlock(&boardmutex);
        string pstr = "Place a piece at " + place.substr(1);
        cout << pstr << endl;
        broadcast_message(client_fd, pstr);
        print_all_boards();
    }else{
        string err_place = "Position filled. Try another one...\n";
        server_write(client_fd, err_place.c_str(), strlen(err_place.c_str()));
    }
}


void process_client_input(const int client_fd, const char *buf){
    if(buf[0] == '0')
        show_board(client_fd);
    else if(buf[0] == '1'){
        string resetgame= "Resetting game...\n";
        broadcast_message(client_fd, resetgame);
        newgame();
        print_all_boards();
    }else
    // Handle recv when size of buf is larger than bytes received
    if(sizeof(buf) >= 4 && (buf[0] == 'o' || buf[0] == 'x') &&
      buf[1] >= '1' && buf[1] <= '5' && buf[2] >= 'a' && buf[2] <= 'e'){
        string place = "";
        for(int i = 0; i < 3; i ++)
            place += buf[i];
        place_piece(client_fd, place);
    }else{
        string invalid = "Invalid command...\n";
        server_write(client_fd, invalid.c_str(), strlen(invalid.c_str()));
    }
}


void *client_command_handler(void *arg){
    int *client_fd_ptr = reinterpret_cast<int *>(arg);
    int client_fd = *client_fd_ptr;
    char buf[MAX_ACCEPTABLE_CLIENT_MSG_LENGTH];
    while(true){
        auto read_bytes = recv(client_fd, buf, sizeof(buf), 0);
        if(!read_bytes){
            cout << "Client disconnected...\nClearing client file descriptor "
                << client_fd << endl;
            string on_close = color_red + "Not getting input...\n"
                              "Closing connection...\n";
            server_write(client_fd, on_close.c_str(), strlen(on_close.c_str()));
            pthread_mutex_lock(&fdmutex);
            FD_CLR(client_fd, &threads_fd);
            pthread_mutex_unlock(&fdmutex);
            close(client_fd);
            pthread_exit(NULL);
        }
        process_client_input(client_fd, buf);
    }
}


void start_service(int server_fd){
    string info = "Commands:\n'0' to show the board\n'1' to reset game\n"
                  "Type board coordinates to place an piece. For example:\n"
                  "'o1a' places a circle mark at 1a, 'x2b' places a cross "
                  "mark at 2b\n";
    string color_info = color_red + info;
    pthread_t threads[MAX_THREADS_NUM];
    FD_ZERO(&threads_fd); // clear the file descriptor set
    while(true){
        int client_fd = on_client_connection(server_fd);
        if(client_fd >= (MAX_THREADS_NUM + FD_OFFSET)){
            cout << "Hit access limit..." << endl;
            string hitlim = color_red + "Too many users...\nTry later...\n";
            server_write(client_fd, hitlim.c_str(), strlen(hitlim.c_str()));
            close(client_fd);
            continue;
        }
        if(client_fd){
            cout << "Client connected. Using file desciptor " << client_fd << endl;
            pthread_mutex_lock(&fdmutex);
            FD_SET(client_fd, &threads_fd); // push client fd into fd set
            pthread_mutex_unlock(&fdmutex);
            void *arg = reinterpret_cast<void *>(&client_fd);
            server_write(client_fd, color_info.c_str(), strlen(color_info.c_str()));
            pthread_create(&threads[client_fd - FD_OFFSET],
                          NULL,
                          client_command_handler,
                          arg);
        }
    }
}


int main(){
    //create SIZE*SIZE board
    gameboard.resize(BOARD_SIZE);
    for(int i = 0; i < BOARD_SIZE; i++)
        gameboard[i].resize(BOARD_SIZE);
    newgame();
    cout << "Starting server..." << endl;
    int server_fd = server_listen();
    if(server_fd == -1){
        cout << "Can\'t start server..." << endl;
        return 1;
    }
    FD_OFFSET = server_fd + 1;
    start_service(server_fd);
    return 0;
}
