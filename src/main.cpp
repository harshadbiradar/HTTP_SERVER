#include"../include/Server.h"

int main(){
    std::cout<<"The main program started"<<std::endl;
    Config config;
    config.set_config(8080,10,10,4,1000,1024);
    Server server(config);
    server.start();
}