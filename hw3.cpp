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

void bbs_main(int sockfd){
    string command = Read_line(sockfd);
    if(command[0] != 0){
        vector<string> para = split(command);

        for(int i = 0 ; i < para.size() ; i++){
            int j = para[i].size() - 1;
            while(para[i][j-1] == '\n') {
                para[i][j] = 0;
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
    }
    write2cli(sockfd, "% ");
}

int main(int argc, char** argv){
    if(argc != 2){
        printf("Usage: ./hw2 <port>\n");
        return 0;
    }

    int listenfd, connectfd;
    struct sockaddr_in srv_addr;
    
    int port = char2int(argv[1]);
    bzero(&srv_addr, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    srv_addr.sin_port = htons(port);
    
    listenfd = Socket(AF_INET, SOCK_STREAM, 0);    
    int flag = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int));

    Bind(listenfd, (struct sockaddr *) &srv_addr, sizeof(srv_addr));

    const int backlog = max_size;
    Listen(listenfd, backlog);

    FD_ZERO(&all_set);
    FD_SET(listenfd, &all_set);

    vector<int> cli(max_size, -1);
    int i = 0;
    int maxi = -1;
    int maxfd = listenfd;

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

        for(i = 0 ; i <= maxi ; i++){
            if (cli[i] < 0) continue;
            int sockfd = cli[i];
            if(FD_ISSET(sockfd, &rset)){
                bbs_main(sockfd);
            }
        }
    }
}