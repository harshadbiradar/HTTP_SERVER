#include "Thread_pool.h"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string>
#include <cstring>
#include <unordered_map>
#include <sys/epoll.h>
#include <fcntl.h>
#include <utility>
#include<sys/eventfd.h>
#include<memory>

#define PORT 8080
#define N_CLIENTS 1
#define MAX_EVENTS 10
#define N_WORKERS 5
#define BUFFLEN 1024

int epoll_fd = -1;
int server_socket = -1;
int notify_fd;
Thread_Pool<N_WORKERS> Worker_thread;
Blocking_Queue<int>Writable_queue(1000);

struct HttpRequest
{
    std::string method;
    std::string path;

    // std::unordered_map<std::string,std::string>headers;

    std::string body;
};
struct HttpResponse
{

    int status_code;
    std::string reason;
    // std::unordered_map<std::string,std::string>headers;
    std::string body;
};

struct Connection
{
    int fd;
    std::string read_buffer;
    std::string write_buffer;
    // adding this to ensure thread_safety for this particular resource.
    std::mutex m;
    std::condition_variable cv;
    struct epoll_event event;

    enum class STATE
    {
        READING,
        PROCESSING,
        WRITING,
        CLOSED
    } state;

    HttpRequest req;
    HttpResponse resp;
    Connection(int Cl_fd, struct epoll_event Cl_event) : fd(Cl_fd), event(Cl_event)
    {
        read_buffer.reserve(1024);
        write_buffer.reserve(1024);
        state = Connection::STATE::READING;
    }

    Connection(Connection &&other)noexcept{
        event = std::move(other.event);
        fd = other.fd;
        other.fd=-1;
        read_buffer = (std::move(other.read_buffer));
        write_buffer = (std::move(other.write_buffer));
        // read_buffer.reserve(1024);
        // write_buffer.reserve(1024);
        state = std::move(other.state);
    }
    ~Connection(){
        if(fd!=-1){
            //it means the object is out of scope, hence closing connection.
            std::cout<<"[INFO] Connection: "<<fd<<" no longer in use, closing it."<<std::endl;
            close(fd);
        }
    }
};

void terminate()
{
    //
    // also handle epoll_close,server_fd,event_fd close
    std::cout << "Shutting down thread pool" << std::endl;
    Worker_thread.shutdown();
    std::cout << "Closing server." << std::endl;
    close(server_socket);
    std::cout << "Closing epoll" << std::endl;
    close(epoll_fd);
    std::cout << "Terminating program......" << std::endl;
    std::terminate();
}

void http_request_parser(struct std::shared_ptr<Connection>my_conn)
{
    std::cout << my_conn->read_buffer << std::endl;
}
void http_response_parser()
{
}

void read_func(struct std::shared_ptr<Connection>&my_conn)
{
    std::cout << "In read_func" << std::endl;
    auto lam = [my_conn]()
    {
        std::unique_lock<std::mutex> lock(my_conn->m);
        // need to handle closing state
        while (my_conn->state != Connection::STATE::READING)
        {
            my_conn->cv.wait(lock);
            // here need to add Conditional_variable to handle it properly.
        }
        my_conn->state = Connection::STATE::PROCESSING;
        std::cout << "In read_lambda" << std::endl;
        http_request_parser(my_conn);
        my_conn->state = Connection::STATE::WRITING;
        // my_conn->event.events = EPOLLOUT;//this is wrong, should be handled in epoll thread using eventfd.
        // post Business logic.
        my_conn->state = Connection::STATE::WRITING;
        Writable_queue.push(my_conn->fd);
        uint64_t value=1;
        if(write(notify_fd,&value,sizeof(value))==-1){
            std::cerr << "[ERROR]: Error writing in eventfd: "<<strerror(errno)<< std::endl;
            terminate();
        }
        my_conn->cv.notify_all();
    };
    Worker_thread.submit(lam);
}
void write_func(struct std::shared_ptr<Connection> &my_conn)
{
    // just for now.
    std::cout << "In write_func" << std::endl;
    my_conn->write_buffer.append(my_conn->read_buffer);
}

