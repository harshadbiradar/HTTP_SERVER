#ifndef CONFIG_H
#define CONFIG_H
class Config{
    private:
        int port;
        int n_clients;
        int max_events;
        int n_workers;
        int queue_size;
        int buff_len;
    public:
        void get_config(int &port,int &n_clients,int &max_events,int &n_workers,int &queue_size,int &buff_len){
            port=this->port;
            n_clients=this->n_clients;
            max_events=this->max_events;
            n_workers=this->n_workers;
            queue_size=this->queue_size;
            buff_len=this->buff_len;
        }
        void set_config(int port,int n_clients,int max_events,int n_workers,int queue_size,int buff_len){
            this->port=port;
            this->n_clients=n_clients;
            this->max_events=max_events;
            this->n_workers=n_workers;
            this->queue_size=queue_size;
            this->buff_len=buff_len;
        }
};

#endif