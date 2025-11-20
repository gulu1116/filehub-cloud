#include "api_upload.h"

#include <unistd.h>
#include <sys/wait.h>

/*
*    上传本地文件到FastDFS，返回上传结果（0成功，-1失败）
*/
// 参数说明：
//   file_path：本地文件的绝对路径（如 "/home/user/test.jpg"）
//   fileid：输出参数，用于存储上传成功后的FastDFS文件ID（需提前分配足够内存，至少TEMP_BUF_MAX_LEN字节）
//   fileid 返回
int uploadFileToFastDfs(char *file_path, char *fileid) {

    int ret = 0;
    // s_dfs_path_client：全局变量，存储FastDFS客户端配置文件路径（如 "/etc/fdfs/client.conf"）
    if(s_dfs_path_client.empty()) {
        LOG_ERROR << "s_dfs_path_client is empty";
        return -1;
    }

    pid_t pid; // 存储fork返回的进程ID（子进程ID或错误码）
    int fd[2];

    //无名管道的创建
    if (pipe(fd) < 0) // fd[0] → r； fd[1] → w  获取上传后返回的信息 fileid
    {
        LOG_ERROR << "pipe error";
        ret = -1;
        goto END;
    }

    //创建进程
    pid = fork(); // 
    if (pid < 0)  //进程创建失败
    {
        LOG_ERROR << "fork error";
        ret = -1;
        goto END;
    }
     if (pid == 0) //子进程
    {
        //关闭读端
        close(fd[0]);
        //将标准输出 重定向 写管道
        dup2(fd[1], STDOUT_FILENO); // 往标准输出写的东西都会重定向到fd所指向的文件,
                             // 当fileid产生时输出到管道fd[1]
        // fdfs_upload_file /etc/fdfs/client.conf 123.txt
        // printf("fdfs_upload_file %s %s %s\n", fdfs_cli_conf_path, filename,
        // file_path);
        //通过execlp执行fdfs_upload_file
        //如果函数调用成功,进程自己的执行代码就会变成加载程序的代码,execlp()后边的代码也就不会执行了.
        execlp("fdfs_upload_file", "fdfs_upload_file",
               s_dfs_path_client.c_str(), file_path, NULL);
        // 上传成功后，fdfs_upload_file 工具自动将 fileid 输出到「标准输出（STDOUT）」
        // 执行正常不会跑下面的代码
        //执行失败
        LOG_ERROR << "execlp fdfs_upload_file error";

        close(fd[1]);
    }else //父进程
    {
        //关闭写端
        close(fd[1]);

        // 从管道读端读取子进程的输出（即fileid）
        // 从fd读取count字节到buf，父进程会一直等待，直到子进程写入数据并关闭写端
        read(fd[0], fileid, TEMP_BUF_MAX_LEN); // 等待管道写入然后读取

        LOG_INFO << "fileid1: " <<  fileid;
        //去掉一个字符串两边的空白字符
        TrimSpace(fileid);

        if (strlen(fileid) == 0) {
            LOG_ERROR << "upload failed";
            ret = -1;
            goto END;
        }
        LOG_INFO << "fileid2: " <<  fileid;

        wait(NULL); //等待子进程结束，回收其资源
        close(fd[0]);
    }

END:
    return ret;
}

/*
*    根据FastDFS的fileid获取文件的完整url地址，返回结果（0成功，-1失败）
*/

// 参数说明：
//   fileid：FastDFS文件ID（如 "group1/M00/00/00/xxx.jpg"，上传函数返回的结果）
//   fdfs_file_url：输出参数，存储拼接后的完整URL（需提前分配足够内存，避免拼接溢出）
// 返回值：0成功，-1失败（配置缺失、管道/fork失败、工具执行失败等）