void accept_all(struct epoll_event &event, std::unordered_map<int, std::shared_ptr<Connection>> &Live_connections)
{
    int client_fd;
    sockaddr_in serverAddress;
    socklen_t len_sock;
    int retcode;
    while (true)
    {
        std::cout << "In accept" << std::endl;
        client_fd = accept(server_socket, (struct sockaddr *)&serverAddress, &len_sock);
        std::cout << "Client fd is " << client_fd << std::endl;
        if (client_fd == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            else
            {
                std::cerr << "[ERROR]: Error in accept." <<strerror(errno)<< std::endl;
                terminate();
                // also handle epoll_close,server_fd close
            }
        }
        event.events = EPOLLIN;
        event.data.fd = client_fd;
        int flags = fcntl(client_fd, F_GETFL, 0);
        fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
        retcode=epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event);
        if (retcode == -1)
        {
            std::cerr << "[ERROR]: Error modifying event_fd in epoll_ctl." << std::endl;
            terminate();
        }
        Connection conn(client_fd, event);
        Live_connections.try_emplace(client_fd, std::forward<Connection>(conn));
    }
}

void handle_notify_fd(int notify_fd, std::unordered_map<int, std::shared_ptr<Connection>> &Live_connections)
{
    uint64_t val;
    int retcode;
    
    if(read(notify_fd,&val,8)==-1){
        std::cerr << "[ERROR]: Error reading in eventfd: "<<strerror(errno)<< std::endl;
        terminate();
    }
    while(val--){
        int temp_fd=Writable_queue.try_pop();
        auto ref = Live_connections.find(temp_fd);
        if (ref == Live_connections.end())
        {
            // error here..
        }
        auto &my_conn = ref->second;
        my_conn->event.events=EPOLLOUT;
        retcode = epoll_ctl(epoll_fd, EPOLL_CTL_MOD, temp_fd,&my_conn->event);
        if (retcode == -1)
        {
            std::cerr << "[ERROR]: Error adding socket_fd in epoll_ctl." << std::endl;
            terminate();
        }
    }

}


