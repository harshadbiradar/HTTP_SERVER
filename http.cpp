#include<iostream>
#include<cstring>
#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>

int main(){

    int clientsocket=socket(AF_INET,SOCK_STREAM,0);

    sockaddr_in clientAddress;
    clientAddress.sin_family=AF_INET;
    clientAddress.sin_port=htons(8080);
    clientAddress.sin_addr.s_addr=INADDR_ANY;

    connect(clientsocket,(struct sockaddr*)&clientAddress,sizeof(clientAddress));

    const char* msg="Hello\0";
    // while(true){
    //     sleep(2);
    //     // send(clientsocket,msg,strlen(msg),0);
    //     sleep(10);
    // }
    send(clientsocket,msg,strlen(msg),0);
    sleep(50);

    close(clientsocket);
    return 0;
}