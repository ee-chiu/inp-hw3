#include "my_function.h"
#include <string.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <sys/select.h>
#include <vector>
#include <string>
#include <map>
#include <queue>
#include <iostream>
#include <sstream>
#include <ctime>
#include <algorithm>

using namespace std;

char cli_buff[10000];
char srv_buff[10000];
const int max_size = 20;
vector<bool> isLogin(max_size, false);
vector<string> user(max_size, "");
map<string, string> user2password;
map<string, bool> user2isLogin;
map<string, bool> user2isBlack;
vector<string> chat_history;
map<string, int> user2version;
map<string, int> user2port;

fd_set all_set;

int char2int(char* c){
    int i = 0;
    int num = 0;

    while(c[i] != 0){
        num = num * 10 + c[i] - '0';
        i++;
    }

    return num;
} 

string Read_line(int sockfd){
    string msg;
    while(1){
        bzero(cli_buff, sizeof(cli_buff));
        Read(sockfd, cli_buff, 1);
        if(cli_buff[0] == '\n') break;
        msg += cli_buff[0];
    }

    return msg;
}

string get_single_para(string command, int &i){
    string para;
    while(command[i] != 0 && command[i] != ' '){
        para += command[i];
        i++;
    }    

    while(command[i] == ' ')
        i++;
    
    return para;
}

void get_other_para(string command, int &i, vector<string> &para){
    string tmp;
    while(command[i] != 0){
        tmp = get_single_para(command, i);
        para.push_back(tmp);
    }

    return;
}

vector<string> split(string command){
    vector<string> para;
    int i = 0;
    string first_para = get_single_para(command, i);
    para.push_back(first_para);

    get_other_para(command, i, para);
    return para;
}

void write2cli(int sockfd,const char * message){
    snprintf(cli_buff, sizeof(cli_buff), "%s", message);
    Write(sockfd, cli_buff, strlen(cli_buff));
    return;
}

void write2cli2(int sockfd, const char * text, const char * user){
    snprintf(cli_buff, sizeof(cli_buff), "%s, %s.\n", text, user);
    Write(sockfd, cli_buff, strlen(cli_buff));
    return;
}

void welcome(int sockfd){
    write2cli(sockfd, "********************************\n** Welcome to the BBS server. **\n********************************\n");
    return;
}

void Exit(int sockfd, const vector<string> &para){
    if(para.size() != 1){
        write2cli(sockfd, "Usage: exit\n");
        write2cli(sockfd, "% ");
        return;
    }

    if(isLogin[sockfd]) 
        write2cli2(sockfd, "Bye", user[sockfd].c_str());

    isLogin[sockfd] = false;
    user2isLogin[user[sockfd]] = false;
    user[sockfd].clear();                

    Close(sockfd);

    FD_CLR(sockfd, &all_set);

    return;
}

void reg(int sockfd, const vector<string> &para){
    if(para.size() != 3){
        write2cli(sockfd, "Usage: register <username> <password>\n");
        return;
    }

    auto itr = user2password.find(para[1]);
    if(itr != user2password.end()){
        write2cli(sockfd, "Username is already used.\n");
        return;
    }

    write2cli(sockfd, "Register successfully.\n");
    user2password[para[1]] = para[2];
    return;
}

void login(int sockfd, const vector<string> &para){
    if(para.size() != 3){
        write2cli(sockfd, "Usage: login <username> <password>\n");
        return;
    }

    if(isLogin[sockfd]){ //this client is login
        write2cli(sockfd, "Please logout first.\n");
        return;
    }

    if(user2isLogin[para[1]]){ //this user is login
        write2cli(sockfd, "Please logout first.\n");
        return;
    }

    auto itr = user2password.find(para[1]);
    if(itr == user2password.end()){ //user does not exist
        write2cli(sockfd, "Login failed.\n");
        return;
    }

    if(user2isBlack[para[1]]){ //user is in blacklist
        snprintf(cli_buff, sizeof(cli_buff), "We don't welcome %s!\n", para[1].c_str());
        Write(sockfd, cli_buff, strlen(cli_buff));
    }

    if(user2password[para[1]] != para[2]){ //password is wrong
        write2cli(sockfd, "Login failed.\n");
        return;
    }

    write2cli2(sockfd, "Welcome", para[1].c_str());
    isLogin[sockfd] = true;
    user2isLogin[para[1]] = true;
    user[sockfd] = para[1];
}