int getFullUrlByFileid(char *fileid, char *fdfs_file_url) {
    if(s_storage_web_server_ip.empty()) {
        LOG_ERROR << "s_storage_web_server_ip is empty";
        return -1;
    }    

    int ret = 0;

    // 字符串指针，用于解析工具输出的IP：
    // p：指向"source ip address: "的起始位置
    // q：指向存储节点IP的起始位置（跳过p的标记字符串）
    // k：指向存储节点IP的结束位置（换行符\n）
    char *p = NULL;
    char *q = NULL;
    char *k = NULL;

    char fdfs_file_stat_buf[TEMP_BUF_MAX_LEN] = {0};
    char fdfs_file_host_name[HOST_NAME_LEN] = {0}; // storage所在服务器ip地址

    pid_t pid;
    int fd[2];

    //无名管道的创建
    if (pipe(fd) < 0) {
        LOG_ERROR << "pipe error";
        ret = -1;
        goto END;
    }

    //创建进程
    pid = fork();
    if (pid < 0) //进程创建失败
    {
        LOG_ERROR << "fork error";
        ret = -1;
        goto END;
    }

    if (pid == 0) //子进程
    {
        //关闭读端
        close(fd[0]);
        //将标准输出 重定向 写管道 ->  fdfs_file_info工具的输出会写入管道，而非终端
        dup2(fd[1], STDOUT_FILENO); // dup2(fd[1], 1);


        // 执行FastDFS文件信息查询工具fdfs_file_info
        // 命令格式：fdfs_file_info [客户端配置文件路径] [fileid]
        // 工具输出示例：
        //   group name: group1
        //   remote filename: M00/00/00/xxx.jpg
        //   source ip address: 192.168.1.100  （关键：存储节点的局域网IP）
        //   file size: 123456
        //   ...

        execlp("fdfs_file_info", "fdfs_file_info", s_dfs_path_client.c_str(),
               fileid, NULL);

        //执行失败
        LOG_ERROR << "execlp fdfs_file_info error";

        close(fd[1]);
    } else //父进程
    {
        //关闭写端
        close(fd[1]);

        // 从管道读端读取子进程的输出（工具fdfs_file_info的完整输出）
        // 阻塞等待：直到子进程关闭写端（工具执行完毕），才会返回读取结果
        read(fd[0], fdfs_file_stat_buf, TEMP_BUF_MAX_LEN);

        wait(NULL); //等待子进程结束，回收其资源
        close(fd[0]);

        // 从工具输出中提取存储节点IP（核心）
        // strstr(const char *haystack, const char *needle)：在haystack中查找needle的首次出现
        // 这里查找标记字符串"source ip address: "，p指向该字符串的起始位置
        p = strstr(fdfs_file_stat_buf, "source ip address: ");

        q = p + strlen("source ip address: ");
        k = strstr(q, "\n");  // k指向IP的结束位置：查找q之后的第一个换行符\n

        strncpy(fdfs_file_host_name, q, k - q);
        fdfs_file_host_name[k - q] = '\0'; // 这里这个获取回来只是局域网的ip地址

        LOG_INFO << "host_name: " << s_storage_web_server_ip << ", fdfs_file_host_name: " <<  fdfs_file_host_name;

        // storage_web_server服务器的端口
        strcat(fdfs_file_url, "http://");
        strcat(fdfs_file_url, s_storage_web_server_ip.c_str());
        // strcat(fdfs_file_url, ":");
        // strcat(fdfs_file_url, s_storage_web_server_port.c_str());
        strcat(fdfs_file_url, "/");
        strcat(fdfs_file_url, fileid);

        LOG_INFO << "fdfs_file_url:" <<  fdfs_file_url;
        // 拼接上传文件的完整url地址--->http://host_name/group1/M00/00/00/D12313123232312.png
    }

END:

    return ret;
}

