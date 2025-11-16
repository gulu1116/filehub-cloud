#include <iostream>
// #include "muduo/net"

#include "muduo/net/TcpServer.h"
#include "muduo/net/TcpConnection.h"
#include "muduo/net/EventLoop.h"  //EventLoop
#include "muduo/base/Logging.h" // Logger日志头文件
#include "http_parser_wrapper.h"
#include "http_conn.h"
using namespace muduo;
using namespace muduo::net;
using namespace std;

std::map<uint32_t, CHttpConnPtr> s_http_map;

class HttpServer
{
public:
    //构造函数 loop主线程的EventLoop， addr封装ip，port, name服务名字，num_event_loops多少个subReactor
    HttpServer(EventLoop *loop, const InetAddress &addr, const std::string &name,  int num_event_loops)
    :loop_(loop), server_(loop, addr,name)
    {
        server_.setConnectionCallback(std::bind(&HttpServer::onConnection, this, std::placeholders::_1));
        server_.setMessageCallback(
            std::bind(&HttpServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        server_.setWriteCompleteCallback(std::bind(&HttpServer::onWriteComplete, this, std::placeholders::_1));
   
        server_.setThreadNum(num_event_loops);
    }
    void start() {
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
            s_http_map.insert({uuid, http_conn});
        } else {
            LOG_INFO <<  "onConnection  dis conn" << conn.get();
            uint32_t uuid = std::any_cast<uint32_t>(conn->getContext());
            s_http_map.erase(uuid);
        }
    }

    void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time) {
        LOG_INFO <<  "onMessage " << conn.get();
        uint32_t uuid = std::any_cast<uint32_t>(conn->getContext());
        CHttpConnPtr &http_conn = s_http_map[uuid];
         //处理 相关业务
        http_conn->OnRead(buf);  // 直接在io线程处理
        // LOG_INFO << "get msg: " << in_buf;
          // conn->send(in_buf, buf->readableBytes());
       
    }

    void onWriteComplete(const TcpConnectionPtr& conn) {
        LOG_INFO <<  "onWriteComplete " << conn.get();
    }


    TcpServer server_;    // 每个连接的回调数据 新的连接/断开连接  收到数据  发送数据完成   
    EventLoop *loop_ = nullptr; //这个是主线程的EventLoop
    std::atomic<uint32_t> conn_uuid_generator_ = 0;  //这里是用于http请求，不会一直保持链接
};


int main()
{
    std::cout << "hello GuLu ../../bin/filehub\n";
    uint16_t http_bind_port = 8081;
    const char *http_bind_ip = "0.0.0.0";
    int32_t num_event_loops = 4;

    EventLoop loop;     //主循环
    InetAddress addr(http_bind_ip, http_bind_port);     // 注意别搞错位置了
    LOG_INFO << "port: " << http_bind_port;
    HttpServer server(&loop, addr, "HttpServer", num_event_loops);
    server.start();
    loop.loop();
    return 0;
}