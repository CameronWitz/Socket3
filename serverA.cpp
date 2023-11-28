/*
** server.cpp -- a stream socket server 
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <unordered_map>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <deque>
#include <vector>
#include <set>
#define MYPORT "30659"  // the port users will be connecting to
#define MAINPORT "33659"
#define MAXDATASIZE 1024 // max request size, unlikely to be larger than this
using namespace std;

// process the csv line by line by splitting into a queue
deque<string> split(string line, string delim){
    deque<string> split_queue;
    size_t pos;
    while ((pos = line.find(delim)) != string::npos) {
        string val  = line.substr(0, pos);
        line = line.substr(pos + delim.length());
        cout << "word is : " << val << endl;
        split_queue.push_back(val);
    }
    cout << "word is : " << line << endl;
    split_queue.push_back(line);
    return split_queue;
}


// Reads the data from data#.csv where # is A, B or C
void readData(unordered_map<string, double> &find_gpas, unordered_map<string, vector<string>> &dept_to_ids, string filename){
    ifstream infile;
    infile.open(filename);
    string line;
    infile >> line; // doing this to discard the first line
    while(infile >> line){
        // split the string into a queue of strings
        deque<string> split_queue = split(line, ",");
        // extract dept and studentID
        string dept = split_queue.front();
        split_queue.pop_front();
        string id = split_queue.front();
        split_queue.pop_front();
        // get the gpa of this student
        int count = 0;
        long total = 0;
        double gpa;    
        for(string grade : split_queue){
            if(grade != ""){
                count++;
                total += stol(grade);
            }
        }
        gpa = ((double) total)/count;
        find_gpas[id] = gpa;
        if(dept_to_ids.find(dept) == dept_to_ids.end()){
            vector<string> new_list;
            new_list.push_back(id);
            dept_to_ids[dept] = new_list;
        }
        else{
            dept_to_ids[dept].push_back(id);
        }
    }
}

int main(void)
{
    int mysockfd, serversockfd;
    string myServer = "A"; // defines the server data file to read from and label it prints
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    struct sockaddr_storage their_addr;
    char buf[MAXDATASIZE];

    //Note: From Beejs guide
    // First set up for listening on our port
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET6; // ipv6
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    
    // KEEPING STATS FOR
    
    // map of student id's to GPA's
    unordered_map<string, double> find_gpa;
    // map of departments to list of student id's
    unordered_map<string, vector<string>> dept_to_ids;

    readData(find_gpa, dept_to_ids, "data" + myServer + ".csv"); 

    //Note: From Beejs guide
    // store linked list of potential hosting ports in servinfo 
    if ((rv = getaddrinfo("localhost", MYPORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

     //Note: From Beejs guide
    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((mysockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (bind(mysockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(mysockfd);
            perror("server: bind");
            continue;
        }

        break;
    }
    //Note: From Beejs guide
    freeaddrinfo(servinfo); // all done with this structure
    //Note: From Beejs guide
    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    // now setup for sending to their port..
    // clear this out again
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET6; // ipv6
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    // store linked list of potential hosting ports in servinfo
    if ((rv = getaddrinfo("localhost", MAINPORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((serversockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }
        // don't need to bind to servermain socket
        break;
    }

    // TODO: see if commenting in is going to break
    freeaddrinfo(servinfo); // all done with this structure
    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }


    // SETUP IS DONE
    cout << "Server " << myServer << " is up and running using UDP on port " << MYPORT << endl;
  
    while(1) {  // respond to requests
        socklen_t addr_len = sizeof their_addr;
        numbytes = recvfrom(mysockfd, buf, MAXDATASIZE - 1, 0, (struct sockaddr *) &their_addr, &addr_len);
        if (numbytes == -1) {
            perror("recvfrom");
            continue;
        }
        buf[numbytes] = '\0';
        string request(buf);

        
        string response = "";
        // special initial request
        if(request == "*list"){
            // send all the departments for which we are available:
            for (const auto & it : dept_to_ids) {
                response += it.first + ";";
            }
            
            cout << "Server " + myServer + " has sent a department list to Main Server" << endl;
        }

        // get the actual data for the associated request
        else{
            deque<string> request_vec = split(request, ";");
            cout << "Server " + myServer + " has received a request for " << request << endl;
            int found = 0;
            if(dept_to_ids.find(request) != dept_to_ids.end())
                found = 1;
            
            if(found){
                cout << "Server " + myServer + " found " << dept_to_ids[request].size() << " distinct students for " << request << ": ";
                int first = 1;
                for(auto &elem : dept_to_ids[request]){
                    string out = first ? elem : ", " + elem;
                    cout << out;
                    response += elem + ";";
                    first = 0;
                }
                cout << endl;
            }
            else{
                response = "Not Found.";
            }
        }
        // SEND RESPONSE
        numbytes = sendto(serversockfd, response.c_str(), response.length(), 0, p->ai_addr, p->ai_addrlen);
        if(numbytes < 0){
            perror("sendto");
            exit(1);
        }
        if(request != "*list")
            cout << "Server " << myServer << " has sent the results to Main Server" << endl;

    }

    return 0;
}