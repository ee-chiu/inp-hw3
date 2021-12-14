#include <iostream>
#include <cstdio>
#include <sys/socket.h>
#include <cstring>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

int Socket(int family, int type, int protocol){
    int sockfd = socket(family, type, protocol);
    if(sockfd < 0){
        perror("Socket error");
        exit(0);
    }

    return sockfd;
} 

int Bind(int listenfd, struct sockaddr * ptr_srv_addr, int size){
    int b = bind(listenfd, ptr_srv_addr, size);

    if(b < 0){
        perror("Bind error");
        exit(0);
    }

    return 0;
}

int Listen(int listenfd, int backlog){
    int l = listen(listenfd, backlog);
    
    if(l < 0){
        perror("Listen error");
        exit(0);
    }

    return 0;
}

int Accept(int listenfd){
    int a = accept(listenfd, (struct sockaddr *) NULL, NULL);

    if (a < 0){
        perror("Accept error");
        exit(0);
    }

    return a;
}

int Close(int socket){
    int c = close(socket);
    if (c < 0){
        perror("Close error");
        exit(0);
    }

    return 0;
}

int Read(int socket, void* buffer, unsigned int size){
    int r = read(socket, buffer, size);
    if(r < 0){
        perror("Read error");
        exit(0);
    }

    return r;
}

ssize_t Write(int connfd, const char * cli_buff, size_t nbyte){
    ssize_t w = write(connfd, cli_buff, strlen(cli_buff));
    if (w < 0){
        perror("Write error");
        exit(0);
    }

    return w;
}

int Recvfrom(int sockfd, void *buff, size_t nbytes, int flags, 
              struct sockaddr* from, socklen_t *addrlen){
    int r = recvfrom(sockfd, buff, nbytes, flags, from, addrlen);
    if(r < 0){
        perror("Recvfrom error");
        exit(0);
    }

    return r;
}

int Sendto(int sockfd, const void *buff, size_t nbytes, int flags, 
           const struct sockaddr* to, socklen_t addrlen){
    int s = sendto(sockfd, buff, nbytes, flags, to, addrlen);
    if(s < 0){
        perror("Sendto error");
        exit(0);
    }

    return s;
}

int Setsockopt(int sockfd, int level, int optname, 
               const void *optval, socklen_t optlen){
    int s = setsockopt(sockfd, level, optname, optval, optlen);
    if(s < 0){
        perror("Setsockopt");
        exit(0);
    }

    return s;
}


