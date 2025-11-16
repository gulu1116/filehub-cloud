#include "api_login.h"

#include <iostream>
#include "muduo/base/Logging.h" // Logger日志头文件
#include <json/json.h>


using namespace std;


// / 解析登录信息
int decodeLoginJson(const std::string &str_json, string &user_name,
                    string &pwd) {
    bool res;
    Json::Value root;
    Json::Reader jsonReader;
    res = jsonReader.parse(str_json, root);
    if (!res) {
        LOG_ERROR << "parse login json failed ";
        return -1;
    }
    // 用户名
    if (root["user"].isNull()) {
        LOG_ERROR << "user null";
        return -1;
    }
    user_name = root["user"].asString();

    //密码
    if (root["pwd"].isNull()) {
        LOG_ERROR << "pwd null";
        return -1;
    }
    pwd = root["pwd"].asString();

    return 0;
}

// 封装登录结果的json
int encodeLoginJson(int code, string &token, string &str_json) {
    Json::Value root;
    root["code"] = code;
    if (code == 0) {
        root["token"] = token; // 正常返回的时候才写入token
    }
    Json::FastWriter writer;
    str_json = writer.write(root);
    return 0;
}

int verifyUserPassword(string &user_name, string &pwd) {
    int ret = 0;
    // 这里暂时不做处理，因为这里还没有涉及数据库

    return ret;
}

int setToken(string &user_name, string &token) {
    int ret = 0;
    token = "1234";
    //更新到redis
    return ret;
}
 

int ApiUserLogin(std::string &post_data, std::string &resp_json){
    string user_name;
    string pwd;
    string token;
    // 判断数据是否为空
    if (post_data.empty()) {
        encodeLoginJson(1, token, resp_json);
        return -1;
    }

     // 解析json
    if (decodeLoginJson(post_data, user_name, pwd) < 0) {
        LOG_ERROR << "decodeRegisterJson failed";
        encodeLoginJson(1, token, resp_json);
        return -1;
    }
    // 验证账号和密码是否匹配
    if (verifyUserPassword(user_name, pwd) < 0) {
        LOG_ERROR << "verifyUserPassword failed";
        encodeLoginJson(1, token, resp_json);
        return -1;
    }
    // 生成token

    if (setToken(user_name, token) < 0) {
        LOG_ERROR << "setToken failed";
        encodeLoginJson(1, token, resp_json);
        return -1;
    }

     // 封装登录结果
    encodeLoginJson(0, token, resp_json);

    return 0;
}
