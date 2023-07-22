/*
 * @Author       : mark
 * @Date         : 2020-06-15
 * @copyleft Apache 2.0
 */ 
#include "httpconn.h"
using namespace std;

const char* HttpConn::srcDir;
std::atomic<int> HttpConn::userCount;
bool HttpConn::isET;

HttpConn::HttpConn() { 
    fd_ = -1;
    addr_ = { 0 };
    isClose_ = true;
};

HttpConn::~HttpConn() { 
    Close(); 
};

void HttpConn::init(int fd, const sockaddr_in& addr) {
    assert(fd > 0);
    userCount++;
    addr_ = addr; //sockaddr_in是一个结构体，包含了ip地址和端口号
    fd_ = fd; 
    writeBuff_.RetrieveAll(); //清空写缓冲区
    readBuff_.RetrieveAll(); //清空读缓冲区
    isClose_ = false;
    LOG_INFO("Client[%d](%s:%d) in, userCount:%d", fd_, GetIP(), GetPort(), (int)userCount); //打印日志
}

void HttpConn::Close() {
    response_.UnmapFile();
    if(isClose_ == false){
        isClose_ = true; 
        userCount--;
        close(fd_);
        LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
    }
}

int HttpConn::GetFd() const {
    return fd_; 
};

struct sockaddr_in HttpConn::GetAddr() const { 
    return addr_;
}

const char* HttpConn::GetIP() const {  //返回值const char*，表示返回的是一个字符串，且该字符串不可修改，即只读
    return inet_ntoa(addr_.sin_addr); //inet_ntoa()函数将网络地址转换成“.”点隔的字符串格式。
}

int HttpConn::GetPort() const {
    return addr_.sin_port;
}

ssize_t HttpConn::read(int* saveErrno) {
    ssize_t len = -1; //ssize_t是有符号整型，用来表示可以读取或写入的字节数，如果出错则返回-1，如果读到文件末尾则返回0
    do {
        len = readBuff_.ReadFd(fd_, saveErrno);//saveErrno是一个指针，指向errno，errno是一个全局变量，用来表明发生了什么错误
        if (len <= 0) {
            break;
        }
    } while (isET); //isET是一个静态变量，表示是否采用ET模式,ET模式下，需要一次性将数据读完，所以需要循环读取
    return len;
}

ssize_t HttpConn::write(int* saveErrno) {
    ssize_t len = -1;
    do {
        len = writev(fd_, iov_, iovCnt_); //writev()函数用于在一次函数调用中写入多个非连续缓冲区，即分散写，返回值为写入的字节数，出错返回-1，
                                        //iov_是一个iovec结构体数组，iovec结构体包含了指向缓冲区的指针和缓冲区的大小，iovCnt_表示iovec结构体数组的大小
        if(len <= 0) {
            *saveErrno = errno;
            break;
        }
        if(iov_[0].iov_len + iov_[1].iov_len  == 0) { break; } /* 传输结束 */ //iov_[0]表示响应头，iov_[1]表示文件 
        else if(static_cast<size_t>(len) > iov_[0].iov_len) {  
            iov_[1].iov_base = (uint8_t*) iov_[1].iov_base + (len - iov_[0].iov_len); 
            iov_[1].iov_len -= (len - iov_[0].iov_len);
            if(iov_[0].iov_len) {
                writeBuff_.RetrieveAll();
                iov_[0].iov_len = 0;
            }
        }
        else {
            iov_[0].iov_base = (uint8_t*)iov_[0].iov_base + len; 
            iov_[0].iov_len -= len; 
            writeBuff_.Retrieve(len);
        }
    } while(isET || ToWriteBytes() > 10240);
    return len;
}

bool HttpConn::process() {  //处理请求
    request_.Init();
    if(readBuff_.ReadableBytes() <= 0) {
        return false;
    }
    else if(request_.parse(readBuff_)) {
        LOG_DEBUG("%s", request_.path().c_str());
        response_.Init(srcDir, request_.path(), request_.IsKeepAlive(), 200);
    } else {
        response_.Init(srcDir, request_.path(), false, 400);
    }
   
   
    response_.MakeResponse(writeBuff_);
    /* 响应头 */
    iov_[0].iov_base = const_cast<char*>(writeBuff_.Peek());
    iov_[0].iov_len = writeBuff_.ReadableBytes();
    iovCnt_ = 1;

    /* 文件 */
    if(response_.FileLen() > 0  && response_.File()) {
        iov_[1].iov_base = response_.File();
        iov_[1].iov_len = response_.FileLen();
        iovCnt_ = 2;
    }
    LOG_DEBUG("filesize:%d, %d  to %d", response_.FileLen() , iovCnt_, ToWriteBytes());
    return true;
}