void logout(int sockfd, const vector<string> &para){
    if(para.size() != 1){
        write2cli(sockfd, "Usage: logout\n");
        return;
    }

    if(!isLogin[sockfd]){
        write2cli(sockfd, "Please login first.\n");
        return;
    }

    write2cli2(sockfd, "Bye", user[sockfd].c_str());
    isLogin[sockfd] = false;
    user2isLogin[user[sockfd]] = false;
    user[sockfd].clear();
    return;
}

bool isNum(string port){
    for(int i = 0 ; i < port.size() ; i++){
        if(port[i] < '0' || port[i] > '9') return false;
    }

    return true;
}

int string2int(string port){
    int num = 0;

    for(int i = 0 ; i < port.size() ; i++)
        num = 10 * num + port[i] - '0';
    
    return num;
}

void show_msg(int sockfd, int port, int version){
    write2cli(sockfd, "Welcome to public chat room.\n");
    
    snprintf(cli_buff, sizeof(cli_buff), "Port:%d\n", port);
    Write(sockfd, cli_buff, strlen(cli_buff));

    snprintf(cli_buff, sizeof(cli_buff), "Version:%d\n", version);
    Write(sockfd, cli_buff, strlen(cli_buff));

    for(string msg : chat_history){
        snprintf(cli_buff, sizeof(cli_buff), "%s\n", msg.c_str());
        Write(sockfd, cli_buff, strlen(cli_buff));
    }

    return;
}

void enter_room(int sockfd, const vector<string> &para){
    if(para.size() != 3){
        write2cli(sockfd, "Usage: enter-chat-room <port> <version>\n");
        return;
    }

    if(!isNum(para[1])){ //if port is not a number
        snprintf(cli_buff, sizeof(cli_buff), "Port %s is not valid.\n", para[1].c_str());
        Write(sockfd, cli_buff, strlen(cli_buff));
        return;
    }

    int port = string2int(para[1]);
    if(port > 65535 || port < 1){ //if port is too big or too small
        snprintf(cli_buff, sizeof(cli_buff), "Port %s is not valid.\n", para[1].c_str());
        Write(sockfd, cli_buff, strlen(cli_buff));
        return;
    }

    if(!isNum(para[2])){ //if version is not a number
        snprintf(cli_buff, sizeof(cli_buff), "Version %s is not supported.\n", para[2].c_str());
        Write(sockfd, cli_buff, strlen(cli_buff));
        return;
    }

    int version = string2int(para[2]);
    if(version != 1 && version != 2){ //if version is not 1 or 2
        snprintf(cli_buff, sizeof(cli_buff), "Version %s is not supported.\n", para[2].c_str());
        Write(sockfd, cli_buff, strlen(cli_buff));
        return;
    }

    if(!isLogin[sockfd]){ //client is not login
        write2cli(sockfd, "Please login first.\n");
        return;
    }

    show_msg(sockfd, port, version);
    user2port[user[sockfd]] = port;
    user2version[user[sockfd]] = version;
    return;
}

void bbs_main(int sockfd){
    string command = Read_line(sockfd);
    if(command[0] != 0){
        vector<string> para = split(command);

        for(int i = 0 ; i < para.size() ; i++){
            int j = para[i].size() - 1;
            while(para[i][j-1] == '\n') {
                para[i].pop_back();
                j--;
            }
        }

        if(para[0] == "exit"){
            Exit(sockfd, para);
            return;
        }

        if(para[0] == "register") reg(sockfd, para);
        else if(para[0] == "login") login(sockfd, para);
        else if(para[0] == "logout") logout(sockfd, para);
        else if(para[0] == "enter-chat-room") enter_room(sockfd, para);
    }
    write2cli(sockfd, "% ");
}

string Read_line_udp(int udpfd, struct sockaddr* cli_addr_ptr, int len){
    socklen_t Len = len;

    bzero(srv_buff, sizeof(srv_buff));
    Recvfrom(udpfd, srv_buff, sizeof(srv_buff), 0, cli_addr_ptr, &Len);
    string msg(srv_buff);

    return msg;
}