// 函数功能：将文件上传后的关键信息存储到数据库，并预留缓存更新逻辑
// 参数说明：
//   db_conn：数据库连接对象（已封装连接、执行SQL等操作）
//   cache_conn：缓存连接对象（用于更新热点数据缓存）
//   user：文件所属用户名（如 "test_user"）
//   filename：原始文件名（如 "风景.jpg"）
//   md5：文件的MD5值（用于文件去重、唯一标识）
//   size：文件大小（单位：字节）
//   fileid：FastDFS返回的文件ID（如 "group1/M00/00/00/xxx.jpg"）
//   fdfs_file_url：完整的文件HTTP访问URL（getFullUrlByFileid函数返回的结果）
// 返回值：0成功（双表插入均成功），-1失败（任一表插入失败）
int storeFileinfo(CDBConn *db_conn, CacheConn *cache_conn, char *user,
                  char *filename, char *md5, long size, char *fileid,
                  const char *fdfs_file_url) {
    int ret = 0;
    time_t now;
    char create_time[TIME_STRING_LEN];
    char suffix[SUFFIX_LEN];
    char sql_cmd[SQL_MAX_LEN] = {0};

    //得到文件后缀字符串 如果非法文件后缀,返回"null"
    GetFileSuffix(filename, suffix); // mp4, jpg, png

    // sql 语句
    /*
       -- =============================================== 文件信息表
       -- md5 文件md5
       -- file_id 文件id
       -- url 文件url
       -- size 文件大小, 以字节为单位
       -- type 文件类型： png, zip, mp4……
       -- count 文件引用计数， 默认为1， 每增加一个用户拥有此文件，此计数器+1
       */
    sprintf(sql_cmd,
            "insert into file_info (md5, file_id, url, size, type, count) "
            "values ('%s', '%s', '%s', '%ld', '%s', %d)",
            md5, fileid, fdfs_file_url, size, suffix, 1);
    LOG_INFO << "执行: " <<  sql_cmd;
    if (!db_conn->ExecuteCreate(sql_cmd)) //执行sql语句
    {
        LOG_ERROR << sql_cmd << " 操作失败";
        ret = -1;
        goto END;
    }

    //获取当前时间
    now = time(NULL);
    strftime(create_time, TIME_STRING_LEN - 1, "%Y-%m-%d %H:%M:%S", localtime(&now));

    /*
       -- =============================================== 用户文件列表
       -- user 文件所属用户
       -- md5 文件md5
       -- create_time 文件创建时间
       -- file_name 文件名字
       -- shared_status 共享状态, 0为没有共享， 1为共享
       -- pv 文件下载量，默认值为0，下载一次加1
       */
    // sql语句
    sprintf(sql_cmd,
            "insert into user_file_list(user, md5, create_time, file_name, "
            "shared_status, pv) values ('%s', '%s', '%s', '%s', %d, %d)",
            user, md5, create_time, filename, 0, 0);
    // LOG_INFO << "执行: " <<  sql_cmd;
    if (!db_conn->ExecuteCreate(sql_cmd)) {
        LOG_ERROR << sql_cmd << " 操作失败";
        ret = -1;
        goto END;
    }

    // // 询用户文件数量+1      web热点 大明星  微博存在缓存里面。
    // if (CacheIncrCount(cache_conn, string(user)) < 0) {
    //     LOG_ERROR << " CacheIncrCount 操作失败";
    // }

END:
    return ret;
}

