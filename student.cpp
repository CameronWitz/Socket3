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
#include <deque>
#include <vector>

#include <arpa/inet.h>

#define PORT "45659"  // the port we will be connecting to

#define MAXDATASIZE 100 // max number of bytes we can get at once 

// process the csv line by line by splitting into a queue
std::deque<std::string> split(std::string line, std::string delim){
    std::deque<std::string> split_queue;
    size_t pos;
    while ((pos = line.find(delim)) != std::string::npos) {
        std::string val  = line.substr(0, pos);
        line = line.substr(pos + delim.length());
        std::cout << "word is : " << val << std::endl;
        split_queue.push_back(val);
    }
    std::cout << "word is : " << line << std::endl;
    split_queue.push_back(line);
    return split_queue;
}

// creates a delimited string 
std::string join(std::vector<std::string> elements, std::string delim){
    std::string ret = "";
    std::string last = elements.back();
    elements.pop_back();
    for(auto elem : elements){
        ret += elem + delim;
    }
    ret += last;
    return ret;
}

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
    int sockfd, numbytes, myPort;  
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

        // Get the dynamic port
        struct sockaddr_storage local_addr;
        socklen_t addr_len = sizeof local_addr;
        getsockname(sockfd, (struct sockaddr*)&local_addr, &addr_len);

        if (local_addr.ss_family == AF_INET) {
            myPort = ntohs(((struct sockaddr_in*)&local_addr)->sin_port);
        } else {
            myPort = ntohs(((struct sockaddr_in6*)&local_addr)->sin6_port);
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
        std::string dept_query, studentID, query;
        std::cout << "Department Name: ";
        std::cin >> dept_query; // read in the query
        std::cout << std::endl << "Student ID: ";
        std::cin >> studentID ;
        std::cout << std::endl;
        
        query = dept_query + ";" + studentID;
        
        if (send(sockfd, query.c_str(), dept_query.length(), 0) == -1){
            perror("send");
            exit(1);
        }

        std::cout << "Client has sent Department " << dept_query << " and " << studentID <<  " to Main Server using TCP over port" << myPort << std::endl;
                
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
            std::deque<std::string> stats = split(response, ";");
            std::cout << "The academic record for Student " << studentID << " in " << dept << "is: " << std::endl;
            std::cout << "Student GPA: " << stats.front() << std::endl << "Percentage Rank: " << stats.back() << std::endl;
        }

        std::cout << "-----Start a new reqyest-----" << std::endl;
    }

    return 0;
   
}