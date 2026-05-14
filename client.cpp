#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <string>
#include <chrono>
#include <cmath>

#ifdef _WIN32
    #include <SFML\Graphics.hpp>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib") 
    typedef int socklen_t;
#else
    #include <SFML/Graphics.hpp>
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <fcntl.h>
    typedef int SOCKET;
#endif

using namespace std;

#define SERVER_PORT 9999
#define BUFFER_SIZE 1024
#define MAX_PLAYERS 8
#define ll long long
#define SERVER_IP "169.254.18.240"

sf::Color player_color=sf::Color::Yellow;
double PLAYER_X=200.0, PLAYER_Y=200.0;
double WINDOW_HEIGHT=1000,WINDOW_WIDTH=1000;
double speed=WINDOW_WIDTH/600.0;
double it_speed=1.5*speed;
double player_size=WINDOW_WIDTH/120.0;
bool am_it=false;
vector<pair<double,double>> position_markers;
ll packets_sent=0,packets_lost=0;
chrono::steady_clock::time_point last_tag=chrono::steady_clock::now();


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
StatePacket prev_packet;

class Button{
    sf::RectangleShape main_button;
    public:
        void draw(sf::RenderWindow& window,double x,double y){
            x-=WINDOW_WIDTH/10,y-=WINDOW_HEIGHT/20;
            sf::RectangleShape button(sf::Vector2f(2*WINDOW_WIDTH/10,WINDOW_HEIGHT/10));
            button.setPosition(sf::Vector2f(x,y));
            button.setFillColor(sf::Color::Green);
            main_button=button;
            window.draw(button);
            sf::Font font;
            if (!font.loadFromFile("D:\\cs212\\Multiplayer Game\\GoogleSans.ttf")) {
                return;
            }
            sf::Text txt("START",font);
            txt.setCharacterSize(24);
            x+=WINDOW_WIDTH/20,y+=WINDOW_HEIGHT/40;
            txt.setFillColor(sf::Color::Black);
            txt.setPosition(sf::Vector2f(x,y));
            window.draw(txt);
        }
        bool isMouseOver(sf::RenderWindow& window){
            sf::Vector2i mouse_pos=sf::Mouse::getPosition(window);
            sf::FloatRect bounds=main_button.getGlobalBounds();
            return bounds.contains(mouse_pos.x,mouse_pos.y);
        }
};


void set_nonblocking(SOCKET sockfd) {
    #ifdef _WIN32
        u_long mode = 1;
        ioctlsocket(sockfd, FIONBIO, &mode);
    #else
        fcntl(sockfd, F_SETFL, O_NONBLOCK);
    #endif
}

void draw_person(sf::RenderWindow& game,sf::Color clr,double x,double y){
    sf::RectangleShape player(sf::Vector2f(player_size,player_size));
    player.setFillColor(clr);
    player.setPosition(sf::Vector2f(x,y));
    game.draw(player);
}

void update_relative_position_marker(double x,double y){
    double theta=atan2(y-PLAYER_Y,x-PLAYER_X);
    double radius=player_size;
    position_markers.push_back({PLAYER_X+radius*cos(theta),PLAYER_Y+radius*sin(theta)});
}

void render_time_text(sf::RenderWindow& game,int time,bool am_it){
    string text="";
    if(time==-1) text="Waiting for Players";
    else if(time==0 && am_it) text="You Lose!";
    else if(time==0) text="You Win!";
    else text=to_string(time);
    sf::Font font;
    font.loadFromFile("GoogleSans.ttf");

    sf::Text timer_text;
    timer_text.setString(text);
    timer_text.setFont(font);
    timer_text.setCharacterSize(24);
    timer_text.setFillColor(sf::Color::Black);
    timer_text.setPosition(sf::Vector2f(WINDOW_WIDTH/2.0,WINDOW_HEIGHT/2.0));
    
    sf::View text_view;
    text_view.setSize(1000.f, 1000.f);
    text_view.setCenter(500.f, 500.f);
    game.draw(timer_text);
}

void recieve_pkt(SOCKET sockfd,char* buffer,sf::RenderWindow& game){
    int n=recvfrom(sockfd,buffer,BUFFER_SIZE,0,NULL,NULL);
    StatePacket* pkt;
    position_markers.clear();
    if(n<0 || n!=sizeof(StatePacket)){
        pkt=&prev_packet;
    }
    else{
        pkt=reinterpret_cast<StatePacket*>(buffer);
        prev_packet=*pkt;
    }
    for(int i=0;i<MAX_PLAYERS;i++){
        if(!pkt->players[i].is_present) continue;
        sf::Color clr;
        if(pkt->players[i].is_it){
            if(i==pkt->player_number){
                player_color=sf::Color::Magenta;
                am_it=true;
                speed=it_speed;
                packets_lost=packets_sent-pkt->players[i].packets_recieved;
                continue;
            }
            else{
                if(am_it){
                    last_tag=chrono::steady_clock::now();
                }
                if(chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now()-last_tag).count()<2.0) speed=it_speed*2.5;
                else speed=it_speed/1.5;
                am_it=false;
                player_color=sf::Color::Yellow;
            }
            clr=sf::Color::Red;
        }
        else{
            clr=sf::Color::Green;
            if(i==pkt->player_number){
                packets_lost=packets_sent-pkt->players[i].packets_recieved;
                continue;
            }
        }
        double x=pkt->players[i].x-player_size/2,y=pkt->players[i].y-player_size/2;
        update_relative_position_marker(pkt->players[i].x,pkt->players[i].y);
        draw_person(game,clr,x,y);
    }
    render_time_text(game,pkt->time_remaining,pkt->players[pkt->player_number].is_it);
}


