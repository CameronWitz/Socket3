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
#include <unordered_map>

#include <arpa/inet.h>
// used to keep track of which ports in my array belong to which server
#define indexA 0
#define indexB 1
#define indexC 2
#define indexMain 3

#define PORTA "30659"
#define PORTB "31659"
#define PORTC "32659"
#define MAINPORT "33659"

#define MAXDATASIZE 100 // max number of bytes we can get at once 


/*
This function is used to request the department names each backend server is responsible for, and add them 
to a map which is used in the main function of the program. Because it retrieves and stores the information 
from a single backend server at a time, this function also produces the output of displaying what was retrieved.
*/
void askForDepts(int backendServer, int backendServerInd, std::unordered_map<std::string, int> &dept_to_server, sockaddr *ps[], socklen_t ps_len[], int *sockfds){
    // std::cout << "Querying " << backendServer << std::endl;
    int numbytes = sendto(backendServer, "*list", 5, 0, ps[backendServerInd], ps_len[backendServerInd]);
    if(numbytes < 0){
        perror("list request send");
        exit(1);
    }
    char buf[MAXDATASIZE];
    struct sockaddr_storage their_addr;
    socklen_t addr_len = sizeof their_addr;
    numbytes = recvfrom(sockfds[indexMain], buf, MAXDATASIZE - 1, 0, (struct sockaddr *)&their_addr, &addr_len);
    if(numbytes < 0){
        perror("list request recv");
        return;
    }
    //Output
    std::string server_str = backendServerInd == indexA ? "A" : backendServerInd == indexB ? "B" : "C";
    std::cout << "Main server has received the department list from Backend Server " << server_str;
    std::cout << " using UDP over port " << MAINPORT << std::endl;

    buf[numbytes] = '\0';
    std::string response(buf);
    std::cout << "Server " + server_str << std::endl;
    size_t beginning = 0;
    for(size_t i = 0; i < response.length(); i++){
        if(response[i] == ';'){
            std::string dept = response.substr(beginning, i - beginning);
            beginning = i + 1;
            dept_to_server[dept] = backendServerInd;
            std::cout << dept << std::endl;
        }
    }
}


int main(int argc, char *argv[])
{
    int numbytes;
    int sockfds[4] = {0};
    const char* ports[4] = {PORTA, PORTB, PORTC, MAINPORT};

    char buf[MAXDATASIZE];
    sockaddr* ps_addr[4] = {0, 0, 0, 0};
    socklen_t ps_addrlen[4] =  {0, 0, 0, 0};
    struct addrinfo hints, *servinfo, *p;
    int rv;
   
    // Set up sockets, only binding for our port
    for(int i = 0; i < 4; i ++){
        
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET6; // ipv6
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_flags = AI_PASSIVE; // use my IP

        // store linked list of potential hosting ports in servinfo
        if ((rv = getaddrinfo("localhost", ports[i], &hints, &servinfo)) != 0) {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
            return 1;
        }

        // loop through all the results and bind to the first we can
        int mysockfd;
        for(p = servinfo; p != NULL; p = p->ai_next) {
            if ((mysockfd = socket(p->ai_family, p->ai_socktype,
                    p->ai_protocol)) == -1) {
                perror("server: socket");
                continue;
            }
            
            if(i == indexMain){
                if (bind(mysockfd, p->ai_addr, p->ai_addrlen) == -1) {
                    close(mysockfd);
                    perror("server: bind");
                    continue;
                }
            }

            break;
        }

         // all done with this structure

        if (p == NULL)  {
            fprintf(stderr, "server: failed to bind\n");
            exit(1);
        }
        sockfds[i] = mysockfd;
        ps_addr[i] = p->ai_addr;
        ps_addrlen[i] = p->ai_addrlen;
    }

    std::cout << "Main server is up and running." << std::endl;

    // query backend servers for departments
    std::unordered_map<std::string, int> dept_to_server;
    
    askForDepts(sockfds[indexA], indexA, dept_to_server, ps_addr, ps_addrlen, sockfds); // request depts from serverA
    askForDepts(sockfds[indexB], indexB, dept_to_server, ps_addr, ps_addrlen, sockfds); // request depts from serverB
    askForDepts(sockfds[indexC], indexC, dept_to_server, ps_addr, ps_addrlen, sockfds); // request depts from serverC

    while(1){
        std::string dept_query;
        std::cout << "Enter Department Name: ";
        std::cin >> dept_query; // read in the query

        // find the server that this dept is associated with
        if(dept_to_server.find(dept_query) == dept_to_server.end()){
            std::cout << "Department does not show up in Backend servers." << std::endl;
        }
        else{
            int server = dept_to_server[dept_query];
            std::string server_str = server == indexA ? "A" : server == indexB ? "B" : "C"; // get server name based on index
            std::cout << "Department " << dept_query << " shows up in server " << server_str << std::endl;

            struct sockaddr_storage their_addr;
            socklen_t addr_len = sizeof their_addr;
            // request the query
            numbytes = sendto(sockfds[server], dept_query.c_str(), dept_query.length(), 0, ps_addr[server], ps_addrlen[server]);
            if(numbytes < 0){
                perror("request send");
                return -1;
            }

            std::cout << "The Main Server has sent request for " << dept_query << " to server ";
            std::cout << server_str << " using UDP over port " << MAINPORT << std::endl;

            // retrieve the student ids 
            numbytes = recvfrom(sockfds[indexMain], buf, MAXDATASIZE - 1, 0, (struct sockaddr *)&their_addr, &addr_len);
            if(numbytes < 0){
                perror("request recv");
                return -1;
            }
            buf[numbytes] = '\0';
            std::string response(buf);
            size_t beginning = 0;
            
            std::cout << "The Main server has received searching result(s) of " << dept_query;
            std::cout << " from Backend server " << server_str << std::endl;
            // std::cout << "DEBUG: " << response << std::endl;

            std::vector<std::string> ids;
            // parse out the ids
            for(size_t i = 0; i < response.length(); i++){
                if(response[i] == ';'){
                    std::string id = response.substr(beginning, i - beginning);
                    beginning = i + 1;
                    ids.push_back(id);
                }
            }
            std::cout << "There are " << ids.size() << " distinct students in " << dept_query <<"."<<std::endl;
            std::cout << "Their IDs are ";
        
            int firsttime = 1;
            // print the ids 
            for(auto elem : ids){
                std::string out = firsttime ? elem : ", " + elem;
                std::cout << out;
                firsttime = 0;
            }
            std::cout << std::endl << "-----Start a new query-----" << std::endl;
        }

    }
        

    return 0;
   
}