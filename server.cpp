#include "Thread_pool.h"
#include<iostream>
#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include<string>
#include<unordered_map>
#include<sys/epoll.h>
#include<fcntl.h>


#define PORT 8080
#define N_CLIENTS 5
#define MAX_EVENTS 10
#define N_WORKERS 5
#define BUFFLEN 1024

int epoll_fd=-1;
int server_socket=-1;
Thread_Pool<N_WORKERS> Worker_thread;



struct HttpRequest{
    std::string method;
    std::string path;

    // std::unordered_map<std::string,std::string>headers;

    std::string body;
};
struct HttpResponse{

    int status_code;
    std::string reason;
    // std::unordered_map<std::string,std::string>headers;
    std::string body;
};

struct Connection{
    int fd;
    std::string read_buffer;
    std::string write_buffer;
    // adding this to ensure thread_safety for this particular resource.
    std::mutex m;
    struct epoll_event event;

    enum class STATE{
        READING,
        PROCESSING,
        WRITING,
        CLOSED
    }state;
    
    HttpRequest req;
    HttpResponse resp;
    Connection(int Cl_fd,struct epoll_event Cl_event):fd(Cl_fd),event(Cl_event){
        read_buffer.reserve(1024);
        write_buffer.reserve(1024);
        state=Connection::STATE::READING;
    };
};

void terminate(){
    //
    //also handle epoll_close,server_fd close
    std::cout<<"Shutting down thread pool"<<std::endl;
    Worker_thread.shutdown();
    std::cout<<"Closing server."<<std::endl;
    close(server_socket);
    std::cout<<"Closing epoll"<<std::endl;
    close(epoll_fd);
    std::cout<<"Terminating program......"<<std::endl;
    std::terminate();
}

void http_request_parser(struct Connection &my_conn){

}
void http_response_parser(){

}

void read_func(struct Connection &my_conn){
    auto lam=[&my_conn](){
        std::unique_lock<std::mutex> lock(my_conn.m);
        while(my_conn.state==Connection::STATE::PROCESSING||my_conn.state!=Connection::STATE::READING){
            //here need to add Conditional_variable to handle it properly.
        }
        my_conn.state=Connection::STATE::PROCESSING;
        char buffer[BUFFLEN];

        int bytes_read;
        while(bytes_read=recv(my_conn.fd,buffer,BUFFLEN-1,0)>0){
            buffer[BUFFLEN]='\0';
            my_conn.read_buffer.append(buffer);
        }
        if(bytes_read!=0){
            std::cerr<<"Error in reading data for :"<<my_conn.fd<<std::endl;
        }
        else{
            http_request_parser(my_conn);
        }
    };
    Worker_thread.submit(lam);
}
void write_func(struct Connection &my_conn){

}


void accept_all(struct epoll_event &event,std::unordered_map<int,Connection>&Live_connections){
    int client_fd;
    sockaddr_in serverAddress;
    socklen_t len_sock;
    while (true)
    {
        client_fd = accept(server_socket, (struct sockaddr *)&serverAddress, &len_sock);
        if (client_fd == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            else
            {
                std::cerr << "Error in accept." << std::endl;
                terminate();
                // also handle epoll_close,server_fd close
            }
        }
        event.events = EPOLLIN;
        event.data.fd = client_fd;
        int flags = fcntl(client_fd, F_GETFL, 0);
        fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event);
        Connection conn(client_fd,event);
        Live_connections.emplace(client_fd,conn);
    }
}

void handle_client(int fd,std::unordered_map<int,Connection>&Live_connections){

    auto ref=Live_connections.find(fd);
    if(ref==Live_connections.end()){
        //error here..
    }
    auto &my_conn=ref->second;
    if(my_conn.event.events==EPOLLIN){
        read_func(my_conn);
        //handle this when state is processing.
    }
    else if(my_conn.event.events==EPOLLOUT){
        write_func(my_conn);
    }
}



int main(){
    //creates a Thread_pool of 5 worker threads.
    Worker_thread.create_pool();
    std::unordered_map<int,Connection>Live_conncetions;//fd as key, and connection struct as value.
    sockaddr_in serverAddress;
    socklen_t len_sock;
    int retcode=0;
    std::string str;
    
    //event is a predefined structure for passing events.
    struct epoll_event event,events[MAX_EVENTS];

    //setting up server to port 8080
    server_socket=socket(AF_INET,SOCK_STREAM,0);
    if(server_socket==-1){
        std::cerr<<"Error creating server_socket.\nTerminating program"<<std::endl;
        terminate();
    }

    serverAddress.sin_family=AF_INET;
    serverAddress.sin_port=htons(PORT);
    serverAddress.sin_addr.s_addr=INADDR_ANY;


    int bind_val=bind(server_socket,(struct sockaddr*)&serverAddress,sizeof(serverAddress));
    if(bind_val==-1){
        std::cerr<<"Error binding server_socket."<<std::endl;
        terminate();
    }
    int listen_val=listen(server_socket,N_CLIENTS);//listening/holding 5 pending connections.
    if(listen_val==-1){
        std::cerr<<"Error listening on server_socket."<<std::endl;
        terminate();
    }

    //creating epoll_fd that will handle all the events we get.
    epoll_fd=epoll_create1(0);
    if(epoll_fd==-1){
        std::cerr<<"Error creating epoll_fd.\nTerminating program"<<std::endl;
        terminate();
    }

    //epoll event starts here.
    event.events=EPOLLIN;
    event.data.fd=server_socket;
    retcode=epoll_ctl(epoll_fd,EPOLL_CTL_ADD,server_socket,&event);
    if(retcode==-1){
        std::cerr<<"Error adding epoll_fd in epoll_ctl."<<std::endl;
        terminate();
    }

    int client_fd;

    while(true){
        int n=epoll_wait(epoll_fd,events,MAX_EVENTS,-1);
        for(int i=0;i<n;i++){
            if(events[i].data.fd==server_socket){
                //accept all-- server event.
                accept_all(events[i],Live_conncetions);   
            }
            else{
                //client event here.
                handle_client(events[i].data.fd,Live_conncetions);
            }
        }
    }




    // close(server_socket);
    terminate();
    return 0;
}


//issues faced

/*

//only includes issues while building this particular program
    (Blocking queue & Thread_pool were built as standalone program, previously).
    
--The first was designing the epoll_event handling,
     differentiating from server_fd_handlilng, and clients_fd_handling.
--Then post clinet_fd readiness, what to do read/write??(leveraged what was event.events_flag was set).
--Post deciding read/wirte, how to get the work done by thread_pool without compromising, data robustness.
    -- solved it using a mutex with each particular client_fd in un_map.
--About handling and isolating the map_oprations plus, also controlling few map_ops so to check its
    safety before performing ops.
    --leveraged how un_maps are built.
    --each pair in un_map has {key,value},
    --now rehash only modifies the ref of this whole KV obj, the internal VALUE obj's ref remains same
        till it is alive.
    --Giving us leverage over ref for VALUE(rehash safe ref), can work independently on these refs.
*/