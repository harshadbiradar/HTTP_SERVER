#include"../include/Server.h"

int main(){
    std::cout<<"The main program started"<<std::endl;
    Config config;
    int port=8080;
    int n_clients=10;
    int max_events=2048;
    int n_workers=4;
    int queue_size=5000;
    int buff_len=4096;
    config.set_config(port,n_clients,max_events,n_workers,queue_size,buff_len);
    Server server(config);
    server.start();
}