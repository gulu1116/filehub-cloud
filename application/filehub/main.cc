#include <iostream>
#include <signal.h>


#include "muduo/net/TcpServer.h"
#include  "muduo/net/TcpConnection.h"
#include "muduo/base/ThreadPool.h"

#include "muduo/net/EventLoop.h"  //EventLoop
#include "muduo/base/Logging.h" // Logger日志头文件
#include "http_parser_wrapper.h"
#include "http_conn.h"
#include "config_file_reader.h"
#include "db_pool.h"
#include "cache_pool.h"
#include "api_upload.h"
using namespace muduo;
using namespace muduo::net;
using namespace std;

std::map<uint32_t, CHttpConnPtr> s_http_map;

class HttpServer
{
public:
    //构造函数 loop主线程的EventLoop， addr封装ip，port, name服务名字，num_event_loops多少个subReactor
    HttpServer(EventLoop *loop, const InetAddress &addr, const std::string &name,  int num_event_loops
                ,int num_threads)
    :loop_(loop)
    , server_(loop, addr,name)
    , num_threads_(num_threads)
    {
        server_.setConnectionCallback(std::bind(&HttpServer::onConnection, this, std::placeholders::_1));
        server_.setMessageCallback(
            std::bind(&HttpServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        server_.setWriteCompleteCallback(std::bind(&HttpServer::onWriteComplete, this, std::placeholders::_1));
   
        server_.setThreadNum(num_event_loops);
    }
    void start() {
        if(num_threads_ != 0)
            thread_pool_.start(num_threads_);
        server_.start();
    }
private:
    void onConnection(const TcpConnectionPtr &conn)  {
        if (conn->connected())
        {
            LOG_INFO <<  "onConnection  new conn" << conn.get();
            uint32_t uuid = conn_uuid_generator_++;
            conn->setContext(uuid);
            CHttpConnPtr http_conn = std::make_shared<CHttpConn>(conn);
         
            std::lock_guard<std::mutex> ulock(mtx_); //自动释放
            s_http_map.insert({ uuid, http_conn});
         
        } else {
            LOG_INFO <<  "onConnection  dis conn" << conn.get();
            uint32_t uuid = std::any_cast<uint32_t>(conn->getContext());
            std::lock_guard<std::mutex> ulock(mtx_); //自动释放
            s_http_map.erase(uuid);
        }
    }

    // void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time) {
    //     LOG_INFO <<  "onMessage " << conn.get();
    //     uint32_t uuid = std::any_cast<uint32_t>(conn->getContext());
    //     mtx_.lock();  
    //     CHttpConnPtr &http_conn = s_http_map[uuid];
    //     mtx_.unlock();
    //      //处理 相关业务
    //     if(num_threads_ != 0)  //开启了线程池
    //         thread_pool_.run(std::bind(&CHttpConn::OnRead, http_conn, buf)); //给到业务线程处理
    //     else {  //没有开启线程池
    //         http_conn->OnRead(buf);  // 直接在io线程处理
    //     }   
       
    // }
    void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time) {
        LOG_INFO <<  "onMessage " << conn.get();
        uint32_t uuid = std::any_cast<uint32_t>(conn->getContext());
        
        CHttpConnPtr http_conn;  // ✅ 使用值而不是引用
        {
            std::lock_guard<std::mutex> ulock(mtx_);
            auto it = s_http_map.find(uuid);
            if (it == s_http_map.end()) {
                LOG_WARN << "connection not found in map, uuid: " << uuid;
                return;  // ✅ 连接已断开，直接返回
            }
            http_conn = it->second;  // ✅ 复制 shared_ptr，增加引用计数
        }  // ✅ 锁在这里自动释放
        
        // 现在 http_conn 是安全的，即使 map 中删除也不会被销毁
        if(num_threads_ != 0) {
            thread_pool_.run(std::bind(&CHttpConn::OnRead, http_conn, buf));
        } else {
            http_conn->OnRead(buf);
        }   
    }

    void onWriteComplete(const TcpConnectionPtr& conn) {
        LOG_INFO <<  "onWriteComplete " << conn.get();
    }


    TcpServer server_;    // 每个连接的回调数据 新的连接/断开连接  收到数据  发送数据完成   
    EventLoop *loop_ = nullptr; //这个是主线程的EventLoop
    std::atomic<uint32_t> conn_uuid_generator_ = 0;  //这里是用于http请求，不会一直保持链接
    std::mutex mtx_;

    //线程池
    ThreadPool thread_pool_;
    const int num_threads_ = 0;
};


int main(int argc, char *argv[])
{

    std::cout  << argv[0] << "[conf ] "<< std::endl;
     

     // 默认情况下，往一个读端关闭的管道或socket连接中写数据将引发SIGPIPE信号。我们需要在代码中捕获并处理该信号，
    // 或者至少忽略它，因为程序接收到SIGPIPE信号的默认行为是结束进程，而我们绝对不希望因为错误的写操作而导致程序退出。
    // SIG_IGN 忽略信号的处理程序
    signal(SIGPIPE, SIG_IGN); //忽略SIGPIPE信号
    int ret = 0;
    char *str_tc_http_server_conf = NULL;
    if(argc > 1) {
        str_tc_http_server_conf = argv[1];  // 指向配置文件路径
    } else {
        str_tc_http_server_conf = (char *)"tc_http_server.conf";
    }
     std::cout << "conf file path: " <<  str_tc_http_server_conf << std::endl;
     // 读取配置文件
    CConfigFileReader config_file(str_tc_http_server_conf);     //读取配置文件

    // Logger::setLogLevel(Logger::ERROR);     //性能测试时减少打印
    //日志设置级别
    char *str_log_level =  config_file.GetConfigName("log_level");  
    Logger::LogLevel log_level = static_cast<Logger::LogLevel>(atoi(str_log_level));
    Logger::setLogLevel(log_level);


    char *dfs_path_client = config_file.GetConfigName("dfs_path_client"); // /etc/fdfs/client.conf
    char *storage_web_server_ip = config_file.GetConfigName("storage_web_server_ip"); //后续可以配置域名
    char *storage_web_server_port = config_file.GetConfigName("storage_web_server_port");

    
     // 初始化mysql、redis连接池，内部也会读取读取配置文件tc_http_server.conf
    CacheManager::SetConfPath(str_tc_http_server_conf); //设置配置文件路径
    CacheManager *cache_manager = CacheManager::getInstance();
    if (!cache_manager) {
        LOG_ERROR <<"CacheManager init failed";
        return -1;
    }

    // 将配置文件的参数传递给对应模块
     ApiUploadInit(dfs_path_client, storage_web_server_ip, storage_web_server_port, "", "");

    CDBManager::SetConfPath(str_tc_http_server_conf);   //设置配置文件路径
    CDBManager *db_manager = CDBManager::getInstance();
    if (!db_manager) {
        LOG_ERROR <<"DBManager init failed";
        return -1;
    }

    std::cout << "hello GuLu ../../bin/filehub\n";
    uint16_t http_bind_port = 8081;
    const char *http_bind_ip = "0.0.0.0";
    char *str_num_event_loops = config_file.GetConfigName("num_event_loops");  
    int num_event_loops = atoi(str_num_event_loops);
    char *str_num_threads = config_file.GetConfigName("num_threads");  
    int num_threads = atoi(str_num_threads);

    char *str_timeout_ms = config_file.GetConfigName("timeout_ms");  
    int timeout_ms = atoi(str_timeout_ms);
    std::cout << "timeout_ms: " << timeout_ms << std::endl;

    EventLoop loop;     //主循环
    InetAddress addr(http_bind_ip, http_bind_port);     // 注意别搞错位置了
    LOG_INFO << "port: " << http_bind_port;
    HttpServer server(&loop, addr, "HttpServer", num_event_loops, num_threads);
    server.start();
    loop.loop(timeout_ms); //1000ms
    return 0;
}