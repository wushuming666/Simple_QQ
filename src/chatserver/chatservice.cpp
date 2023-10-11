#include "chatservice.hpp"
#include "public.hpp"
#include <muduo/base/Logging.h>
#include <vector>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <iostream>

using namespace std;
using namespace muduo;

ChatService *ChatService::instance()
{
    static ChatService service;
    return &service;
}

// 注册消息以及对应的回调操作
ChatService::ChatService()
{
	// 用户基本业务管理相关事件处理回调注册
    _msgHandlerMap.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
	
    _msgHandlerMap.insert({REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});
    // 群组业务管理相关事件处理回调注册
    _msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});
    // 文件路径存入
    _msgHandlerMap.insert({FILE_PATH, std::bind(&ChatService::addfilePath, this, _1, _2, _3)});
    _msgHandlerMap.insert({VIDEO, std::bind(&ChatService::mergevideos, this, _1, _2, _3)});  
}

// 服务器异常，业务重置方法
void ChatService::reset()
{
    // 把online状态的用户，设置为offline
    _userModel.resetState();
}

// 获取消息对应的处理器
MsgHandler ChatService::getHandler(int msgid)
{
    // 记录错误日志, msgid没有对应的事件处理回调
    auto it = _msgHandlerMap.find(msgid);
    if (it == _msgHandlerMap.end())
    {
        // 返回一个默认的处理器, 空
        return [=](const TcpConnectionPtr &conn, json &js, Timestamp)
        {
            LOG_ERROR << "msgid:" << msgid << " can not find handler!";
        };
    }
    else
    {
        return _msgHandlerMap[msgid];
    }
}

// 处理登录业务 id pwd
void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time)
{

// FILE *file;
// const char *filename = "/project/wsm/server/server.log";    
// file = fopen(filename, "a");

    int id = js["id"].get<int>();
    string pwd = js["password"];

// fprintf(file, "pid = %d, id = %d, msgid = %d, pwd = %s\n", getpid(), id, msgid, pwd.c_str());
    
    User user = _userModel.query(id);
// fprintf(file, "pid = %d before query\n", getpid());

    // log

// fprintf(file, "pid = %d, id = %d, name = %s \n", getpid(), user.getId(), user.getName().c_str());
// fclose(file);

    if (user.getId() == id && user.getPwd() == pwd)
    {
        if (user.getState() == "online")
        {
            // 用户已经登录, 不允许重复登录
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;
            response["errmsg"] = "this account is using, input another!";
            conn->send(response.dump());           
        }
        else
        {
            // 登录成功，记录用户连接信息
            {
                lock_guard<mutex> lock(_connMutex);
                _userConn.put(id, conn);
                _ConnToUser.insert({conn, id});
            }

            // 登录成功  更新用户状态信息
            user.setState("online");
            _userModel.updateState(user);

            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();
            // 查询该用户是否有离线消息
            vector<string> vec = _offlineMsgModel.query(id);
            if(!vec.empty())
            {
                response["offlinemsg"] = vec;
                // 读取该用户的离线消息后，把该用户的所有离线消息删除掉
                _offlineMsgModel.remove(id);
            }
            //查询该用户的好友信息并返回
            vector<User> userVec = _friendModel.query(id);
            if(!userVec.empty())
            {
                vector<string> vec2;
                for(User &user : userVec)
                {
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    vec2.push_back(js.dump());
                }
                response["friends"] = vec2;
            }

            // 查询用户的群组信息
            vector<Group> groupuserVec = _groupModel.queryGroups(id);
            if (!groupuserVec.empty())
            {
                // group:[{groupid:[xxx, xxx, xxx, xxx]}]
                vector<string> groupV;
                for (Group &group : groupuserVec)
                {
                    json grpjson;
                    grpjson["id"] = group.getId();
                    grpjson["groupname"] = group.getName();
                    grpjson["groupdesc"] = group.getDesc();
                    vector<string> userV;
                    for (GroupUser &user : group.getUsers())
                    {
                        json js;
                        js["id"] = user.getId();
                        js["name"] = user.getName();
                        js["state"] = user.getState();
                        js["role"] = user.getRole();
                        userV.push_back(js.dump());
                    }
                    grpjson["users"] = userV;
                    groupV.push_back(grpjson.dump());
                }

                response["groups"] = groupV;
            }
            conn->send(response.dump());
        }
    }
    else
    {
        // 该用户不存在、用户存在但是密码错误，登录失败
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "id or password is invalid!";
        conn->send(response.dump());
    }
}

