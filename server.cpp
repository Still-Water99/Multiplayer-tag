/* Windows compatible
    - Uses Winsock2 instead of POSIX sockets
    - Replace unistd.h, arpa/inet.h with winsock2.h and ws2tcpip.h
    - Link with -lws2_32 (add -lws2_32 to your compiler flags)
    - Compile: g++ server.cpp -o server -lws2_32
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <string>
#include <chrono>
#include <cmath>
#include <fstream>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib") 
    typedef int socklen_t;
#else
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <fcntl.h>
    #include <netdb.h>
    typedef int SOCKET;
#endif

using namespace std;

#define SERVER_PORT 9999
#define BUFFER_SIZE 1024
#define MAX_PLAYERS 8
#define MIN_PLAYERS 2
#define START_X 0.0
#define START_Y 0.0
#define PLAYER_SIZE 1000.0/120.0
#define GAME_DURATION 120

chrono::steady_clock::time_point start_time=chrono::steady_clock::now();
chrono::steady_clock::time_point game_start;
chrono::steady_clock::time_point wait_time;
bool game_started=false;
bool check_finished=false;

#pragma pack(1)
enum PacketType :uint8_t{
    PKT_POSITION=0x00,
    PKT_DISCONNECT=0x01,
    PKT_TAG=0x02,
    PKT_STATE=0x03
};
struct Packet{
    uint8_t type;
    double x;
    double y;
    long long packets_sent;
};
struct PlayerState{
    bool is_present;
    double x;
    double y;
    long long packets_recieved;
    bool is_it;
};
struct StatePacket{
    uint8_t type;
    uint8_t player_number;
    PlayerState players[MAX_PLAYERS];
    int time_remaining;
};
#pragma pack()
class Player{
    private:
    struct sockaddr_in addr;
    chrono::steady_clock::time_point prev_time;
    chrono::steady_clock::time_point tag_time;
    public:
    double x,y;
    bool tagged;
    long long packets_recieved;
    long long packets_sent;
    Player(sockaddr_in add){
        this->addr=add;
        x=START_X;
        y=START_Y;
        tagged=false;
        prev_time=chrono::steady_clock::now();
        tag_time=chrono::steady_clock::now();
        packets_recieved=0;
    }
    void send_packet(SOCKET sockfd,StatePacket pkt){
        sendto(sockfd,(char*)&pkt,sizeof(pkt),0,(struct sockaddr *)&addr,sizeof(addr));
    }
    void update_position(double x_new,double y_new,chrono::steady_clock::time_point time){
        x=x_new;
        y=y_new;
        prev_time=time;
    }
    bool comms_stopped(chrono::steady_clock::time_point time){
        if(chrono::duration_cast<chrono::seconds>(time-prev_time).count()>2.0) return true;
        return false;
    }
    bool check_tag(Player* it){
        if(chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now()-tag_time).count()<1.0) return false;
        double dx=fabs(it->x-x)-PLAYER_SIZE,dy=fabs(it->y-y)-PLAYER_SIZE;
        if(dx<0 && dy<0){
            tagged=true;
            it->tagged=false;
            it->tag_time=chrono::steady_clock::now();
            return true;
        }
        return false;
    }
    bool check_same_addr(sockaddr_in& incoming){
        return incoming.sin_addr.s_addr==addr.sin_addr.s_addr && incoming.sin_port==addr.sin_port; 
    }
};

bool recieve_incoming(SOCKET sockfd,char* buffer,vector<Player*>& players){
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int n = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0,(struct sockaddr *)&client_addr, &client_len);
    if(n<0) return false;
    buffer[n] = '\0';
    
    if(n!=sizeof(Packet)) return true;
    Packet* pkt=reinterpret_cast<Packet*>(buffer);
    int ind=-1;
    bool found=false;
    for(int i=0;i<MAX_PLAYERS;i++){
        if(ind==-1 && players[i]==nullptr){
            ind=i;
            continue;
        }
        if(players[i]==nullptr) continue;
        if(players[i]->check_same_addr(client_addr)){
            ind=i;
            found=true;
            break;
        }
    }
    if(ind==-1){
        //max players reached (yet to implement)
        return true;
    }
    if(!found){
        players[ind]=new Player(client_addr);
    }
    players[ind]->packets_recieved+=1;
    if(pkt->type==PacketType(PKT_DISCONNECT)){
        delete players[ind];
        players[ind]=nullptr;
        return true;
    }
    if(pkt->type==PacketType(PKT_POSITION)){
        players[ind]->packets_sent=pkt->packets_sent;
        players[ind]->update_position(pkt->x,pkt->y,chrono::steady_clock::now());
        return true;
    }
    return false;
}

void send_positional_updates(SOCKET sockfd,vector<Player*>& players,ofstream& log_file){
    StatePacket pkt;
    pkt.type=PacketType(PKT_STATE);
    pkt.player_number=0;
    int elapsed=-1;
    if(game_started) elapsed=max(0,(int)(GAME_DURATION-chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now()-game_start).count()));
    if(elapsed==0 && !check_finished){
        check_finished=true;
        wait_time=chrono::steady_clock::now();
    }
    else if(elapsed==0 && chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now()-wait_time).count()>15){
        check_finished=false;
        game_start=chrono::steady_clock::now();
        elapsed=-1;
    }
    pkt.time_remaining=elapsed;
    for(auto player:players){
        if(player==nullptr){
            pkt.players[pkt.player_number].is_present=false;
            pkt.player_number+=1;
            continue;
        }
        pkt.players[pkt.player_number].is_present=true;
        pkt.players[pkt.player_number].x=player->x;
        pkt.players[pkt.player_number].y=player->y;
        pkt.players[pkt.player_number].is_it=player->tagged;
        pkt.players[pkt.player_number].packets_recieved=player->packets_recieved;
        pkt.player_number+=1;
    }
    for(int i=0;i<MAX_PLAYERS;i++){
        if(players[i]==nullptr) continue;
        pkt.player_number=i;
        players[i]->send_packet(sockfd,pkt);
    }
    auto now=chrono::steady_clock::now();
    long long ms=chrono::duration_cast<chrono::milliseconds>(now-start_time).count();
    for(int i=0;i<MAX_PLAYERS;i++){
        if(players[i]==nullptr) continue;
        log_file<<ms<<","<<i<<","<<players[i]->packets_recieved<<","<<players[i]->packets_sent<<","<<players[i]->x<<","<<players[i]->y<<","<<players[i]->tagged<<endl;
    }
    log_file.flush();
}

void check_timeouts(vector<Player*>& players){
    for(int i=0;i<players.size();i++){
        if(players[i]==nullptr) continue;
        if(players[i]->comms_stopped(chrono::steady_clock::now())){
            delete players[i];
            players[i]=nullptr;
        }
    }
}

void set_nonblocking(SOCKET sockfd) {
    #ifdef _WIN32
        u_long mode = 1;
        ioctlsocket(sockfd, FIONBIO, &mode);
    #else
        fcntl(sockfd, F_SETFL, O_NONBLOCK);
    #endif
}

void check_tags(vector<Player*>& players){
    Player* it=nullptr;
    for(auto player:players){
        if(player==nullptr) continue;
        if(player->tagged){
            it=player;
            break;
        }
    }
    if(it==nullptr) return;
    for(auto player:players){
        if(player==nullptr || it==player){
            continue;
        }
        if(player->check_tag(it)) return;
    }
}

void set_initial_it(vector<Player*>& players){
    int count=0,it_index=-1;
    for(int i=0;i<MAX_PLAYERS;i++){
        if(players[i]==nullptr) continue;
        if(players[i]->tagged) it_index=i;
        count++;
    }
    if(count<MIN_PLAYERS || check_finished){
        if(it_index!=-1) players[it_index]->tagged=false;
        if(game_started && !check_finished) game_started=false;
        return;
    }
    if(!game_started) game_start=chrono::steady_clock::now();
    game_started=true;
    if(it_index!=-1) return;
    it_index=rand()%count;
    count=0;
    for(auto player:players){
        if(player==nullptr) continue;
        if(count==it_index){
            player->tagged=true;
            return;
        }
        count++;
    }
}

int main(void) {
    ofstream log_file("stats.csv");
    log_file << "timestamp,player_index,packets_received,packets_sent,x,y,is_it\n";
    #ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            fprintf(stderr, "WSAStartup failed\n");
            return 1;
        }
    #endif
    SOCKET sockfd;
    char buffer[BUFFER_SIZE];
    
    vector<Player*> players(MAX_PLAYERS,nullptr);

    struct sockaddr_in server_addr;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    
    #ifdef _WIN32
        if (sockfd == INVALID_SOCKET) {
            fprintf(stderr, "socket failed: %d\n", WSAGetLastError());
            WSACleanup();
            exit(EXIT_FAILURE);
        }
    #else
        if (sockfd < 0) {
            perror("socket");
            exit(EXIT_FAILURE);
        }
    #endif
    set_nonblocking(sockfd);
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        #ifdef _WIN32
            fprintf(stderr, "bind failed: %d\n", WSAGetLastError());
            closesocket(sockfd);
            WSACleanup();
        #else
            perror("bind");
            close(sockfd);
        #endif
        exit(EXIT_FAILURE);
    }

    char hostname[256];
    gethostname(hostname, sizeof(hostname));

    struct hostent* host_info = gethostbyname(hostname);
    char* ip = inet_ntoa(*(struct in_addr*)host_info->h_addr_list[0]);

    printf("Server running on %s:%d\n", ip, SERVER_PORT);

    srand(time(0));
    while (1) {
        while(recieve_incoming(sockfd,buffer,players));
        check_timeouts(players);
        set_initial_it(players);
        check_tags(players);
        send_positional_updates(sockfd,players,log_file);
        #ifdef _WIN32
            Sleep(50);
        #else
            usleep(50000);
        #endif
    }
    #ifdef _WIN32
        closesocket(sockfd);
        WSACleanup();
    #else
        close(sockfd);
    #endif
    return 0;
}