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
#include <sstream>
#include <set>
#define MYPORT "30659"  // the port users will be connecting to
#define MAINPORT "33659" //TODO: Change this
#define MAXDATASIZE 1024 // max request size, unlikely to be larger than this

// HELPER FUNCTIONS

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

// Reads the data from data#.csv where # is A, B or C
void readData(std::unordered_map<std::string, double> &find_gpas, std::unordered_map<std::string, std::vector<std::string>> &dept_to_ids, std::string filename){
    std::ifstream infile;
    infile.open(filename);
    std::string line;
    infile >> line; // doing this to discard the first line
    while(infile >> line){
        // split the std::string into a queue of std::strings
        std::deque<std::string> split_queue = split(line, ",");
        // extract dept and studentID
        std::string dept = split_queue.front();
        split_queue.pop_front();
        std::string id = split_queue.front();
        split_queue.pop_front();
        // get the gpa of this student
        int count = 0;
        long total = 0;
        double gpa;    
        for(std::string grade : split_queue){
            if(grade != ""){
                count++;
                total += stol(grade);
            }
        }
        gpa = ((double) total)/count;
        find_gpas[id] = gpa;
        if(dept_to_ids.find(dept) == dept_to_ids.end()){
            std::vector<std::string> new_list;
            new_list.push_back(id);
            dept_to_ids[dept] = new_list;
        }
        else{
            dept_to_ids[dept].push_back(id);
        }
    }
}

// Gathers department level statistics for the admin requests
std::vector<std::string> adminDeptStats(std::unordered_map<std::string, double> find_gpas, std::unordered_map<std::string, std::vector<std::string>> dept_to_ids, std::string dept){
    
    // Department GPA Mean: 47.9
    // Department GPA Variance: 62.7
    // Department Max GPA: 61.4
    // Department Min GPA: 38.9

    std::vector<std::string> results;
    double mean = 0;
    double variance = 0;
    double GPA_max = -1;
    double GPA_min = 1000;
    double total = 0;
    double total_squared = 0;

    std::vector<std::string> students = dept_to_ids[dept];
    for(auto studID : students){
        double gpa = find_gpas[studID];
        
        // update min
        if(gpa < GPA_min){
            GPA_min = gpa;
        }
        if(gpa > GPA_max){
            GPA_max = gpa;
        }
        total += gpa;
        total_squared += gpa*gpa;
    }

    mean = total/students.size();
    variance = total_squared/students.size() - mean*mean;
    
    std::ostringstream mean_strm, var_strm, max_strm, min_strm;
    mean_strm << std::fixed << std::setprecision(2) << mean;
    var_strm << std::fixed << std::setprecision(2) << variance;
    max_strm << std::fixed << std::setprecision(2) << GPA_max;
    min_strm << std::fixed << std::setprecision(2) << GPA_min;
    
    results.push_back(mean_strm.str());
    results.push_back(var_strm.str());
    results.push_back(max_strm.str());
    results.push_back(min_strm.str());
    return results;
}

//Calculate the percentage rank of the student with id: studentID in the class. Precondition: the student belongs to department dept. 
std::string studentRankStats(std::unordered_map<std::string, double> find_gpas, std::unordered_map<std::string, std::vector<std::string>> dept_to_ids, std::string dept, std::string studentID){
    std::vector<std::string> students = dept_to_ids[dept];
    double percentageRank = 0;
    double myGPA = find_gpas[studentID];
    for(auto studID : students){
        double gpa = find_gpas[studID];
        if(myGPA >= gpa){
            percentageRank += 1;
        }
    }
    percentageRank = percentageRank/students.size()*100.0;
    std::ostringstream rank_strm, myGPA_strm;
    rank_strm << std::fixed << std::setprecision(2) << percentageRank;
    myGPA_strm << std::fixed << std::setprecision(2) << myGPA;
    return myGPA_strm.str() + ";" + rank_strm.str();

}



// MAIN FUNCTION
// sets up the sockets used for communication and bindings required to listen. 
int main(void)
{
    int mysockfd, serversockfd;
    std::string myServer = "A"; // defines the server data file to read from and label it prints //TODO: Change
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
    std::unordered_map<std::string, double> find_gpas;
    // map of departments to list of student id's
    std::unordered_map<std::string, std::vector<std::string>> dept_to_ids;

    readData(find_gpas, dept_to_ids, "data" + myServer + ".csv"); 

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
    std::cout << "Server " << myServer << " is up and running using UDP on port " << MYPORT << std::endl;
  
    while(1) {  // respond to requests
        socklen_t addr_len = sizeof their_addr;
        numbytes = recvfrom(mysockfd, buf, MAXDATASIZE - 1, 0, (struct sockaddr *) &their_addr, &addr_len);
        if (numbytes == -1) {
            perror("recvfrom");
            continue;
        }
        buf[numbytes] = '\0';
        std::string request(buf);
        std::string response = ""; // defining the response variable for scoping below

        // special initial request
        if(request == "*list"){
            // send all the departments for which we are available:
            for (const auto & it : dept_to_ids) {
                response += it.first + ";";
            }
            
            std::cout << "Server " + myServer + " has sent a department list to Main Server" << std::endl;
        }
        // get the actual data for the associated request
        else{
            int found = 1;
            std::deque<std::string> request_vec = split(request, ";");
            if(request_vec.size() == 2){
                // Student Rank Query 
                std::string dept = request_vec.front();
                std::string studentID = request_vec.back();
                std::cout << "Server " << myServer << " has received a student academic record query for Student " << studentID << " in " << dept << std::endl;

                // check if student is in this department
                std::vector<std::string> students = dept_to_ids[dept];
                if(std::find(students.begin(), students.end(), studentID) == students.end()){
                    // Element was not present
                    found = 0;
                    response = "Not Found.";
                    std::cout << "Student " << studentID << " does not show up in  " << dept << std::endl;
                    std::cout << "Server " << myServer << " has sent \"Student " << studentID << " not found\" to Main Server" << std::endl;
                }
                else{
                    response = studentRankStats(find_gpas, dept_to_ids, dept, studentID);
                    std::deque<std::string> stats = split(response, ";");
                    std::cout << "Server " << myServer << " calculated following academic statistics for Student " << studentID << " in " << dept << ":" << std::endl;
                    std::cout << "Student GPA: " << stats.front() << std::endl;
                    std::cout << "Student Rank: " << stats.back() << std::endl; 
                }
               

            }
            else{
                // the vector size should be 1
                std::string dept = request_vec.front();
                std::cout << "Server " << myServer << " has received a department academic statistics query for "<< dept << std::endl;
                std::vector<std::string> stats = adminDeptStats(find_gpas, dept_to_ids, dept);
                response = join(stats, ";");
                std::cout << "Server " << myServer << "calculated following academic statistics for " << dept << std::endl;
                std::cout << "Department GPA Mean: " << stats[0] << std::endl;
                std::cout << "Department GPA Variance: " << stats[1] << std::endl;
                std::cout << "Department GPA Max: " << stats[2] << std::endl;
                std::cout << "Department GPA Min: " << stats[3] << std::endl;
            }
            if(found)
                std::cout << "Server " << myServer << "has sent the result to Main Server" << std::endl;
        }
        // SEND RESPONSE
        numbytes = sendto(serversockfd, response.c_str(), response.length(), 0, p->ai_addr, p->ai_addrlen);
        if(numbytes < 0){
            perror("sendto");
            exit(1);
        }
    }
    return 0;
}