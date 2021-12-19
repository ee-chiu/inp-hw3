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
#include <stdlib.h>

using namespace std;

char cli_buff[10000];
char srv_buff[10000];
char udp_buff1[2000];
char udp_buff2[2000];
const int max_size = 20;
vector<bool> isLogin(max_size, false);
vector<string> user(max_size, "");
map<string, string> user2password;
map<string, bool> user2isLogin;
map<string, bool> user2isBlack;
vector<string> chat_history;
vector<int> ports;
map<int, int> port2version;

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
    for(int i = 0 ; i < (int) port.size() ; i++){
        if(port[i] < '0' || port[i] > '9') return false;
    }

    return true;
}

int string2int(string port){
    int num = 0;

    for(int i = 0 ; i < (int) port.size() ; i++)
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
    ports.push_back(port);
    port2version[port] = version;
    return;
}

void bbs_main(int sockfd){
    string command = Read_line(sockfd);
    if(command[0] != 0){
        vector<string> para = split(command);

        for(int i = 0 ; i < (int) para.size() ; i++){
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

void get_packet(int udpfd, struct sockaddr* cli_addr_ptr, socklen_t len){
    bzero(srv_buff, sizeof(srv_buff));
    Recvfrom(udpfd, srv_buff, sizeof(srv_buff), 0, cli_addr_ptr, &len);
    return;
}

struct a {
    unsigned char flag;
    unsigned char version;
    unsigned char payload[0];
} __attribute__((packed));

struct b {
    unsigned short len;
    unsigned char data[0];
} __attribute__((packed));

struct c {
    unsigned char data[0];
} __attribute__((packed));

unsigned char* get_data(unsigned short len, struct b* pb){
    unsigned char* data = (unsigned char *) calloc(len, sizeof(char));
    bzero(data, len);
    memcpy(data, pb->data, len);
    return data;
}

unsigned short get_len2(unsigned char* tmp_data){
    unsigned short name_len = 0;
    for( ; *(tmp_data) != '\n' ; tmp_data++, name_len++) ;

    return name_len;
}

unsigned char* get_data2(unsigned short len, struct c* pc){
    unsigned char* data = (unsigned char *) calloc(len, sizeof(char));
    bzero(data, len);
    memcpy(data, pc->data, len);
    return data;
}

string binary(char c){
    string tmp;
    while(c != 0) {
        tmp.push_back(c % 2 + '0');
        c /= 2;
    }

    while(tmp.size() < 8) tmp.push_back('0');

    std::reverse(tmp.begin(), tmp.end());
    return tmp;
}

string get_bits(const unsigned char* data, const unsigned short len){
    string bits;
    for(int i = 0 ; i < len ; i++) bits += binary(data[i]);
    while(bits.size() % 6 != 0) bits.push_back('0');
    while(bits.size() % 24 != 0) bits.push_back('-');

    return bits;
}

string encode(const unsigned char* data, const unsigned short len, int &encode_len){
    string bits = get_bits(data, len);
    string result;
    int i = 0;

    for(i = 0 ; i < (int) bits.size() && bits[i] != '-'; i += 6){
        int index = 0;
        index += 32 * (bits[i] - '0');
        index += 16 * (bits[i + 1] - '0');
        index +=  8 * (bits[i + 2] - '0');
        index +=  4 * (bits[i + 3] - '0');
        index +=  2 * (bits[i + 4] - '0');
        index +=  1 * (bits[i + 5] - '0');

        char c = '|';
        if(index <= 25) c = 'A' + index;
        else if(index >= 26 && index <= 51) c = 'a' + (index - 26);
        else if(index >= 52 && index <= 61) c = '0' + (index - 52);
        else if(index == 62) c = '+';
        else if(index == 63) c = '/';

        result.push_back(c);
    }

    for( ; i < (int) bits.size() ; i += 6)  result.push_back('=');
    
    result.push_back('\n');

    encode_len = result.size();

    return result;
}

string binary2(int n){
    string tmp;
    while(n != 0) {
        tmp.push_back(n % 2 + '0');
        n /= 2;
    }

    while(tmp.size() < 6) tmp.push_back('0');

    std::reverse(tmp.begin(), tmp.end());
    return tmp;
}

string get_bits2(const unsigned char* data, const unsigned short len){
    string bits;
    for(int i = 0 ; i < len ; i++) {
        if(data[i] >= 'A' && data[i] <= 'Z') bits += binary2(data[i] - 'A');
        else if(data[i] >= 'a' && data[i] <= 'z') bits += binary2(data[i] - 'a' + 26);
        else if(data[i] >= '0' && data[i] <= '9') bits += binary2(data[i] - '0' + 52);
        else if(data[i] == '+') bits += binary2(62);
        else if(data[i] == '/') bits += binary2(63);
    }

    if(data[len-2] == '=')
        for(int i = 1 ; i <= 4 ; i++) bits.pop_back();

    else if(data[len-1] == '=')
        for(int i = 1 ; i <= 2 ; i++) bits.pop_back();

    return bits;
}

string decode(const unsigned char* data, const unsigned short len, int &decode_len){
    string bits = get_bits2(data, len);
    string result;

    for(int i = 0 ; i < (int) bits.size() ; i += 8){
        char c = 0;
        c += 128 * (bits[i] - '0');
        c +=  64 * (bits[i + 1] - '0');
        c +=  32 * (bits[i + 2] - '0');
        c +=  16 * (bits[i + 3] - '0');
        c +=   8 * (bits[i + 4] - '0');
        c +=   4 * (bits[i + 5] - '0');
        c +=   2 * (bits[i + 6] - '0');
        c +=   1 * (bits[i + 7] - '0');

        result.push_back(c);
    }

    decode_len = result.size();
    return result;
}

void add_history(const char* name, const char* msg){  
    string name_str(name);
    string msg_str(msg);
    string chat = name_str + ":" + msg_str;
    chat_history.push_back(chat);

    return;
}

void udp_main(int udpfd, struct sockaddr* cli_addr_ptr, socklen_t len){
    bzero(&udp_buff1, sizeof(udp_buff1));
    bzero(&udp_buff2, sizeof(udp_buff2));
    
    get_packet(udpfd, cli_addr_ptr, len);
    struct a* pa = (struct a *) srv_buff;
    int version = pa->version;

    if(version == 1){
        memcpy(udp_buff1, srv_buff, sizeof(udp_buff1));

        /*analyze version 1 packet*/
        struct b* pb1 = (struct b *) (srv_buff + sizeof(struct a));
        unsigned short name_len = ntohs(pb1->len);
        unsigned char* name = get_data(name_len, pb1);
        struct b* pb2 = (struct b *) (srv_buff + sizeof(struct a) + sizeof(struct b) + name_len);
        unsigned short msg_len = ntohs(pb2->len);
        unsigned char* msg = get_data(msg_len, pb2);

        /*set version 2 packet*/
        struct a* pA = (struct a *) udp_buff2;
        pA->flag = 0x01;
        pA->version = 0x02;

        int name_encode_len = -1;
        string name_encode_str = encode(name, name_len, name_encode_len);
        const char* name_encode = name_encode_str.c_str();
        struct c* pC1 = (struct c *) (udp_buff2 + sizeof(struct a));
        memcpy(pC1->data, name_encode, name_encode_len);

        int msg_encode_len = -1;
        string msg_encode_str = encode(msg, msg_len, msg_encode_len);
        const char* msg_encode = msg_encode_str.c_str();
        struct c* pC2 = (struct c *) (udp_buff2 + sizeof(struct a) + name_encode_len);
        memcpy(pC2->data, msg_encode, msg_encode_len);

        add_history((const char *) name, (const char *) msg);

        for(int port : ports){
            struct sockaddr_in cli_addr;
            bzero(&cli_addr, sizeof(cli_addr));
            cli_addr.sin_family = AF_INET;
            cli_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
            cli_addr.sin_port = htons(port);
            
            int this_version = port2version[port];

            if(this_version == 1) {
                int size = 1 + 1 + 2 + name_len + 2 + msg_len;
                Sendto(udpfd, udp_buff1, size, 0, (struct sockaddr *) &cli_addr, len);
            }

            else if(this_version == 2) {
                int size = 1 + 1 + name_encode_len + msg_encode_len;
                Sendto(udpfd, udp_buff2, size, 0, (struct sockaddr *) &cli_addr, len);
            }
        }
    }

    else if(version == 2){
        memcpy(udp_buff2, srv_buff, sizeof(udp_buff2));

        /*analyze version 2 packet*/
        unsigned char* tmp_data = (unsigned char *) (srv_buff + sizeof(struct a));
        unsigned short name_len = get_len2(tmp_data);
        struct c* pc1 = (struct c *) (srv_buff + sizeof(struct a));
        unsigned char* name = get_data2(name_len, pc1);

        unsigned char* tmp_data2 = (unsigned char *) (srv_buff + sizeof(struct a) + (name_len + 1));
        unsigned short msg_len = get_len2(tmp_data2);
        struct c* pc2 = (struct c *) (srv_buff + sizeof(struct a) + (name_len + 1));
        unsigned char* msg = get_data2(msg_len, pc2);

        /*set version 1 packet*/
        struct a* pA = (struct a *) udp_buff1;
        pA->flag = 0x01;
        pA->version = 0x01;
        
        int name_decode_len = -1;
        string name_decode_str = decode(name, name_len, name_decode_len);
        const char* name_decode = name_decode_str.c_str();
        struct b* pB1 = (struct b *) (udp_buff1 + sizeof(struct a));
        pB1->len = htons(name_decode_len);
        memcpy(pB1->data, name_decode, name_decode_len);
        
        int msg_decode_len = -1;
        string msg_decode_str = decode(msg, msg_len, msg_decode_len);
        const char* msg_decode = msg_decode_str.c_str();
        struct b* pB2 = (struct b *) (udp_buff1 + sizeof(struct a) + sizeof(struct b) + name_decode_len);
        pB2->len = htons(msg_decode_len);
        memcpy(pB2->data, msg_decode, msg_decode_len);

        add_history((const char *) pB1->data, (const char *) pB2->data);

        for(int port : ports){
            struct sockaddr_in cli_addr;
            bzero(&cli_addr, sizeof(cli_addr));
            cli_addr.sin_family = AF_INET;
            cli_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
            cli_addr.sin_port = htons(port);
            
            int this_version = port2version[port];
            if(this_version == 1) {
                int size = 1 + 1 + 2 + name_decode_len + 2 + msg_decode_len;
                Sendto(udpfd, udp_buff1, size, 0, (struct sockaddr*) &cli_addr, len);
            }

            else if(this_version == 2) {
                int size = 1 + 1 + name_len + 1 + msg_len + 1;
                Sendto(udpfd, udp_buff2, size, 0, (struct sockaddr*) &cli_addr, len);
            }
        }
    }

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
            const socklen_t len = sizeof(cli_addr);
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