void handle_client_fd(int fd, std::unordered_map<int, std::shared_ptr<Connection>> &Live_connections){
    // client event here.
    //  handle_client(events[i].data.fd,Live_connections);
    std::cout << "In post server_fd block wait" << std::endl;
    auto ref = Live_connections.find(fd);
    if (ref == Live_connections.end())
    {
        // error here..
    }
    auto &my_conn = ref->second;
    if (my_conn->event.events == EPOLLIN)
    {
        // read_func(my_conn);
        std::cout << "In recving block" << std::endl;
        int bytes_read;
        size_t old_size = my_conn->read_buffer.size();
        my_conn->read_buffer.resize(old_size + BUFFLEN);
        while ((bytes_read = recv(my_conn->fd, &my_conn->read_buffer[old_size], BUFFLEN, 0)) > 0)
        {
            std::cout << "In while recving block with : " << bytes_read << std::endl;
            old_size += bytes_read;
            my_conn->read_buffer.resize(old_size + BUFFLEN);
        }
        std::cout << "post while recving block with : " << bytes_read << std::endl;
        if(bytes_read==0){
            std::cout << "[INFO]: The Client has closed the connection, deleting this Connection." << std::endl;
            //handle post closed connection.
        }
        else if (bytes_read == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            std::cout << "In read_func calling block" << std::endl;
            my_conn->read_buffer.resize(old_size);
            read_func(my_conn);
        }

        // handle this when state is processing.
    }
    else if (my_conn->event.events == EPOLLOUT)
    {
        std::cout << "In writing block" << std::endl;
        write_func(my_conn);
        int bytes_sent=0;
        int old_bytes=0;
        int write_buff_len=strlen(my_conn->write_buffer.c_str());
        char buffer[BUFFLEN];
        while (true)
        {
            old_bytes+=bytes_sent;
            write_buff_len-=bytes_sent;
            strncpy(buffer, &my_conn->write_buffer.c_str()[old_bytes], BUFFLEN - 1);
            buffer[BUFFLEN - 1] = '\0';
            bytes_sent = send(my_conn->fd, buffer, BUFFLEN, 0);
            if (bytes_sent == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
                break;
            if (bytes_sent!=BUFFLEN||bytes_sent <write_buff_len )
            {
                std::cerr << "[ERROR]: Error sending data - Partial data is sent. Retrying...." << std::endl;
                continue;
            }
        }
    }
}

int main()
{
    // creates a Thread_pool of 5 worker threads.
    Worker_thread.create_pool();
    std::unordered_map<int, std::shared_ptr<Connection>> Live_connections; // fd as key, and connection struct as value.
    sockaddr_in serverAddress;
    socklen_t len_sock;
    int retcode = 0;
    std::string str;

    // event is a predefined structure for passing events.
    struct epoll_event event,notify_event, events[MAX_EVENTS];

    // setting up server to port 8080
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
    {
        std::cerr << "[ERROR]: Error creating server_socket.\nTerminating program" << std::endl;
        terminate();
    }

    // making it non_blocing, error handling yet to be done.
    int flags = fcntl(server_socket, F_GETFL, 0);
    fcntl(server_socket, F_SETFL, flags | O_NONBLOCK);

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(PORT);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    int bind_val = bind(server_socket, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
    if (bind_val == -1)
    {
        std::cerr << "[ERROR]: Error binding server_socket." << std::endl;
        terminate();
    }
    int listen_val = listen(server_socket, N_CLIENTS); // listening/holding 5 pending connections.
    if (listen_val == -1)
    {
        std::cerr << "[ERROR]: Error listening on server_socket." << std::endl;
        terminate();
    }

    notify_fd=eventfd(0,EFD_NONBLOCK);
    if (notify_fd == -1)
    {
        std::cerr << "[ERROR]: Error creating eventfd." << std::endl;
        terminate();
    }

    // creating epoll_fd that will handle all the events we get.
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        std::cerr << "[ERROR]: Error creating epoll_fd.\nTerminating program" << std::endl;
        terminate();
    }

    // epoll event starts here.
    event.events = EPOLLIN;
    event.data.fd = server_socket;
    retcode = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &event);
    if (retcode == -1)
    {
        std::cerr << "[ERROR]: Error adding socket_fd in epoll_ctl." << std::endl;
        terminate();
    }

    notify_event.events=EPOLLIN;
    notify_event.data.fd=notify_fd;
    retcode = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, notify_fd, &notify_event);
    if (retcode == -1)
    {
        std::cerr << "[ERROR]: Error adding event_fd in epoll_ctl." << std::endl;
        terminate();
    }

    int client_fd;

    while (true)
    {
        std::cout << "In epoll wait" << std::endl;
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        for (int i = 0; i < n; i++)
        {
            if (events[i].data.fd == server_socket)
            {
                // accept all-- server event.
                accept_all(events[i], Live_connections);
            }
            else if(events[i].data.fd==notify_fd){
                handle_notify_fd(notify_fd,Live_connections);
            }
            else
            {
                handle_client_fd(events[i].data.fd,Live_connections);
            }
        }
        sleep(1);
    }

    // close(server_socket);
    terminate();
    return 0;
}

// issues faced

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
    **Note: here Connection_obj is a shared_ptr, and below is only valid for pointers of obj..
    --leveraged how un_maps are built.
    --each pair in un_map has {key,value},
    --now rehash only modifies the ref of this whole KV obj, the internal VALUE obj's ref remains same
        till it is alive.
    --Giving us leverage over ref for VALUE(rehash safe ref), can work independently on these refs.
--Notifying the epoll about write_fds that are ready,without giving the control to worker threads.
    --used eventfd along with Blocking_queue to do the work.
*/