void send_position(SOCKET sockfd,sockaddr_in& server_addr){
    Packet pkt;
    pkt.type=PacketType(PKT_POSITION);
    pkt.x=PLAYER_X;
    pkt.y=PLAYER_Y;
    pkt.packets_sent=packets_sent+1;
    sendto(sockfd,(char*)&pkt,sizeof(pkt),0,(sockaddr*)&server_addr,sizeof(server_addr));
    packets_sent++;
}

void send_disc_pkt(SOCKET sockfd, sockaddr_in& server_addr){
    Packet pkt;
    pkt.type=PacketType(PKT_DISCONNECT);
    sendto(sockfd,(char*)&pkt,sizeof(pkt),0,(sockaddr*)&server_addr,sizeof(server_addr));
}

void draw_walls(sf::RenderWindow& game){
    sf::RectangleShape wall(sf::Vector2f(WINDOW_WIDTH/10.0,WINDOW_HEIGHT));
    wall.setFillColor(sf::Color::Black);
    wall.setPosition(sf::Vector2f(-WINDOW_WIDTH/10.0,0.0));
    game.draw(wall);
    wall.setPosition(sf::Vector2f(WINDOW_WIDTH,0.0));
    game.draw(wall);

    wall.setSize(sf::Vector2f(WINDOW_WIDTH*1.2,WINDOW_HEIGHT/10.0));
    wall.setPosition(sf::Vector2f(-WINDOW_WIDTH*0.1,-WINDOW_HEIGHT/10.0));
    game.draw(wall);
    wall.setPosition(sf::Vector2f(-WINDOW_WIDTH*0.1,WINDOW_HEIGHT));
    game.draw(wall);
}

void game_renderer(sf::RenderWindow& game){
    double x=PLAYER_X-player_size/2,y=PLAYER_Y-player_size/2;
    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)){
        if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) x-=speed/1.414;
        else x-=speed;
    }

    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)){
        if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) x+=speed/1.414;
        else x+=speed;
    }
    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)){
        if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) y-=speed/1.414;
        else y-=speed;
    }
    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)){
        if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) y+=speed/1.414;
        else y+=speed;
    }
    draw_walls(game);
    if(x<0) x=0;
    if(x>WINDOW_WIDTH-player_size) x=WINDOW_WIDTH-player_size;
    if(y<0) y=0;
    if(y>WINDOW_HEIGHT-player_size) y=WINDOW_HEIGHT-player_size;
    
    draw_person(game,player_color,x,y);
    
    PLAYER_X=(x+player_size/2),PLAYER_Y=(y+player_size/2);

}

void draw_markers(sf::RenderWindow& game){
    if(!am_it) return;
    sf::CircleShape pointer(player_size/10.0);
    pointer.setFillColor(sf::Color::Cyan);
    for(auto position_pair:position_markers){
        pointer.setPosition(sf::Vector2f(position_pair.first,position_pair.second));
        game.draw(pointer);
    }
}

void start_screen(sf::RenderWindow& game,bool& joined_server){
    Button start_button;
    start_button.draw(game,WINDOW_HEIGHT/2,WINDOW_WIDTH/2);
    if(sf::Mouse::isButtonPressed(sf::Mouse::Left) && start_button.isMouseOver(game)){
        joined_server=true;
    }
}

int main(){
    bool joined_server=false;
    char buffer[BUFFER_SIZE];
    for(int i=0;i<MAX_PLAYERS;i++){
        prev_packet.players[i].is_present=false;
    }
    prev_packet.time_remaining=-1;
    #ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            fprintf(stderr, "WSAStartup failed\n");
            return 1;
        }
    #endif
    SOCKET sockfd;
    struct sockaddr_in server_addr;
    sockfd=socket(AF_INET,SOCK_DGRAM,0);
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
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    set_nonblocking(sockfd);
   
    sf::RenderWindow game(sf::VideoMode(WINDOW_WIDTH,WINDOW_HEIGHT),"TAG");
    game.setFramerateLimit(60);

    sf::View view(sf::Vector2f(0.0f,0.0f),sf::Vector2f(WINDOW_WIDTH/10.0,WINDOW_HEIGHT/10.0));

    while(game.isOpen()){
        sf::Event evnt;
        while(game.pollEvent(evnt)){
            switch(evnt.type){
                case sf::Event::Closed:
                    game.close();
                    if(joined_server) send_disc_pkt(sockfd,server_addr);
                    break;
            }
        }
        
        game.clear(sf::Color(255,255,255,255));

        if(joined_server){
            game_renderer(game);
            recieve_pkt(sockfd,buffer,game);
            game.setView(view);
            draw_markers(game);

            view.setCenter(sf::Vector2f(PLAYER_X,PLAYER_Y));
            game.setView(view);
            
            send_position(sockfd,server_addr);
            printf("%lld\n",packets_lost);
        }
        else{
            start_screen(game,joined_server);
        }
        game.display();
    }
    #ifdef _WIN32
        closesocket(sockfd);
        WSACleanup();
    #else
        close(sockfd);
    #endif
    return 0;
}