// 处理注册业务 name password
void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    string name = js["name"];
    string pwd = js["password"];

    // 经过测试 pwd没问题
    // LOG_INFO << pwd;

    User user;
    user.setName(name);
    user.setPwd(pwd);
    bool state = _userModel.insert(user);
    if (state)
    {
        // 注册成功
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0;
        response["id"] = user.getId();
        conn->send(response.dump());
    }
    else
    {
        // 注册失败
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1;
        conn->send(response.dump());
    }
}

// 处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr &conn)
{
    User user;
    {
        lock_guard<mutex> lock(_connMutex);

        if (_ConnToUser.count(conn))
        {
            int id = _ConnToUser[conn];
            user.setId(id);
            _userConn.remove(id);
        }

    }

    // 更新用户的状态信息
    if (user.getId() != -1)
    {
        user.setState("offline");
        _userModel.updateState(user);
    }
}

// 一对一聊天业务
void ChatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int toid = js["toid"].get<int>();
    
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConn.get(toid);
        if(it != nullptr) 
        {
            // 在线
            it->send(js.dump());
            return;
        }
    }

    // 查询toid是否在线 
    User user = _userModel.query(toid);
    if (user.getState() == "online")
    {
        // _redis.publish(toid, js.dump());
        // return;
    }
    // toid 不在线，存储离线消息
    _offlineMsgModel.insert(toid, js.dump());

}

// 添加好友业务 msgid userid friendid
void ChatService::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();

    // 存储好友信息
    _friendModel.insert(userid, friendid);
}

// 创建群组业务
void ChatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];

    // 存储新创建的群组信息
    Group group(-1, name, desc);
    if (_groupModel.createGroup(group))
    {
        _groupModel.addGroup(userid, group.getId(), "creator");
    }
}

// 加入群组业务
void ChatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    _groupModel.addGroup(userid, groupid, "normal");
}

// 群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    vector<int> useridVec = _groupModel.queryGroupUsers(userid, groupid);

    lock_guard<mutex> lock(_connMutex);
    for (int id : useridVec)
    {
        auto it = _userConn.get(id);
        if (it != nullptr)
        {
            // 转发群消息
            it->send(js.dump());
        }
        else
        {
            // 查询toid是否在线 
            User user = _userModel.query(id);
            if (user.getState() == "online")
            {
                // _redis.publish(id, js.dump());
            }
            else
            {
                // 存储离线群消息
                _offlineMsgModel.insert(id, js.dump());
            }
        }
    }
}

// 往编号为id的User表里面插入文件的路径
void ChatService::addfilePath(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
	int userid = js["id"].get<int>();
	string filepath = js["path"];
	Filepath tmp(userid, filepath);
	_filepathModel.insert(tmp);	
}

void ChatService::mergevideos(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    string path1 = js["path1"];
    string path2 = js["path2"];
    string targetfile = js["targetfile"];
    targetfile = string("/tmp/tcp/videopath/") + targetfile;
    int start_time = js["start"].get<int>();
    int end_time = js["end"].get<int>();

    // 使用fork调用merge函数
    char* pargv[10];
    for (int i = 0; i < 10; i ++) pargv[i] = new char[256];
    strcpy(pargv[1], path1.c_str());
    strcpy(pargv[2], path2.c_str());
    strcpy(pargv[3], targetfile.c_str());
    strcpy(pargv[4], to_string(start_time).c_str());
    strcpy(pargv[5], to_string(end_time).c_str());
    pargv[6] = NULL;

// for (int i = 1; i < 6; i ++)
// {
//     printf("wsm %s\n", pargv[i]);
// }

    if (fork() == 0)
    {
        execv("/project/wsm/server/src/chatserver/ffmpeg/merge", pargv);
        exit(0);
    }
    else
    {
        int status;
        wait(&status);
    }

    for (int i = 1; i < 6; i++) {
        delete[] pargv[i];
    }

    // 向客户端发送ok信号
    json response;
    response["msgid"] = VIDEO_ACK;
    conn->send(response.dump());
}
