#ifndef COMMON_HEADER
#define COMMON_HEADER

#include<sys/socket.h>
#include<netinet/in.h>
#include<iostream>
#include<sys/epoll.h>
#include<string>
#include<unistd.h>
#include<utility>
#include<fcntl.h>
#include<sys/eventfd.h>
#include<cstring>
#include<unordered_map>
#include<memory>
#include<atomic>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <netinet/tcp.h>
#include<csignal>
inline std::string get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::stringstream ss;
    ss << std::put_time(std::localtime(&tt), "%Y-%m-%d %H:%M:%S")
       << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

#define LOG_INFO(msg)    std::cout << "[" << get_timestamp() << "] [INFO]  " << msg << std::endl
#define LOG_WARN(msg)    std::cerr << "[" << get_timestamp() << "] [WARN]  " << msg << std::endl
#define LOG_ERROR(msg)   std::cerr << "[" << get_timestamp() << "] [ERROR] " << msg << std::endl
#define LOG_DEBUG(msg)   std::cout << "[" << get_timestamp() << "] [DEBUG] " << msg << std::endl
#endif