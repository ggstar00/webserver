#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <unordered_map>
#include <fcntl.h>       // fcntl()
#include <unistd.h>      // close()
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "epoller.h"
#include "../log/log.h"
#include "../timer/heaptimer.h"
#include "../pool/sqlconnpool.h"
#include "../pool/threadpool.h"
#include "../pool/sqlconnRAII.h"
#include "../http/httpconn.h"

class WebServer {
public:
    WebServer(
        int port, int trigMode, int timeoutMS, bool OptLinger, 
        int sqlPort, const char* sqlUser, const  char* sqlPwd, 
        const char* dbName, int connPoolNum, int threadNum,
        bool openLog, int logLevel, int logQueSize);

    ~WebServer();
    void Start();

private:
    bool InitSocket_(); 
    void InitEventMode_(int trigMode);
    void AddClient_(int fd, sockaddr_in addr);
  
    void DealListen_();
    void DealWrite_(HttpConn* client);
    void DealRead_(HttpConn* client);

    void SendError_(int fd, const char*info);
    void ExtentTime_(HttpConn* client);
    void CloseConn_(HttpConn* client);

    void OnRead_(HttpConn* client);
    void OnWrite_(HttpConn* client);
    void OnProcess(HttpConn* client);

    static const int MAX_FD = 65536;

    static int SetFdNonblock(int fd);

    int port_;
    bool openLinger_;  //是否保持连接，即在客户端断开连接后是否继续等待客户端重新连接
    int timeoutMS_;  /* 毫秒MS */
    bool isClose_;
    int listenFd_; //监听套接字的文件描述符
    char* srcDir_; //源目录路径
    
    uint32_t listenEvent_; //监听事件类型，用于通知线程池处理监听事件   uint32_t是32位无符号整数
    uint32_t connEvent_; //连接事件类型，用于通知线程池处理连接事件 
   
    std::unique_ptr<HeapTimer> timer_;   //unique_ptr  c++11 智能指针类型 定时器对象，用于定时执行一些任务
    std::unique_ptr<ThreadPool> threadpool_; //线程池对象，用于吃了多个客户端连接的请求
    std::unique_ptr<Epoller> epoller_; //epoll对象，用于监控文件描述符的变化情况，以便及时处理新的连接和数据传输
    std::unordered_map<int, HttpConn> users_; //存储所有已建立连接的客户端对象，键为客户端的id，值为HTTPCONN对象 unordered_map 关联容器，存储键值对
};


#endif //WEBSERVER_H