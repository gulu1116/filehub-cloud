#include "api_register.h"
#include <iostream>

#include <json/json.h>
#include "muduo/base/Logging.h" // Logger日志头文件

using namespace std;

int encodeRegisterJson(int code, string &str_json) {
    Json::Value root;
    root["code"] = code;
    Json::FastWriter writer;
    str_json = writer.write(root);
    return 0;
}

int decodeRegisterJson(const std::string &str_json, string &user_name,
                       string &nick_name, string &pwd, string &phone,
                       string &email)
{   
    bool res;
    Json::Value root;
    Json::Reader jsonReader;
    res = jsonReader.parse(str_json, root);    
    if (!res) {
        LOG_ERROR << "parse reg json failed ";
        return -1;
    }
    // 用户名
    if (root["userName"].isNull()) {
        LOG_ERROR << "userName null";
        return -1;
    }
    user_name = root["userName"].asString();

    // 昵称
    if (root["nickName"].isNull()) {
        LOG_ERROR << "nickName null";
        return -1;
    }
    nick_name = root["nickName"].asString();

    //密码
    if (root["firstPwd"].isNull()) {
        LOG_ERROR << "firstPwd null";
        return -1;
    }
    pwd = root["firstPwd"].asString();

     //电话  非必须
    if (root["phone"].isNull()) {
        LOG_WARN << "phone null";
    } else {
        phone = root["phone"].asString();
    }

    //邮箱 非必须
    if (root["email"].isNull()) {
        LOG_WARN << "email null";
    } else {
        email = root["email"].asString();
    }

}

int registerUser(string &user_name, string &nick_name, string &pwd,
                 string &phone, string &email) {
    int ret = 0;
    // 还没有处理，先直接返回0

    return ret;
}


int ApiRegisterUser(string &post_data, string &resp_json) {

    int ret = 0;
    string user_name;
    string nick_name;
    string pwd;
    string phone;
    string email;

    // json 反序列化
    LOG_INFO << "post_data: " <<  post_data;


    
    // 判断数据是否为空
    if (post_data.empty()) {
        LOG_ERROR << "decodeRegisterJson failed";
        //序列化 把结果返回给客户端
        // code = 1
        encodeRegisterJson(1, resp_json);
        return -1;
    }
    // json反序列化

    ret = decodeRegisterJson(post_data, user_name, nick_name, pwd, phone, email);
    if(ret < 0) {
        encodeRegisterJson(1, resp_json);
        return -1;
    }

    // 注册账号
    // 先在数据库查询用户名 昵称 是否存在 如果不存在才去注册
    ret = registerUser(user_name, nick_name, pwd, phone, email); //先不操作数据库看看性能

    encodeRegisterJson(ret, resp_json);

    return 0;
}