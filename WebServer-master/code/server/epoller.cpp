//epoller 用于管理epoll的相关操作,epoll指的是Linux内核中的一种IO多路复用机制，epoll可以同时监听多个文件描述符上的事件，
#include "epoller.h"

Epoller::Epoller(int maxEvent):epollFd_(epoll_create(512)), events_(maxEvent){   //：构造函数初始化参数列表，初始化文件描述符、事件数组
    assert(epollFd_ >= 0 && events_.size() > 0); //断言，epollFd_大于等于0，events_数组大小大于0
}

Epoller::~Epoller() {
    close(epollFd_);
}

bool Epoller::AddFd(int fd, uint32_t events) {
    if(fd < 0) return false;
    epoll_event ev = {0}; //epoll_event结构体，用于注册事件
    ev.data.fd = fd; //文件描述符,用于标识事件来源,可以是socket文件描述符,也可以是其他文件描述符,如标准输入,标准输出等
    ev.events = events; //事件
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev); //epoll_ctl函数，用于控制epoll事件，添加事件,成功返回0,失败返回-1,第一个参数是epoll_create返回的文件描述符，
                                                //第二个参数是操作类型，第三个参数是要监听的文件描述符，第四个参数是要监听的事件,EPOLL_CTL_ADD表示注册新的fd到epfd中
}

bool Epoller::ModFd(int fd, uint32_t events) {
    if(fd < 0) return false;
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev);
}

bool Epoller::DelFd(int fd) {
    if(fd < 0) return false;
    epoll_event ev = {0}; 
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, &ev);//EPOLL_CTL_DEL表示从epfd中删除一个fd,成功返回0,失败返回-1
}

int Epoller::Wait(int timeoutMs) {
    return epoll_wait(epollFd_, &events_[0], static_cast<int>(events_.size()), timeoutMs); //epoll_wait函数，等待事件的发生，
    //成功返回就绪的文件描述符的个数，失败返回-1,第一个参数是epoll_create返回的文件描述符，第二个参数是epoll_event结构体数组，第三个参数是数组大小，第四个参数是超时时间
    //static_cast<int>(events_.size())表示将events_.size()转换为int类型
}

int Epoller::GetEventFd(size_t i) const { 
    assert(i < events_.size() && i >= 0);
    return events_[i].data.fd;
}

uint32_t Epoller::GetEvents(size_t i) const {
    assert(i < events_.size() && i >= 0);
    return events_[i].events;
}
