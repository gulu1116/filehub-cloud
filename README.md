## 一、项目概述

FileHub 是一个基于 C++ 和 muduo 网络库开发的云文件存储与共享系统，支持文件上传、下载、分享、转存、排行榜等功能。系统采用 Reactor 网络模型，使用 MySQL 存储元数据，Redis 作为缓存层，FastDFS 作为分布式文件存储系统。

**核心特性：**

- 🚀 基于 Reactor 多线程模型的高性能架构
- 💾 FastDFS 分布式文件存储
- 🔄 Redis 缓存层提升访问性能
- 🔐 Token 用户认证机制
- 📊 完整的文件上传、下载、分享、转存功能

## 二、系统架构

>详细架构文档参阅 `docs/` 目录

### 2.1 顶层目录

```
filehub-cloud/
├── application/         # 应用层代码
│   └── filehub/         # 核心业务代码
├── muduo/               # muduo 网络库（第三方依赖）
├── sql/                 # 数据库建表脚本
├── front/               # 前端代码（待补充）
├── client/              # 客户端测试代码
├── wrk/                 # 压力测试脚本
└── build/               # 构建目录
```

### 2.2 核心模块目录（application/filehub/）

```
filehub/
├── main.cc                     # 程序入口，服务器启动
├── http_conn.h/cc              # HTTP 连接封装，路由分发
├── http_parser.h/cc            # HTTP 协议解析器（底层）
├── http_parser_wrapper.h/cc    # HTTP 解析器封装
├── tc_http_server.conf         # 配置文件
│
├── api/                        # 业务 API 层
│   ├── api_common.h/cc         # 公共工具函数
│   ├── api_register.h/cc       # 用户注册
│   ├── api_login.h/cc          # 用户登录
│   ├── api_upload.h/cc         # 文件上传
│   ├── api_myfiles.h/cc        # 我的文件列表
│   ├── api_sharefiles.h/cc     # 共享文件列表
│   ├── api_dealfile.h/cc       # 文件操作（分享/删除/下载）
│   ├── api_deal_sharefile.h/cc # 共享文件操作（取消分享/转存/下载）
│   ├── api_sharepicture.h/cc   # 图片分享
│   └── api_md5.h/cc            # MD5 计算
│
├── mysql/                      # MySQL 数据库连接池
│   ├── db_pool.h/cc            # 连接池实现
│
├── redis/                      # Redis 缓存连接池
│   ├── cache_pool.h/cc         # 连接池实现
│   └── hiredis.*               # hiredis 客户端库
│
└── base/                       # 基础工具模块
    ├── config_file_reader.h/cc # 配置文件读取
    └── util.h/cc               # 工具函数
```

### 2.3 模块介绍

系统采用 **Reactor 多线程模型**，整体架构分为四层：

- **网络层**：基于 muduo 网络库，主 Reactor 负责连接接收，SubReactor 处理 I/O 事件
- **业务层**：HTTP 协议解析 + 路由分发 + API 业务处理
- **数据层**：MySQL 连接池存储元数据，Redis 连接池作为缓存
- **存储层**：FastDFS 分布式文件系统负责文件物理存储

核心数据流：客户端请求 → HTTP 解析 → 路由分发 → 业务处理 → 数据库/缓存操作 → FastDFS 存储 → 响应返回

### 2.4 数据库设计

```sql
user_info           # 用户信息
file_info           # 文件元数据
user_file_list      # 用户文件关系  
share_file_list     # 文件分享信息
share_picture_list  # 图片分享信息
```

### 2.5 业务API 列表

| API 路径 | 处理函数 | 功能说明 |
|---------|---------|---------|
| `/api/reg` | `_HandleRegisterRequest` | 用户注册 |
| `/api/login` | `_HandleLoginRequest` | 用户登录 |
| `/api/md5` | `_HandleMd5Request` | 计算文件 MD5 |
| `/api/upload` | `_HandleUploadRequest` | 文件上传 |
| `/api/myfiles` | `_HandleMyFilesRequest` | 获取我的文件列表 |
| `/api/sharefiles` | `_HandleSharefilesRequest` | 获取共享文件列表 |
| `/api/dealfile?cmd=share` | `_HandleDealfileRequest` | 分享文件 |
| `/api/dealfile?cmd=del` | `_HandleDealfileRequest` | 删除文件 |
| `/api/dealsharefile?cmd=cancel` | `_HandleDealsharefileRequest` | 取消分享 |
| `/api/dealsharefile?cmd=save` | `_HandleDealsharefileRequest` | 转存文件 |
| `/api/sharepic` | `_HandleSharepictureRequest` | 图片分享 |

## 三、快速入门

>环境搭建详见 **docs/环境搭建.md**

### 3.1 依赖环境

- **服务器**：VMware虚拟机或云主机
- **操作系统**：Ubuntu 20.04 64位
- **MySQL**：8.0（或5.7+）
- **Redis**：6.2.3
- **FastDFS**：libfastcommon 1.0.50 + FastDFS 6.0.7
- **Nginx**：1.16.1
- **fastdfs-nginx-module**：1.22
- **nginx_upload_module**：2.2.0（修复版）

**其他依赖**
```bash
# JSON库
sudo apt-get install libjsoncpp-dev

# UUID库
sudo apt-get install uuid-dev
```


### 3.2 项目编译

```bash
# 进入项目根目录
cd /path/to/filehub-cloud

# 创建构建目录
mkdir -p build
cd build

# 使用 CMake 配置（Debug 模式）
cmake -DCMAKE_BUILD_TYPE=Debug ..

# 或者使用 Release 模式
cmake -DCMAKE_BUILD_TYPE=Release ..

# 多线程编译
make -j4

# 编译完成后，可执行文件在 build/bin/ 目录下
# filehub - 服务器程序
# filehub-client - 客户端测试程序
```

编译完成后，将配置文件 tc_http_server.conf 拷贝到 build 目录。

修改 tc_http_server.conf 配置
```ini
# 数据库配置
filehub_master_host=localhost
filehub_master_username=your_username
filehub_master_password=your_password
filehub_master_dbname=filehub

# Redis 配置  
token_host=127.0.0.1
token_port=6379

# FastDFS 配置
dfs_path_client=/etc/fdfs/client.conf
storage_web_server_ip=your_storage_ip
```

### 3.3 服务启动

```bash
# 启动 MySQL
sudo /etc/init.d/mysql start

# 启动 Redis
redis-server /path/to/redis/redis.conf

# 启动 FastDFS Tracker
sudo /etc/init.d/fdfs_trackerd start

# 启动 FastDFS Storage
sudo /etc/init.d/fdfs_storaged start

# 启动 Nginx
sudo /usr/local/nginx/sbin/nginx

# 启动 Filehub 服务（需要 root 权限）
cd /path/to/filehub-cloud/build
sudo ./bin/filehub ./tc_http_server.conf
```

服务启动后默认监听 8081 端口，可通过客户端测试工具进行功能验证。
