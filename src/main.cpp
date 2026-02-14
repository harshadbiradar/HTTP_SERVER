// #include"../include/Server.h"
#include "../include/common_header.h"
#include"../include/config.h"
#include"../include/Thread_pool.h"

int main(int argc,char* argv[]){
    signal(SIGPIPE, SIG_IGN);
    std::cout<<"The main program started"<<std::endl;
    Config config;
    int port=8080;
    int n_clients=15000;
    int max_events=8192;
    int n_workers=11;
    int buff_len=8192;
    int Max_evnt_p_thd=8192;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--threads") n_workers = std::stoi(argv[++i]);
        if (arg == "--events")  max_events = std::stoi(argv[++i]);
        if (arg == "--bufflen")   buff_len = std::stoi(argv[++i]);
        if (arg == "--maxperthrd") Max_evnt_p_thd = std::stoi(argv[++i]);
    }
    config.set_config(port,n_clients,max_events,n_workers,buff_len,Max_evnt_p_thd);
    // Thread_Pool Worker(Max_evnt_p_thd,n_clients);
    Thread_Pool Worker(config);
    Worker.create_pool(n_workers);
    // Server server(config,Worker);
    // server.start();
}