void chat(int udpfd, struct sockaddr* cli_addr_ptr, int len, vector<string> para){
    if(para.size() != 2){
        snprintf(cli_buff, sizeof(cli_buff), "Usage: chat <message>\n");
        Sendto(udpfd, cli_buff, strlen(cli_buff), 0, cli_addr_ptr, len);
        return;
    }

    for(map<string, int>::iterator it = user2port.begin() ; it != user2port.end() ; it++){
        int port = it->second;

        struct sockaddr_in cli_addr;
        bzero(&cli_addr, sizeof(cli_addr));
        cli_addr.sin_family = AF_INET;
        cli_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        cli_addr.sin_port = htons(port);

        snprintf(cli_buff, sizeof(cli_buff), ":%s\n", para[1].c_str());
        Sendto(udpfd, cli_buff, strlen(cli_buff), 0, (struct sockaddr*) &cli_addr, len);
    }
    
    return;
}

vector<string> get_chat_para(string command){
    int i = 0;
    string tmp;
    vector<string> para;

    while(i < command.size() && command[i] != ' ' && command[i] != '\n'){
        tmp += command[i];
        i++;
    }

    while(i < command.size() && command[i] == ' ')
        i++;

    para.push_back(tmp);
    tmp.clear();

    if(para[0] != "chat") return para;

    while(i < command.size() && command[i] != '\n'){
        tmp += command[i];
        i++;    
    }

    para.push_back(tmp);

    return para;
}

void udp_main(int udpfd, struct sockaddr* cli_addr_ptr, int len){
    string command = Read_line_udp(udpfd, cli_addr_ptr, len);

    if(command[0] != 0){
        vector<string> para = get_chat_para(command);

        for(int i = 0 ; i < para.size() ; i++){
            int j = para[i].size() - 1;
            while(!para.empty() && para[i][j] == '\n') {
                para[i].pop_back();
                j--;
            }
        }

        if(para[0] == "chat") chat(udpfd, cli_addr_ptr, len, para);
    }

    snprintf(cli_buff, sizeof(cli_buff), "%s", "% ");
    Sendto(udpfd, cli_buff, strlen(cli_buff), 0, cli_addr_ptr, len);
    return;
}

int main(int argc, char** argv){
    if(argc != 2){
        printf("Usage: ./hw3 <port>\n");
        return 0;
    }

    int listenfd, udpfd, connectfd;
    struct sockaddr_in srv_addr, cli_addr;
    
    /*set server address*/
    int port = char2int(argv[1]);
    bzero(&srv_addr, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    srv_addr.sin_port = htons(port);
    
    /*TCP socket*/
    listenfd = Socket(AF_INET, SOCK_STREAM, 0);    
    const int flag = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int));

    Bind(listenfd, (struct sockaddr *) &srv_addr, sizeof(srv_addr));

    const int backlog = max_size;
    Listen(listenfd, backlog);

    FD_ZERO(&all_set);
    FD_SET(listenfd, &all_set);

    vector<int> cli(max_size, -1);
    int i = 0;
    int maxi = -1;

    /*UDP socket*/
    udpfd = Socket(AF_INET, SOCK_DGRAM, 0);
    setsockopt(udpfd, SOL_SOCKET, SO_REUSEADDR | SO_BROADCAST, &flag, sizeof(int));

    Bind(udpfd, (struct sockaddr *) &srv_addr, sizeof(srv_addr));

    FD_SET(udpfd, &all_set);
    int maxfd = max(listenfd, udpfd);

    while(1){
        memset(cli_buff, 0, sizeof(cli_buff));
        memset(srv_buff, 0, sizeof(srv_buff));

        fd_set rset = all_set;
        int nready = select(maxfd + 1, &rset, NULL, NULL, NULL);

        if(FD_ISSET(listenfd, &rset)){
            connectfd = Accept(listenfd);

            for(i = 0 ; i < max_size ; i++){
                if(cli[i] < 0) {
                    cli[i] = connectfd;
                    break;
                }
            }

            FD_SET(connectfd, &all_set);

            welcome(connectfd);
            write2cli(connectfd, "% ");

            if(connectfd > maxfd) maxfd = connectfd;
            if(i > maxi) maxi = i;
            if(--nready <= 0) continue;
        }

        if(FD_ISSET(udpfd, &rset)){
            const int len = sizeof(cli_addr);
            udp_main(udpfd, (struct sockaddr*) &cli_addr, len);
        }

        for(i = 0 ; i <= maxi ; i++){
            if (cli[i] < 0) continue;
            int sockfd = cli[i];
            if(FD_ISSET(sockfd, &rset)){
                bbs_main(sockfd);
            }
        }

    }
}