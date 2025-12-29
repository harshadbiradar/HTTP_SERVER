#include"../include/Server.h"

int main(int argc,char* argv[]){
    std::cout<<"The main program started"<<std::endl;
    Config config;
    int port=8080;
    int n_clients=65000;
    int max_events=8192;
    int n_workers=11;
    int queue_size=16384;
    int buff_len=8192;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--threads") n_workers = std::stoi(argv[++i]);
        if (arg == "--events")  max_events = std::stoi(argv[++i]);
        if (arg == "--queue")   queue_size = std::stoi(argv[++i]);
        if (arg == "--bufflen")   buff_len = std::stoi(argv[++i]);
    }
    config.set_config(port,n_clients,max_events,n_workers,queue_size,buff_len);
    Server server(config);
    server.start();
}