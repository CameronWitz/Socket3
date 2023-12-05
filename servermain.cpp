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
#include <sys/wait.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <deque>
#include <unordered_map>
#include <signal.h>


#include <arpa/inet.h>
// used to keep track of which ports in my array belong to which server
#define BACKLOG 10  // how many pending connections queue will hold

#define indexA 0
#define indexB 1
#define indexC 2
#define indexMainUDP 3
#define indexMainTCP 4

#define PORTA "41659"
#define PORTB "42659"
#define PORTC "43659"
#define UDP_MAINPORT "44659"
#define TCP_MAINPORT "45659"

#define MAXDATASIZE 100 // max number of bytes we can get at once 

// HELPER FUNCTIONS

// process the csv line by line by splitting into a queue
std::deque<std::string> split(std::string line, std::string delim){
    //  std::cout << "DEBUG: line is " << line << std::endl;
    std::deque<std::string> split_queue;
    size_t pos;
    while ((pos = line.find(delim)) != std::string::npos) {
        std::string val  = line.substr(0, pos);
        line = line.substr(pos + delim.length());
        // std::cout << "word is : " << val << std::endl;
        split_queue.push_back(val);
    }
    // std::cout << "word is : " << line << std::endl;
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

// invoked due to sigaction
void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}


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
    numbytes = recvfrom(sockfds[indexMainUDP], buf, MAXDATASIZE - 1, 0, (struct sockaddr *)&their_addr, &addr_len);
    if(numbytes < 0){
        perror("list request recv");
        return;
    }
    //Output
    std::string server_str = backendServerInd == indexA ? "A" : backendServerInd == indexB ? "B" : "C";
    std::cout << "Main server has received the department list from Backend Server " << server_str;
    std::cout << " using UDP over port " << UDP_MAINPORT << std::endl;

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

std::string handleQueryA(int backendServer, int backendServerInd, sockaddr *ps[], socklen_t ps_len[], int *sockfds, std::string dept, std::string studentID){
    std::string query = dept + ";" + studentID;
    int numbytes = sendto(backendServer, query.c_str(), query.size(), 0, ps[backendServerInd], ps_len[backendServerInd]);
    if(numbytes < 0){
        perror("Query A send");
    }
    std::string server_str = backendServerInd == indexA ? "A" : backendServerInd == indexB ? "B" : "C";
    std::cout << "Main Server has sent request to server " << server_str << " using UDP over port " << UDP_MAINPORT << std::endl;
    
    char buf[MAXDATASIZE];
    struct sockaddr_storage their_addr;
    socklen_t addr_len = sizeof their_addr;
    numbytes = recvfrom(sockfds[indexMainUDP], buf, MAXDATASIZE - 1, 0, (struct sockaddr *)&their_addr, &addr_len);
    if(numbytes < 0){
        perror("Receive Query A");
        return "";
    }
    buf[numbytes] = '\0';
    std::string response(buf);
    std::cout << "Main Server has received searching result of Student " << studentID << " from server " << server_str << std::endl;
    return response;
}

std::string handleQueryB(int backendServer, int backendServerInd, sockaddr *ps[], socklen_t ps_len[], int *sockfds, std::string dept){
    std::string query = dept;
    int numbytes = sendto(backendServer, query.c_str(), query.size(), 0, ps[backendServerInd], ps_len[backendServerInd]);
    if(numbytes < 0){
        perror("Query B send");
    }
    std::string server_str = backendServerInd == indexA ? "A" : backendServerInd == indexB ? "B" : "C";
    std::cout << "Main Server has sent request to server " << server_str << " using UDP over port " << UDP_MAINPORT << std::endl;
    
    char buf[MAXDATASIZE];
    struct sockaddr_storage their_addr;
    socklen_t addr_len = sizeof their_addr;
    numbytes = recvfrom(sockfds[indexMainUDP], buf, MAXDATASIZE - 1, 0, (struct sockaddr *)&their_addr, &addr_len);
    if(numbytes < 0){
        perror("Receive Query B");
        return "";
    }
    buf[numbytes] = '\0';
    std::string response(buf);
    return response;
}

