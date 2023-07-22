#include "log.h"

using namespace std;

Log::Log() {
    lineCount_ = 0;
    isAsync_ = false;
    writeThread_ = nullptr;
    deque_ = nullptr;
    toDay_ = 0;
    fp_ = nullptr;
}

//确保在销毁 Log 类对象时，将日志缓冲区中的所有日志写入文件并关闭文件，同时确保写日志线程已经完成所有工作并退出。
Log::~Log() {
    if(writeThread_ && writeThread_->joinable()) {
        while(!deque_->empty()) {
            deque_->flush();         
        };
        deque_->Close();
        writeThread_->join();
    }
    if(fp_) {
        lock_guard<mutex> locker(mtx_);
        flush();
        fclose(fp_);
    }
}

int Log::GetLevel() {
    lock_guard<mutex> locker(mtx_); //std::lock_guard是一个RAII(Resource Acquisition Is Initialization)类，它可以自动管理一个互斥锁的加锁和解锁操作。
                                    //将mtx_这个互斥锁封装成一个lock_guard对象
    return level_;
}

void Log::SetLevel(int level) {
    lock_guard<mutex> locker(mtx_);
    level_ = level;
}

void Log::init(int level = 1, const char* path, const char* suffix,
    int maxQueueSize) {
    isOpen_ = true;
    level_ = level;
    if(maxQueueSize > 0) {
        isAsync_ = true;
        if(!deque_) {
            unique_ptr<BlockDeque<std::string>> newDeque(new BlockDeque<std::string>);
            deque_ = move(newDeque); //c++11引入的新关键字，用于实现资源的转移，将newDeque资源转移到deque_中

            std::unique_ptr<std::thread> NewThread(new thread(FlushLogThread)); //创建一个新的线程，并将FlushLogThread函数作为线程的入口点。
            writeThread_ = move(NewThread); 
        }
    } else {
        isAsync_ = false;
    }

    lineCount_ = 0;

    time_t timer = time(nullptr); //获取当前时间时间戳，nullptr可去掉，c++11中time（）函数的参数已经默认为nullptr
    struct tm *sysTime = localtime(&timer); //将时间戳转换为本地时间
    struct tm t = *sysTime;//将sysTime指向的结构体内容拷贝到t指向的结构体中，方便操作，t.tm_year
    path_ = path;
    suffix_ = suffix;
    char fileName[LOG_NAME_LEN] = {0};
    snprintf(fileName, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s", 
            path_, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, suffix_); //将日志文件名格式化为一个字符串，并将其存储在fileName数组中。
                                                                        //snprintf()函数是一个标准库函数，用于将格式化的数据写入到一个字符数组中
                                                                        //suffix_文件后缀名
    toDay_ = t.tm_mday;

    {
        lock_guard<mutex> locker(mtx_);
        buff_.RetrieveAll(); //清空缓冲区，将缓冲区中的所有数据取出并丢弃
        if(fp_) { 
            flush();
            fclose(fp_); 
        }

        fp_ = fopen(fileName, "a"); //“a”：以追加模式打开文件，如果文件不存在，则会创建一个新文件，如果文件存在，则会在文件末尾追加内容。
        if(fp_ == nullptr) {
            mkdir(path_, 0777); //创建目录，0777表示权限，即所有用户都有权限进行读、写、执行操作
            fp_ = fopen(fileName, "a"); //再次尝试打开文件
        } 
        assert(fp_ != nullptr);
    }
}

void Log::write(int level, const char *format, ...) {
    struct timeval now = {0, 0}; //timeval结构体，用于存储时间，其中tv_sec表示秒，tv_usec表示微秒 <sys/time.h>头文件包含时间结构体定义
    gettimeofday(&now, nullptr); //获取当前时间
    time_t tSec = now.tv_sec; //tv_sec表示秒
    struct tm *sysTime = localtime(&tSec);
    struct tm t = *sysTime;
    va_list vaList;

    /* 日志日期 日志行数 */
    if (toDay_ != t.tm_mday || (lineCount_ && (lineCount_  %  MAX_LINES == 0)))
    {
        unique_lock<mutex> locker(mtx_);
        locker.unlock();
        
        char newFile[LOG_NAME_LEN];
        char tail[36] = {0}; //tail表示日志文件名中的日期部分
        snprintf(tail, 36, "%04d_%02d_%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday); 

        if (toDay_ != t.tm_mday) //如果是新的一天，则创建新的日志文件
        {
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s%s", path_, tail, suffix_);
            toDay_ = t.tm_mday;
            lineCount_ = 0;
        }
        else {
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s-%d%s", path_, tail, (lineCount_  / MAX_LINES), suffix_);
        }
        
        locker.lock();
        flush();
        fclose(fp_);
        fp_ = fopen(newFile, "a");
        assert(fp_ != nullptr);
    }

    {
        unique_lock<mutex> locker(mtx_);
        lineCount_++;
        int n = snprintf(buff_.BeginWrite(), 128, "%d-%02d-%02d %02d:%02d:%02d.%06ld ",
                    t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                    t.tm_hour, t.tm_min, t.tm_sec, now.tv_usec);
                    
        buff_.HasWritten(n);
        AppendLogLevelTitle_(level);

        va_start(vaList, format);
        int m = vsnprintf(buff_.BeginWrite(), buff_.WritableBytes(), format, vaList);
        va_end(vaList);

        buff_.HasWritten(m);
        buff_.Append("\n\0", 2);

        if(isAsync_ && deque_ && !deque_->full()) {
            deque_->push_back(buff_.RetrieveAllToStr());
        } else {
            fputs(buff_.Peek(), fp_);
        }
        buff_.RetrieveAll();
    }
}

void Log::AppendLogLevelTitle_(int level) {
    switch(level) {
    case 0:
        buff_.Append("[debug]: ", 9);
        break;
    case 1:
        buff_.Append("[info] : ", 9);
        break;
    case 2:
        buff_.Append("[warn] : ", 9);
        break;
    case 3:
        buff_.Append("[error]: ", 9);
        break;
    default:
        buff_.Append("[info] : ", 9);
        break;
    }
}

/* 检查是否开启了异步模式，如果开启了异步模式，就调用阻塞队列对象的flush()函数来将所有的日志信息刷新到磁盘中。
如果没有开启异步模式，则直接调用标准库函数fflush()来将缓冲区中的数据刷新到磁盘中。
fflush并没有关闭文件*/
void Log::flush() {
    if(isAsync_) { 
        deque_->flush(); 
    }
    fflush(fp_);
}

void Log::AsyncWrite_() {
    string str = "";
    while(deque_->pop(str)) {
        lock_guard<mutex> locker(mtx_);
        fputs(str.c_str(), fp_);
    }
}

Log* Log::Instance() {
    static Log inst;
    return &inst;
}

void Log::FlushLogThread() {
    Log::Instance()->AsyncWrite_();
}


