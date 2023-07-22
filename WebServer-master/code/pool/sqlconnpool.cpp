#include "sqlconnpool.h"
using namespace std;

SqlConnPool::SqlConnPool() {
    useCount_ = 0;
    freeCount_ = 0;
}

SqlConnPool* SqlConnPool::Instance() {
    static SqlConnPool connPool;
    return &connPool;
}

void SqlConnPool::Init(const char* host, int port,
            const char* user,const char* pwd, const char* dbName,
            int connSize = 10) {
    assert(connSize > 0);
    for (int i = 0; i < connSize; i++) {
        MYSQL *sql = nullptr;
        sql = mysql_init(sql);
        if (!sql) {
            LOG_ERROR("MySql init error!");
            assert(sql);
        }
        sql = mysql_real_connect(sql, host,
                                 user, pwd,
                                 dbName, port, nullptr, 0); /*第七个参数unix_socket，Unix套接字路径，如果未指定，
                                                            则使用TCP/IP协议连接到数据库服务器
                                                            第八个参数clientflag，标志位，指定客服端的一些选项，
                                                            如CLIENT_SSL，启动ssl加密连接
                                                            CLIENT_PLUGIN_AUTH，启动插件认证
                                                            CLIENT_CONNECT_ATTRS，启动客户端属性支持
                                                            CLIENT_MULTI_STATEMENTS，启动多语句支持*/
        if (!sql) {
            LOG_ERROR("MySql Connect error!");
        }
        connQue_.push(sql);
    }
    MAX_CONN_ = connSize;
    sem_init(&semId_, 0, MAX_CONN_); //初始化一个信号量，用于控制并发数据库连接池里连接数量，semid信号量结构体，0表示独占信号量，1表示可以被多个进程共享，MAX_CONN_初始化信号量
}

MYSQL* SqlConnPool::GetConn() {
    MYSQL *sql = nullptr;
    if(connQue_.empty()){
        LOG_WARN("SqlConnPool busy!");
        return nullptr;
    } 
    sem_wait(&semId_);   //信号量-1
    {
        lock_guard<mutex> locker(mtx_);   //lock_guard智能指针，管理互斥锁的生命周期，mutex互斥量，用于同步多线程访问共享资源的数据结构
        sql = connQue_.front();
        connQue_.pop();
    }
    return sql;
}

void SqlConnPool::FreeConn(MYSQL* sql) {
    assert(sql);
    lock_guard<mutex> locker(mtx_);
    connQue_.push(sql);
    sem_post(&semId_);  //信号量加1
}

void SqlConnPool::ClosePool() {
    lock_guard<mutex> locker(mtx_);
    while(!connQue_.empty()) {
        auto item = connQue_.front();
        connQue_.pop();
        mysql_close(item);
    }
    mysql_library_end(); //终止使用mysql数据库，对于涉及客户端库的使用，提供改进的内存管理       
}

int SqlConnPool::GetFreeConnCount() {
    lock_guard<mutex> locker(mtx_);
    return connQue_.size();
}

SqlConnPool::~SqlConnPool() {
    ClosePool();
}

