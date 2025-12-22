#ifndef HTTP_PSR
#define HTTP_PSR

#define NO_FLDS 4

#include<string>
#include<iostream>
#include<unistd.h>
#include<utility>
#include<unordered_map>
#include<vector>
// #include<sstream>

// constexpr int NO_FLDS=4;
class HttpRequest{
    private:
        std::vector<std::string*> Fields;
        std::string method;
        std::string path;
        std::unordered_map<std::string,std::string>headers;
        std::string header_str;
        std::string body;
        std::string delimiter;
        void parse_req(const std::string &req_str);
        void parse_header(std::string &header_str);
    public:
        HttpRequest(const std::string &req_str){
            Fields.reserve(NO_FLDS);
            Fields.emplace_back(&method);
            Fields.emplace_back(&path);
            Fields.emplace_back(&header_str);
            Fields.emplace_back(&body);
            parse_req(req_str);
        }

        HttpRequest(const HttpRequest &other):method(other.method),
        path(other.path),header_str(other.header_str),body(other.body),
        headers(other.headers),delimiter(other.delimiter){
            //copy ctor
            Fields.reserve(NO_FLDS);
            Fields.emplace_back(&method);
            Fields.emplace_back(&path);
            Fields.emplace_back(&header_str);
            Fields.emplace_back(&body);

        }

        HttpRequest& operator=(const HttpRequest &other){
            if(this!=&other){
                // this->clear();
                method=other.method;
                path=other.path;
                header_str=other.header_str;
                body=other.body;
                headers=other.headers;
                delimiter=other.delimiter;
            }
            return *this;
        }

        HttpRequest(HttpRequest &&other)noexcept:method(std::move(other.method)),
        path(std::move(other.path)),header_str(std::move(other.header_str)),body(std::move(other.body))
        ,headers(std::move(other.headers)),delimiter(std::move(other.delimiter)){
            //move ctor
            Fields.reserve(NO_FLDS);
            Fields.emplace_back(&method);
            Fields.emplace_back(&path);
            Fields.emplace_back(&header_str);
            Fields.emplace_back(&body);
        }

        HttpRequest& operator=(HttpRequest &&other)noexcept{
            if(this!=&other){
                this->clear();
                method=std::move(other.method);
                path=std::move(other.path);
                header_str=std::move(other.header_str);
                body=std::move(other.body);
                delimiter=(std::move(other.delimiter));
                headers=std::move(other.headers);
                other.clear();
            }
            return *this;
        }

        void clear(){
            method.clear();
            path.clear();
            header_str.clear();
            body.clear();
            headers.clear();
            delimiter.clear();
        }

        void print_req(){
            for(auto fld:Fields){
                std::cout<<(*fld)<<std::endl;
            }
        }

};

void HttpRequest::parse_req(const std::string &req_str){
    size_t start=0;
    size_t pos=0;
    for(auto fld:Fields){
        pos=req_str.find(delimiter,start);
        if(pos==std::string::npos){
            std::cout<<"[ERROR]: in finding substr"<<std::endl;
            break;
        }
        fld->append(req_str.substr(start,pos-start));
        start=pos+delimiter.length();
    }
}


void HttpRequest::parse_header(std::string &header_str){
    //will update some time.
}


struct ResStruct{
    std::string version;
    int status_code;
    std::string status_msg;
    std::string header_str;
    std::string body;
};

class HttpResponse{
    private:
        struct ResStruct Res_body;
        std::unordered_map<std::string,std::string>headers;
    public:
        HttpResponse(struct ResStruct out_body):Res_body(out_body){
            parse_header();
        }

        HttpResponse(const HttpResponse &other):Res_body(other.Res_body){
            //copy ctor
        }
        HttpResponse& operator=(const HttpResponse &other){
            if(this!=&other)
                Res_body=other.Res_body;
            return *this;
        }
        HttpResponse(HttpResponse &&other):Res_body(std::move(other.Res_body)){
            //move ctor
            other.clear();

        }

        HttpResponse& operator=(HttpResponse &&other){
            if(this!=&other){
                Res_body=std::move(other.Res_body);
                other.clear();
            }
            return *this;
        }




        void clear(){
            Res_body.body.clear();
            Res_body.header_str.clear();
            Res_body.status_msg.clear();
        }
        void parse_header();
};

#endif