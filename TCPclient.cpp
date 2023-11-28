/*
** client.cpp -- a stream socket client 
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>

#include <arpa/inet.h>

#define PORT "23659"  // the port we will be connecting to

#define MAXDATASIZE 100 // max number of bytes we can get at once 

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
    int sockfd, numbytes;  
    char buf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    // char s[INET6_ADDRSTRLEN];


    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo("localhost", PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    std::cout << "Client is up and running." << std::endl;

    freeaddrinfo(servinfo); // all done with this structure

    // never supposed to exit from here 
    while(1){
        std::string dept_query;
        std::cout << "Enter Department Name: ";
        std::cin >> dept_query; // read in the query

        
        if (send(sockfd, dept_query.c_str(), dept_query.length(), 0) == -1){
            perror("send");
            exit(1);
        }

        std::cout << "Client has sent Department " << dept_query << " to Main Server using TCP." << std::endl;
                
        if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
            perror("recv");
            exit(1);
        }

        // if the connection is closed
        if(numbytes == 0){
            // std::cout << "Connection closed from server, exiting" << std::endl;
            exit(1);
        }

        buf[numbytes] = '\0';
        std::string response(buf);

        // check if not found:
        if(response == "Not Found"){
            std::cout << "Department " << dept_query << " not found." << std::endl;
        }
        else{
            std::cout << "Department " << dept_query << " is associated with server " << response << std::endl;
        }

        std::cout << "-----Start a new query-----" << std::endl;
    }

    return 0;
   
}