int ApiUpload(string &post_data, string &str_json){

    LOG_INFO << "post_data:\n" << post_data;
    char suffix[SUFFIX_LEN] = {0};               // 后缀
    char fileid[TEMP_BUF_MAX_LEN] = {0};         // 文件上传到fastDFS后的文件id
    char fdfs_file_url[FILE_URL_LEN] = {0};      // 文件所存放storage的host_name
    int ret = 0;
    char boundary[TEMP_BUF_MAX_LEN] = {0};       // 分界线信息 分隔符
    char file_name[128] = {0};                   // 文件名
    char file_content_type[128] = {0};           // 文件类型
    char file_path[128] = {0};                   // 文件路径
    char new_file_path[128] = {0};               // 新文件路径
    char file_md5[128] = {0};                    // 文件的md5
    char file_size[32] = {0};                    // 文件的大小
    long long_file_size = 0;                     // 转成long类型的文件大小
    char user[32] = {0};                         // 用户名

    char *begin = (char *)post_data.c_str();
    char *p1, *p2;                               // 临时的界限位置

     Json::Value value;

     // 获取数据库连接
    CDBManager *db_manager = CDBManager::getInstance();
    CDBConn *db_conn = db_manager->GetDBConn("filehub_master"); // 连接池可以配置多个 分库
    AUTO_REL_DBCONN(db_manager, db_conn);

    //分隔符
    // 1. 解析boundary
    // Content-Type: multipart/form-data;
    // boundary=----WebKitFormBoundaryjWE3qXXORSg2hZiB 找到起始位置
    p1 = strstr(begin, "\r\n"); // 作用是返回字符串中首次出现子串的地址
    if (p1 == NULL) {
        LOG_ERROR << "wrong no boundary!";
        ret = -1;
        goto END;
    }
    //拷贝分界线
    strncpy(boundary, begin, p1 - begin); // 缓存分界线, 比如：WebKitFormBoundary88asdgewtgewx
    boundary[p1 - begin] = '\0'; //字符串结束符
    LOG_INFO << "boundary: " <<  boundary; //打印出来

     // 查找文件名file_name 匹配字符串的算法
    begin = p1 + 2;  // 2->\r\n
    p2 = strstr(begin, "name=\"file_name\""); //找到file_name字段
    if (!p2) {
        LOG_ERROR << "wrong no file_name!";
        ret = -1;
        goto END;
    }
    p2 = strstr(begin, "\r\n"); // 找到file_name下一行
    p2 += 4;                    //下一行起始
    begin = p2;                 //  
    p2 = strstr(begin, "\r\n");
    strncpy(file_name, begin, p2 - begin);
    LOG_INFO << "file_name: " <<  file_name;


    // 查找文件类型file_content_type
    begin = p2 + 2;
    p2 = strstr(begin, "name=\"file_content_type\""); //
    if (!p2) {
        LOG_ERROR << "wrong no file_content_type!";
        ret = -1;
        goto END;
    }
    p2 = strstr(p2, "\r\n");
    p2 += 4;
    begin = p2;
    p2 = strstr(begin, "\r\n");
    strncpy(file_content_type, begin, p2 - begin);
    LOG_INFO << "file_content_type: " <<  file_content_type;

 // 查找文件file_path
    begin = p2 + 2;
    p2 = strstr(begin, "name=\"file_path\""); //
    if (!p2) {
        LOG_ERROR << "wrong no file_path!";
        ret = -1;
        goto END;
    }
    p2 = strstr(p2, "\r\n");
    p2 += 4;
    begin = p2;
    p2 = strstr(begin, "\r\n");
    strncpy(file_path, begin, p2 - begin);
    LOG_INFO << "file_path: " <<  file_path;

    // 查找文件file_md5
    begin = p2 + 2;
    p2 = strstr(begin, "name=\"file_md5\""); //
    if (!p2) {
        LOG_ERROR << "wrong no file_md5!";
        ret = -1;
        goto END;
    }
    p2 = strstr(p2, "\r\n");
    p2 += 4;
    begin = p2;
    p2 = strstr(begin, "\r\n");
    strncpy(file_md5, begin, p2 - begin);
    LOG_INFO << "file_md5: " <<  file_md5;

    // 查找文件file_size
    begin = p2 + 2;
    p2 = strstr(begin, "name=\"file_size\""); //
    if (!p2) {
        LOG_ERROR << "wrong no file_size!";
        ret = -1;
        goto END;
    }
    p2 = strstr(p2, "\r\n");
    p2 += 4;
    begin = p2;
    p2 = strstr(begin, "\r\n");
    strncpy(file_size, begin, p2 - begin);
    LOG_INFO << "file_size: " <<  file_size;
    long_file_size = strtol(file_size, NULL, 10); //字符串转long

    // 查找user
    begin = p2 + 2;
    p2 = strstr(begin, "name=\"user\""); //
    if (!p2) {
        LOG_ERROR << "wrong no user!";
        ret = -1;
        goto END;
    }
    p2 = strstr(p2, "\r\n");
    p2 += 4;
    begin = p2;
    p2 = strstr(begin, "\r\n");
    strncpy(user, begin, p2 - begin);
    LOG_INFO << "user: " <<  user;


    //把临时文件拷贝到fastdfs  /root/tmp/9/0035297749, 后缀呢？
    //从文件名获取文件后缀
    //把临时文件 改成带后缀名文件
    // 获取文件名后缀
    GetFileSuffix(file_name, suffix); //  20230720-2.txt -> txt  mp4, jpg, png
    strcat(new_file_path, file_path); // /root/tmp/1/0045118901
    strcat(new_file_path, ".");  // /root/tmp/1/0045118901.
    strcat(new_file_path, suffix); // /root/tmp/1/0045118901.txt
    // 重命名 修改文件名  fastdfs 他需要带后缀的文件
    ret = rename(file_path, new_file_path); /// /root/tmp/1/0045118901 ->  /root/tmp/1/0045118901.txt sudo 
    if (ret < 0) {
        LOG_ERROR << "rename " << file_path << " to " << new_file_path << " failed" ;
        ret = -1;
        goto END;
    }  
     //===============> 将该文件存入fastDFS中,并得到文件的file_id <============
    LOG_INFO << "uploadFileToFastDfs, file_name:" << file_name << ", new_file_path:" <<  new_file_path;
    if (uploadFileToFastDfs(new_file_path, fileid) < 0) {
        LOG_ERROR << "uploadFileToFastDfs failed, unlink: " <<  new_file_path;
        ret = unlink(new_file_path);
        if (ret != 0) {
            LOG_ERROR << "unlink: " << new_file_path << " failed"; // 删除失败则需要有个监控重新清除过期的临时文件，比如过期两天的都删除
        }
        ret = -1;
        goto END;
    }
    //================> 删除本地临时存放的上传文件 <===============
    LOG_INFO << "unlink: " <<  new_file_path;
    ret = unlink(new_file_path);
    if (ret != 0) {
        LOG_WARN << "unlink: " << new_file_path << " failed"; // 删除失败则需要有个监控重新清除过期的临时文件，比如过期两天的都删除
    }

    //================> 得到文件所存放storage的host_name <=================
    // 拼接出完整的http地址
    LOG_INFO << "getFullurlByFileid, fileid: " <<  fileid;
    if (getFullUrlByFileid(fileid, fdfs_file_url) < 0) {
        LOG_ERROR << "getFullurlByFileid failed ";
        ret = -1;
        goto END;
    }
    
    
  
    //===============> 将该文件的FastDFS相关信息存入mysql中 <======
    LOG_INFO << "storeFileinfo, origin url: " << fdfs_file_url ;// << " -> short url: " <<  short_url;
    // 把文件写入file_info
    if (storeFileinfo(db_conn, NULL, user, file_name, file_md5,
                      long_file_size, fileid, fdfs_file_url) < 0) {
        LOG_ERROR << "storeFileinfo failed ";
        ret = -1;
        // 严谨而言，这里需要删除 已经上传的文件
        //从storage服务器删除此文件，参数为为文件id
        ret = RemoveFileFromFastDfs(fileid);
        if (ret != 0) {
            LOG_ERROR << "RemoveFileFromFastDfs err: " <<  ret;
        }
        ret = -1;
        goto END;
    }
    ret = 0;
    value["code"] = 0;
    str_json = value.toStyledString(); // json序列化,  直接用writer是紧凑方式，这里toStyledString是格式化更可读方式

    return 0;
END:
    value["code"] = 1;
    str_json = value.toStyledString(); // json序列化

    return -1;
}

int ApiUploadInit(const char *dfs_path_client, 
                    const char *storage_web_server_ip, const char *storage_web_server_port, 
                  const char *shorturl_server_address, const char *access_token) {
    s_dfs_path_client = dfs_path_client;
    s_storage_web_server_ip = storage_web_server_ip;
    s_storage_web_server_port = storage_web_server_port;
    s_shorturl_server_address = shorturl_server_address;
    s_shorturl_server_access_token = access_token;
    return 0;
}