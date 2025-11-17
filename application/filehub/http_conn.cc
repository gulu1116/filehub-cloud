#include "http_conn.h"
#include "muduo/base/Logging.h" // Logger日志头文件
#include "api_register.h"
#include "api_login.h"
#include "api_md5.h"
#include "api_upload.h"
#include "api_myfiles.h"
#include "api_sharepicture.h"
#define HTTP_RESPONSE_JSON_MAX 4096
#define HTTP_RESPONSE_JSON                                                     \
    "HTTP/1.1 200 OK\r\n"                                                      \
    "Connection:close\r\n"                                                     \
    "Content-Length:%d\r\n"                                                    \
    "Content-Type:application/json;charset=utf-8\r\n\r\n%s"

#define HTTP_RESPONSE_HTML                                                    \
    "HTTP/1.1 200 OK\r\n"                                                      \
    "Connection:close\r\n"                                                     \
    "Content-Length:%d\r\n"                                                    \
    "Content-Type:text/html;charset=utf-8\r\n\r\n%s"


#define HTTP_RESPONSE_BAD_REQ                                                     \
    "HTTP/1.1 400 Bad\r\n"                                                      \
    "Connection:close\r\n"                                                     \
    "Content-Length:%d\r\n"                                                    \
    "Content-Type:application/json;charset=utf-8\r\n\r\n%s"


CHttpConn::CHttpConn(TcpConnectionPtr tcp_conn):
    tcp_conn_(tcp_conn)
{
    uuid_ = std::any_cast<uint32_t>(tcp_conn_->getContext());
    LOG_INFO << "构造CHttpConn uuid: "<< uuid_ ;
}

CHttpConn::~CHttpConn() {
    LOG_INFO << "析构CHttpConn uuid: "<< uuid_ ;
}

void CHttpConn::OnRead(Buffer *buf) // CHttpConn业务层面的OnRead
{
    const char *in_buf = buf->peek();
    int32_t len = buf->readableBytes();
    http_parser_.ParseHttpContent(in_buf, len);
    if(http_parser_.IsReadAll()) {
        string url = http_parser_.GetUrlString();
        string content = http_parser_.GetBodyContentString();
        LOG_INFO << "url: " << url << ", content: " << content;   

        if (strncmp(url.c_str(), "/api/reg", 8) == 0) { // 注册  url 路由。 根据根据url快速找到对应的处理函数， 能不能使用map，hash
            _HandleRegisterRequest(url, content);
        } else if (strncmp(url.c_str(), "/api/login", 10) == 0) { // 登录
            _HandleLoginRequest(url, content);
        }  else if (strncmp(url.c_str(), "/api/md5", 8) == 0) {       //
            _HandleMd5Request(url, content);                         // 处理
        } else if (strncmp(url.c_str(), "/api/upload", 11) == 0) {   // 上传
            _HandleUploadRequest(url, content);
        }  else if (strncmp(url.c_str(), "/api/myfiles", 12) == 0) {   // 我的文件列表相关的
            _HandleMyFilesRequest(url, content);
        }  else if (strncmp(url.c_str(), "/api/sharepic", 13) == 0) {   
            _HandleSharepictureRequest(url, content);
        }   else {
            char *resp_content = new char[256];
            string str_json = "{\"code\": 1}"; 
            uint32_t len_json = str_json.size();
            //暂时先放这里
            #define HTTP_RESPONSE_REQ                                                     \
                "HTTP/1.1 404 OK\r\n"                                                      \
                "Connection:close\r\n"                                                     \
                "Content-Length:%d\r\n"                                                    \
                "Content-Type:application/json;charset=utf-8\r\n\r\n%s"
            snprintf(resp_content, 256, HTTP_RESPONSE_REQ, len_json, str_json.c_str()); 	
            tcp_conn_->send(resp_content);
        }
    }
}


// 账号注册处理
int CHttpConn::_HandleRegisterRequest(string &url, string &post_data) {
    
    string resp_json;
	int ret = ApiRegisterUser(post_data, resp_json);
	char *http_body = new char[HTTP_RESPONSE_JSON_MAX];
	uint32_t ulen = resp_json.length();
	snprintf(http_body, HTTP_RESPONSE_JSON_MAX, HTTP_RESPONSE_JSON, ulen,
        resp_json.c_str()); 	
    tcp_conn_->send(http_body);
    delete[] http_body;
    LOG_INFO << "    uuid: "<< uuid_;
    return 0;
}
 
int CHttpConn::_HandleLoginRequest(string &url, string &post_data)
{
	string str_json;
	int ret = ApiUserLogin(post_data, str_json);
	char *szContent = new char[HTTP_RESPONSE_JSON_MAX];
	uint32_t ulen = str_json.length();
	snprintf(szContent, HTTP_RESPONSE_JSON_MAX, HTTP_RESPONSE_JSON, ulen, str_json.c_str()); 	
    tcp_conn_->send(szContent);
    delete [] szContent;
	LOG_INFO << "    uuid: "<< uuid_; 
	return 0;
}
int CHttpConn::_HandleMd5Request(string &url, string &post_data)
{
	string str_json;
	int ret = ApiMd5(post_data, str_json);
	char *szContent = new char[HTTP_RESPONSE_JSON_MAX];
	uint32_t ulen = str_json.length();
	snprintf(szContent, HTTP_RESPONSE_JSON_MAX, HTTP_RESPONSE_JSON, ulen, str_json.c_str()); 	
    tcp_conn_->send(szContent);
    delete [] szContent;
	LOG_INFO << "    uuid: "<< uuid_; 
	return 0;
}

 
 

 int CHttpConn::_HandleUploadRequest(string &url, string &post_data)
{
	string str_json;
	int ret = ApiUpload(post_data, str_json);
	char *szContent = new char[HTTP_RESPONSE_JSON_MAX];
	uint32_t ulen = str_json.length();
	snprintf(szContent, HTTP_RESPONSE_JSON_MAX, HTTP_RESPONSE_JSON, ulen, str_json.c_str()); 	
    tcp_conn_->send(szContent);
    delete [] szContent;
	LOG_INFO << "    uuid: "<< uuid_; 
	return 0;
}

int CHttpConn::_HandleMyFilesRequest(string &url, string &post_data) 
{
	string str_json;
	int ret = ApiMyfiles(url, post_data, str_json);
	char *szContent = new char[HTTP_RESPONSE_JSON_MAX];
	uint32_t ulen = str_json.length();
	snprintf(szContent, HTTP_RESPONSE_JSON_MAX, HTTP_RESPONSE_JSON, ulen, str_json.c_str()); 	
    tcp_conn_->send(szContent);
    delete [] szContent;
	LOG_INFO << "    uuid: "<< uuid_; 
	return 0;
}

int CHttpConn::_HandleSharepictureRequest(string &url, string &post_data) 
{
	string str_json;
	int ret = ApiSharepicture(url, post_data, str_json);
	char *szContent = new char[HTTP_RESPONSE_JSON_MAX];
	uint32_t ulen = str_json.length();
	snprintf(szContent, HTTP_RESPONSE_JSON_MAX, HTTP_RESPONSE_JSON, ulen, str_json.c_str()); 	
    tcp_conn_->send(szContent);
    delete [] szContent;
	LOG_INFO << "    uuid: "<< uuid_; 
	return 0;
}
 