int main(int argc, char *argv[])
{
    int numbytes;
    int sockfds[5] = {0};
    const char* ports[5] = {PORTA, PORTB, PORTC, UDP_MAINPORT, TCP_MAINPORT};

    char buf[MAXDATASIZE];
    sockaddr* ps_addr[4] = {0, 0, 0, 0};
    socklen_t ps_addrlen[4] =  {0, 0, 0, 0};
    struct addrinfo hints, *servinfo, *p;
    int rv;
    
    // UDP SECTION
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
            
            if(i == indexMainUDP){
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
    //////////////////////////////////////////////////////////////////////////////////////
    //
    // TCP SECTION
    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints2, *servinfo2, *p2;
    struct sockaddr_storage their_addr;//, my_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int clients = 0;
    int yes=1;
    int rv2;
    // Specify the type of connection we want to host
    memset(&hints2, 0, sizeof hints2);
    hints2.ai_family = AF_UNSPEC; //IPV4 and IPV6 both fine
    hints2.ai_socktype = SOCK_STREAM; // TCP
    hints2.ai_flags = AI_PASSIVE; // use my IP
    // store linked list of potential hosting ports in servinfo
    if ((rv2 = getaddrinfo("localhost", TCP_MAINPORT, &hints2, &servinfo2)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    // loop through all the results and bind to the first we can
    for(p2 = servinfo2; p2 != NULL; p2 = p->ai_next) {
        if ((sockfd = socket(p2->ai_family, p2->ai_socktype,
                p2->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p2->ai_addr, p2->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }
    freeaddrinfo(servinfo2); // all done with this structure
    if (p2 == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    while(1){
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }
        clients ++;
        if (!fork()) { // this is the child process
            int cur_client = clients;
            close(sockfd); // child doesn't need the listener

            while(1){
                // Respond to any queries from the client
                int numbytes;
                char buf[MAXDATASIZE];

                if ((numbytes = recv(new_fd, buf, MAXDATASIZE-1, 0)) == -1) {
                    perror("recv");
                    exit(1);
                }

                if(numbytes == 0){
                    // std::cout << "Client closed connection, this process will exit" << std::endl;
                    close(new_fd);
                    exit(0);
                }

                buf[numbytes] = '\0';
                std::string request(buf);

                std::deque<std::string> request_split = split(request, ";");
                std::string type = request_split.front();
                request_split.pop_front();
                std::string queryType = "A";
                if(type == "admin"){
                    // admin request
                    if(request_split.size() == 1){
                        queryType = "B";
                    }
                }
                // student request
                std::string dept, studentID, reply;
                dept = request_split.front();
                if(queryType == "A"){
                    studentID = request_split.back();
                    std::cout << "Main server has received the request on Student " << studentID << 
                    " in " << dept << " from " << type << " client " ;
                    if(type == "student"){
                        std::cout << cur_client;
                    }
                    std::cout << " using TCP over port " << TCP_MAINPORT << std::endl;
                }
                
                // Check if the department exists
       
                if(dept_to_server.find(dept) == dept_to_server.end()){
                    reply = "Not Found Department"; 
                    std::cout << "Department " << dept << " does not show up in backend server A, B, C ";
                    std::cout << std::endl;
     
                }
                else{
                    // Handle the request
                    std::string server_str = dept_to_server[request] == indexA ? "A" : dept_to_server[request] == indexB ? "B" : "C"; // get server name based on index
                    std::cout << dept << " shows up in server " << server_str << std::endl;
                    reply = queryType == "A" ? 
                    handleQueryA(sockfds[dept_to_server[dept]], dept_to_server[dept], ps_addr, ps_addrlen, sockfds, dept, studentID) :
                    handleQueryB(sockfds[dept_to_server[dept]], dept_to_server[dept], ps_addr, ps_addrlen, sockfds, dept);
                }

                if (send(new_fd, reply.c_str(), reply.length(), 0) == -1){
                    perror("send");
                }
                if(reply == "Not Found" ){
                    std::string server_str = dept_to_server[request] == indexA ? "A" : dept_to_server[request] == indexB ? "B" : "C"; // get server name based on index
                    std::cout << "Main Server has received “Student " << studentID << ": Not Found” from server " ;
                    std::cout << server_str << std::endl;
                    std::cout << "Main server has sent message to " << type << " client ";
                     if(type == "student"){
                        std::cout << cur_client;
                    }
                    std::cout << " using TCP over port " << TCP_MAINPORT << std::endl;
                }
                // Successful query so print output
                else{
                    if(queryType == "A"){
                        std::cout << "Main Server has sent result(s) to " << type << " client ";
                        if(type == "student"){
                            std::cout << cur_client;
                        }
                        std::cout << " using TCP over port " << TCP_MAINPORT << std::endl;
                    }
                    else{
                        std::cout << "Main Server has sent department statistics to Admin client using TCP over port " <<
                        TCP_MAINPORT << std::endl;
                    }
                }

            }
        }

        close(new_fd);  // parent doesn't need this
    }
    
    return 0;
   
}