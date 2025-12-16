#include "Thread_pool.h"
#include<iostream>
#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include<string>
#include<unordered_map>
#include<sys/epoll.h>


#define PORT 8080
#define N_CLIENTS 5
#define MAX_EVENTS 10

struct HttpRequest{
    std::string method;
    std::string path;

    std::unordered_map<std::string,std::string>headers;

    std::string body;
};
struct HttpResponse{

    int status_code;
    std::string reason;
    std::unordered_map<std::string,std::string>headers;
    std::string body;
};

struct Connection{
    int fd;
    std::string read_buffer;
    std::string write_buffer;

    enum class STATE{
        READING,
        PROCESSING,
        WRITING,
        CLOSED
    };
    
    HttpRequest req;
    HttpResponse resp;
};


int main(){
    Thread_Pool<5> Worker_thread;//creates a Thread_pool of 5 worker threads.
    std::unordered_map<int,Connection>Live_conncetions;//fd as key, and connection struct as value.
    sockaddr_in serverAddress;
    int epoll_fd;
    //event is a predefined structure for passing events.
    struct epoll_event event,events[MAX_EVENTS];

    //setting up server to port 8080
    int server_socket=socket(AF_INET,SOCK_STREAM,0);
    if(server_socket==-1){
        std::cerr<<"Error creating server_socket.\nTerminating program"<<std::endl;
        std::terminate();
    }

    serverAddress.sin_family=AF_INET;
    serverAddress.sin_port=htons(PORT);
    serverAddress.sin_addr.s_addr=INADDR_ANY;


    int bind_val=bind(server_socket,(struct sockaddr*)&serverAddress,sizeof(serverAddress));
    int listen_val=listen(server_socket,N_CLIENTS);//listening 5 connections.

    //creating epoll_fd that will handle all the events we get.
    epoll_fd=epoll_create1(0);
    if(epoll_fd==-1){
        std::cerr<<"Error creating epoll_fd.\nTerminating program"<<std::endl;
        std::terminate();
    }

    //epoll event starts here.

    while(true){

    }




    close(server_socket);
    return 0;
}