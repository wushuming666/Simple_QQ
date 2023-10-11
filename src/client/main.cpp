#include "json.hpp"
#include "putfile.hpp"
#include "getfile.hpp"
#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
#include <unordered_map>
#include <functional>
#include <sstream>

using namespace std;
using json = nlohmann::json;

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include <atomic>

#include "group.hpp"
#include "user.hpp"
#include "public.hpp"

// 文件上传下载
class putfile pfile;
class getfile gfile;

// 记录当前系统登录的用户信息
User g_currentUser;
// 记录当前登录用户的好友列表
vector<User> g_currentUserFriendList;
// 记录当前登录用户的群组列表信息
vector<Group> g_currentUserGroupList;

// 控制主菜单页面程序
bool isMainMenuRunning = false;

// 用于读写线程之间的通信
sem_t rwsem;
// 记录登录状态
atomic_bool g_isLoginSuccess{false};

// 接收线程
void readTaskHandler(int clientfd);
// 获取系统时间
string getCurrentTime();
// 主聊天页面程序
void mainMenu(int);
// 显示当前登录成功用户的基本信息
void showCurrentUserData();

char *ip; // 全局记录ip地址
int id = 0; // 用户id

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        cerr << "command invalid! example: ./ChatClient 127.0.0.1 6000" << endl;
        exit(-1);
    }

    // 解析命令行参数
    ip = argv[1];
    uint16_t port = atoi(argv[2]);

    // 创建client端的socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == clientfd)
    {
        cerr << "socket create error" << endl;
        exit(-1);
    }

    // 填写client需要连接的server信息ip+port
    sockaddr_in server;
    memset(&server, 0, sizeof(sockaddr_in));

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(ip);

    // client 和 server 进行连接
    if (-1 == connect(clientfd, (sockaddr *)&server, sizeof(sockaddr_in)))
    {
        cerr << "connect server error" << endl;
        close(clientfd);
        exit(-1);
    }

    // 初始化读写线程通信用的信号量
    sem_init(&rwsem, 0, 0);

    // 连接服务器成功, 启动接收子线程
    std::thread readTask(readTaskHandler, clientfd);
    readTask.detach();

    for(;;)
    {
        // 显示首页面菜单 登录、注册、退出
        cout << "========================" << endl;
        cout << "1. login" << endl;
        cout << "2. register" << endl;
        cout << "3. quit" << endl;
        cout << "========================" << endl;
        cout << "choice:";
        int choice = 0;
        cin >> choice;
        cin.get(); // 读掉缓冲区残留的回车

        switch (choice)
        {
        case 1: // login业务
        {
            char pwd[50] = {0};
            cout << "userid:";
            cin >> id;
            cin.get(); // 读掉缓冲区残留的回车
            cout << "userpassword:";
            cin.getline(pwd, 50);

            json js;
            js["msgid"] = LOGIN_MSG;
            js["id"] = id;
            js["password"] = pwd;
            string request = js.dump();

            g_isLoginSuccess = false;

            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            if (len == -1)
            {
                cerr << "send login msg error:" << request << endl;
            }

            sem_wait(&rwsem); // 等待信号量，由子线程处理完登录的响应消息后，通知这里
                
            if (g_isLoginSuccess) 
            {
                // 进入聊天主菜单页面
                isMainMenuRunning = true;
                mainMenu(clientfd);
            }
        }
        break;
        case 2: // register业务
        {
            char name[50] = {0};
            char pwd[50] = {0};
            cout << "username:";
            cin.getline(name, 50);
            cout << "userpassword:";
            cin.getline(pwd, 50);

            json js;
            js["msgid"] = REG_MSG;
            js["name"] = name;
            js["password"] = pwd;
            string request = js.dump();

            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            if (len == -1)
            {
                cerr << "send reg msg error:" << request << endl;
            }
            
            sem_wait(&rwsem); // 等待信号量，子线程处理完注册消息会通知
        }
        break;
        case 3: // quit业务
            close(clientfd);
            sem_destroy(&rwsem);
            exit(0);
        default:
            cerr << "invalid input!" << endl;
            break;
        }        
    }

    return 0;
}

// 处理注册的响应逻辑
void doRegResponse(json &responsejs)
{
    if (0 != responsejs["errno"].get<int>()) // 注册失败
    {
        cerr << "name is already exist, register error!" << endl;
    }
    else // 注册成功
    {
        cout << "name register success, userid is " << responsejs["id"]
                << ", do not forget it!" << endl;
    }
}

// 处理登录的响应逻辑
void doLoginResponse(json &responsejs)
{
    if (0 != responsejs["errno"].get<int>()) // 登录失败
    {
        cerr << responsejs["errmsg"] << endl;
        g_isLoginSuccess = false;
    }
    else // 登录成功
    {
        // 记录当前用户的id和name
        g_currentUser.setId(responsejs["id"].get<int>());
        g_currentUser.setName(responsejs["name"]);

        // 记录当前用户的好友列表信息
        if (responsejs.contains("friends"))
        {
            // 初始化
            g_currentUserFriendList.clear();

            vector<string> vec = responsejs["friends"];
            for (string &str : vec)
            {
                json js = json::parse(str);
                User user;
                user.setId(js["id"].get<int>());
                user.setName(js["name"]);
                user.setState(js["state"]);
                g_currentUserFriendList.push_back(user);
            }
        }

        // 记录当前用户的群组列表信息
        if (responsejs.contains("groups"))
        {
            // 初始化
            g_currentUserGroupList.clear();

            vector<string> vec1 = responsejs["groups"];
            for (string &groupstr : vec1)
            {
                json grpjs = json::parse(groupstr);
                Group group;
                group.setId(grpjs["id"].get<int>());
                group.setName(grpjs["groupname"]);
                group.setDesc(grpjs["groupdesc"]);

                vector<string> vec2 = grpjs["users"];
                for (string &userstr : vec2)
                {
                    GroupUser user;
                    json js = json::parse(userstr);
                    user.setId(js["id"].get<int>());
                    user.setName(js["name"]);
                    user.setState(js["state"]);
                    user.setRole(js["role"]);
                    group.getUsers().push_back(user);
                }

                g_currentUserGroupList.push_back(group);
            }
        }

        // 显示登录用户的基本信息
        showCurrentUserData();

        // 显示当前用户的离线消息  个人聊天信息或者群组消息
        if (responsejs.contains("offlinemsg"))
        {
            vector<string> vec = responsejs["offlinemsg"];
            for (string &str : vec)
            {
                json js = json::parse(str);
                // time + [id] + name + " said: " + xxx
                if (ONE_CHAT_MSG == js["msgid"].get<int>())
                {
                    cout << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>()
                            << " said: " << js["msg"].get<string>() << endl;
                }
                else
                {
                    cout << "群消息[" << js["groupid"] << "]:" << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>()
                            << " said: " << js["msg"].get<string>() << endl;
                }
            }
        }

        g_isLoginSuccess = true;
    }
}

// 子线程 - 接收线程
void readTaskHandler(int clientfd)
{
    for (;;)
    {
        char buffer[1024] = {0};
        int len = recv(clientfd, buffer, 1024, 0);  // 阻塞了
        if (-1 == len || 0 == len)
        {
            close(clientfd);
            exit(-1);
        }

        // 接收ChatServer转发的数据，反序列化生成json数据对象
        json js = json::parse(buffer);
        int msgtype = js["msgid"].get<int>();
        if (ONE_CHAT_MSG == msgtype)
        {
            cout << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>()
                 << " said: " << js["msg"].get<string>() << endl;
            continue;
        }

        if (GROUP_CHAT_MSG == msgtype)
        {
            cout << "群消息[" << js["groupid"] << "]:" << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>()
                 << " said: " << js["msg"].get<string>() << endl;
            continue;
        }

        if (LOGIN_MSG_ACK == msgtype)
        {
            doLoginResponse(js); // 处理登录响应的业务逻辑
            sem_post(&rwsem);    // 通知主线程，登录结果处理完成
            continue;
        }

        if (REG_MSG_ACK == msgtype)
        {
            doRegResponse(js);
            sem_post(&rwsem);    // 通知主线程，注册结果处理完成
            continue;
        }

        if (VIDEO_ACK == msgtype)
        {
            sem_post(&rwsem);
            continue;
        }
    }
}

// 显示当前登录成功用户的基本信息
void showCurrentUserData()
{
    cout << "======================login user======================" << endl;
    cout << "current login user => id:" << g_currentUser.getId() << " name:" << g_currentUser.getName() << endl;
    cout << "----------------------friend list---------------------" << endl;
    if (!g_currentUserFriendList.empty())
    {
        for (User &user : g_currentUserFriendList)
        {
            cout << user.getId() << " " << user.getName() << " " << user.getState() << endl;
        }
    }
    cout << "----------------------group list----------------------" << endl;
    if (!g_currentUserGroupList.empty())
    {
        for (Group &group : g_currentUserGroupList)
        {
            cout << group.getId() << " " << group.getName() << " " << group.getDesc() << endl;
            for (GroupUser &user : group.getUsers())
            {
                cout << user.getId() << " " << user.getName() << " " << user.getState()
                     << " " << user.getRole() << endl;
            }
        }
    }
    cout << "======================================================" << endl;
}

// "help" command handler
void help(int fd = 0, string str = "");
// "chat" command handler
void chat(int, string);
// "addfriend" command handler
void addfriend(int, string);
// "creategroup" command handler
void creategroup(int, string);
// "addgroup" command handler
void addgroup(int, string);
// "groupchat" command handler
void groupchat(int, string);
// "loginout" command handler
void loginout(int, string);
void putfile(int, string);
void getfile(int, string);
void videomerge(int, string);

// 系统支持的客户端命令列表
unordered_map<string, string> commandMap = {
    {"help", "显示所有支持的命令，格式help"},
    {"chat", "一对一聊天，格式chat:friendid:message"},
    {"addfriend", "添加好友，格式addfriend:friendid"},
    {"creategroup", "创建群组，格式creategroup:groupname:groupdesc"},
    {"addgroup", "加入群组，格式addgroup:groupid"},
    {"groupchat", "群聊，格式groupchat:groupid:message"},
    {"loginout", "注销，格式loginout"},
    {"putfile", "发送本地文件，格式putfile:\"<ptype>1</ptype><clientpath>/tmp/tcp/surfdata1</clientpath><andchild>true</andchild><matchname>*.XML,*.CSV,*.JSON</matchname>\""},
    {"getfile", "从云端下载文件，格式getfile:\"<ptype>2</ptype><srvpath>/tmp/tcp/surfdata2</srvpath><andchild>true</andchild><matchname>*.XML,*.CSV,*.JSON</matchname><clientpath>/tmp/tcp/surfdata3</clientpath>\""},
    {"videomerge", "将本地的两个视频云剪切后下载到本地，格式videomerge:files/video1.mp4 files/video2.mp4 files/out.mp4 0 60"}};

// 注册系统支持的客户端命令处理
unordered_map<string, function<void(int, string)>> commandHandlerMap = {
    {"help", help},
    {"chat", chat},
    {"addfriend", addfriend},
    {"creategroup", creategroup},
    {"addgroup", addgroup},
    {"groupchat", groupchat},
    {"loginout", loginout},
    {"putfile", putfile},
    {"getfile", getfile},
    {"videomerge", videomerge}};

// 主聊天页面程序
void mainMenu(int clientfd)
{
    help();

    char buffer[1024] = {0};
    while (isMainMenuRunning)
    {
        cin.getline(buffer, 1024);
        string commandbuf(buffer);
        string command; // 存储命令
        int idx = commandbuf.find(":");
        if (-1 == idx)
        {
            command = commandbuf;
        }
        else
        {
            command = commandbuf.substr(0, idx);
        }
        auto it = commandHandlerMap.find(command);
        if (it == commandHandlerMap.end())
        {
            cerr << "invalid input command!" << endl;
            continue;
        }

        // 调用相应命令的事件处理回调，mainMenu对修改封闭，添加新功能不需要修改该函数
        it->second(clientfd, commandbuf.substr(idx + 1, commandbuf.size() - idx)); // 调用命令处理方法
    }
}

// "help" command handler
void help(int, string)
{
    cout << "show command list >>> " << endl;
    for (auto &p : commandMap)
    {
        cout << p.first << " : " << p.second << endl;
    }
    cout << endl;
}
// "addfriend" command handler
void addfriend(int clientfd, string str)
{
    int friendid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = g_currentUser.getId();
    js["friendid"] = friendid;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send addfriend msg error -> " << buffer << endl;
    }
}
// "chat" command handler
void chat(int clientfd, string str)
{
    int idx = str.find(":"); // friendid:message
    if (-1 == idx)
    {
        cerr << "chat command invalid!" << endl;
        return;
    }

    int friendid = atoi(str.substr(0, idx).c_str());
    string message = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["toid"] = friendid;
    js["msg"] = message;
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send chat msg error -> " << buffer << endl;
    }
}
// "creategroup" command handler  groupname:groupdesc
void creategroup(int clientfd, string str)
{
    int idx = str.find(":");
    if (-1 == idx)
    {
        cerr << "creategroup command invalid!" << endl;
        return;
    }

    string groupname = str.substr(0, idx);
    string groupdesc = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupname"] = groupname;
    js["groupdesc"] = groupdesc;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send creategroup msg error -> " << buffer << endl;
    }
}
// "addgroup" command handler
void addgroup(int clientfd, string str)
{
    int groupid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupid"] = groupid;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send addgroup msg error -> " << buffer << endl;
    }
}
// "groupchat" command handler   groupid:message
void groupchat(int clientfd, string str)
{
    int idx = str.find(":");
    if (-1 == idx)
    {
        cerr << "groupchat command invalid!" << endl;
        return;
    }

    int groupid = atoi(str.substr(0, idx).c_str());
    string message = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["groupid"] = groupid;
    js["msg"] = message;
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send groupchat msg error -> " << buffer << endl;
    }
}
// "loginout" command handler
void loginout(int clientfd, string)
{
    json js;
    js["msgid"] = LOGINOUT_MSG;
    js["id"] = g_currentUser.getId();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send loginout msg error -> " << buffer << endl;
    }
    else
    {
        isMainMenuRunning = false;
    }   
}

void _putfile(int clientfd, string str)
{
    string logpath = "/tmp/tcp/log/putfile.log";
    bool ret = pfile.setlogfile(logpath);
    if (ret == 0)
    {
        cout << "connect error1" << endl;
        return;
    }
    ret &= pfile.setxml(str);
    if (ret == 0)
    {
        cout << "connect error2" << endl;
        return;
    }
    ret &= pfile.login();
    if (ret == 0)
    {
        cout << "connect error3" << endl;
        return;
    }

    vector<string> vecpath;
    pfile._tcpputfiles(vecpath);

    // vecpath里面存了所有的全路径，将客户端的路径换成服务端的
    for (string &path : vecpath)
    {
        string str1 = string(pfile.starg.clientpath);
        string str2 = string(pfile.starg.srvpath);
        size_t found = path.find(str1); 
        if (found != std::string::npos) {
            path.replace(found, str1.length(), str2);   // 替换子字符串为新字符串
            // std::cout << "替换后的字符串: " << path << std::endl;
        }

        // 将改变后的path通过6000发给云端
        json js;
        js["msgid"] = FILE_PATH;
        js["id"] = id;
        js["path"] = path;
        string request = js.dump();
		cout << request << endl;
        int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
        if (len == -1)
        {
            return;
        }
    }
}

// 将str补充完整后调用类 
// 格式putfile:\"<ptype>1</ptype><clientpath>/tmp/tcp/surfdata1</clientpath><andchild>true</andchild><matchname>*.XML,*.CSV,*.JSON</matchname>\"
// 完整 "<ip>47.108.169.169</ip><port>5005</port><ptype>1</ptype><clientpath>/tmp/tcp/surfdata1</clientpath><clientpathbak>/tmp/tcp/surfdata1bak</clientpathbak><andchild>true</andchild><matchname>*.XML,*.CSV,*.JSON</matchname><srvpath>/tmp/tcp/srvpath</srvpath><timetvl>10</timetvl><timeout>50</timeout><pname>tcpputfiles_surfdata</pname>"
void putfile(int clientfd, string str)
{
    // 将str组成合格的xml
    str.pop_back();
    str += "<ip>47.108.169.169</ip><port>5005</port><clientpathbak>/tmp/tcp/surfdata1bak</clientpathbak><srvpath>/tmp/tcp/surfdata2</srvpath><timetvl>10</timetvl><timeout>50</timeout><pname>tcpputfiles_surfdata</pname>\"";

    _putfile(clientfd, str);
}

void _getfile(int clientfd, string str)
{
    string logpath = "/tmp/tcp/log/getfile.log";
    bool ret = gfile.setlogfile(logpath);
    if (ret == 0)
    {
        cout << "connect error1" << endl;
        return;
    }
    ret &= gfile.setxml(str);
    if (ret == 0)
    {
        cout << "connect error2" << endl;
        return;
    }
    ret &= gfile.login();
    if (ret == 0)
    {
        cout << "connect error3" << endl;
        return;
    }
    gfile._tcpgetfiles();
}

// 格式getfile:\"<ptype>2</ptype><srvpath>/tmp/tcp/surfdata2</srvpath><andchild>true</andchild><matchname>*.XML,*.CSV,*.JSON</matchname><clientpath>/tmp/tcp/surfdata3</clientpath>\"
// 完整: \"<ip>192.168.174.132</ip><port>5005</port><ptype>2</ptype><srvpath>/tmp/tcp/surfdata2</srvpath><srvpathbak>/tmp/tcp/surfdata2bak</srvpathbak><andchild>true</andchild><matchname>*.XML,*.CSV,*.JSON</matchname><clientpath>/tmp/tcp/surfdata3</clientpath><timetvl>10</timetvl><timeout>50</timeout><pname>tcpgetfiles_surfdata</pname>\"
void getfile(int clientfd, string str)
{
    str.pop_back();
    str += "<ip>47.108.169.169</ip><port>5005</port><clientpathbak>/tmp/tcp/surfdata1bak</clientpathbak><srvpathbak>/tmp/tcp/surfdata2bak</srvpathbak><timetvl>10</timetvl><timeout>50</timeout><pname>tcpputfiles_surfdata</pname>\"";
    _getfile(clientfd, str);
}

// 分割字符串函数
std::vector<std::string> splitString(const std::string& input, char delimiter) {
    std::vector<std::string> result;
    std::stringstream ss(input);
    std::string item;

    while (std::getline(ss, item, delimiter)) {
        result.push_back(item);
    }

    return result;
}

std::pair<std::string, std::string> splitLastPathComponent(const std::string& path, char delimiter) {
    size_t lastDelimiterPos = path.find_last_of(delimiter);
    if (lastDelimiterPos == std::string::npos) {
        return std::make_pair("", path);  // 没有找到分隔符，返回空字符串和原始路径
    }
    std::string directory = path.substr(0, lastDelimiterPos);
    std::string filename = path.substr(lastDelimiterPos + 1);
    return std::make_pair(directory, filename);
}

// videomerge:files/video1.mp4 files/video2.mp4 files/out.mp4 0 60
// 将本地的video1和video2传到云端，并剪切成0~60秒的，然后下载到本地
void videomerge(int clientfd, string input)
{
    char delimiter = '/';
    // 0、将str的内容解析
    std::vector<std::string> split = splitString(input, ' ');
    if (split.size() != 5) return;
    std::string str1 = split[0]; // files/video1.mp4
    std::string str2 = split[1]; // files/video2.mp4
    std::string str3 = split[2]; // files/out.mp4
    int start_time, end_time;
    start_time = std::stoi(split[3]); // 0
    end_time = std::stoi(split[4]); // 60

    // 1、调用两次putfile将本地的传到云端
    // clientpath    matchname
    // <ip>47.108.169.169</ip><port>5005</port><ptype>1</ptype><clientpath>/tmp/tcp/surfdata1</clientpath><clientpathbak>/tmp/tcp/surfdata1bak</clientpathbak><andchild>true</andchild><matchname>*.XML,*.CSV,*.JSON</matchname><srvpath>/tmp/tcp/srvpath</srvpath><timetvl>10</timetvl><timeout>50</timeout><pname>tcpputfiles_surfdata</pname>"
    string xml_base = "\"<ip>47.108.169.169</ip><port>5005</port><ptype>1</ptype><clientpathbak>/tmp/tcp/surfdata1bak</clientpathbak><andchild>false</andchild><srvpath>/tmp/tcp/videopath</srvpath><timetvl>10</timetvl><timeout>50</timeout><pname>video_surfdata</pname>\"";
    // 将str1分成文件夹目录和具体的文件
    std::pair<std::string, std::string> splitted = splitLastPathComponent(str1, delimiter);
    std::string directory1 = splitted.first;
    std::string filename1 = splitted.second;

    // 将str2分成文件夹目录和具体的文件
    splitted = splitLastPathComponent(str2, delimiter);
    std::string directory2 = splitted.first;
    std::string filename2 = splitted.second;

    splitted = splitLastPathComponent(str3, delimiter);
    std::string directory3 = splitted.first;
    std::string filename3 = splitted.second;

    // 组装第一个xml
    string xml1 = xml_base;
    xml1.pop_back();
    xml1 += "<clientpath>" + directory1 + "</clientpath><matchname>" + filename1 + "</matchname>\"";

    // 组装第二个xml
    string xml2 = xml_base;
    xml2.pop_back();
    xml2 += "<clientpath>" + directory2 + "</clientpath><matchname>" + filename2 + "</matchname>\"";

    // 发送第一个文件
    _putfile(clientfd, xml1);

    // 发送第二个文件
    // sleep(1); // 对端文件系统断连，将fileserver改成fork解决
    _putfile(clientfd, xml2);

    // 2、通过6000端口将要剪切的信息发给云端，并接收云端的ok报文
    // 将文件名改成服务器上的
    string path1 = "/tmp/tcp/videopath" + string("/") + filename1;
    string path2 = "/tmp/tcp/videopath" + string("/") + filename2;

    json js;
    js["msgid"] = VIDEO;
    js["path1"] = path1;
    js["path2"] = path2;
    js["targetfile"] = str3;
    js["start"] = start_time;
    js["end"] = end_time;
    string request = js.dump();
    int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
    if (len == -1) return;

    sem_wait(&rwsem); // 等待信号量，待云端ok后下载文件

    // 3、调用getfile将文件下载到本地
//     string path3 = "/tmp/tcp/videopath" + string("/") + str3;
// cout << path3 << endl;
    // 组装下载请求的xml
    // \"<ip>47.108.169.169</ip><port>5005</port><ptype>2</ptype><srvpath>/tmp/tcp/surfdata2</srvpath><srvpathbak>/tmp/tcp/surfdata2bak</srvpathbak><andchild>true</andchild><matchname>*.XML,*.CSV,*.JSON</matchname><clientpath>/tmp/tcp/surfdata3</clientpath><timetvl>10</timetvl><timeout>50</timeout><pname>tcpgetfiles_surfdata</pname>\"
    string get_xml = "\"<ip>47.108.169.169</ip><port>5005</port><ptype>2</ptype><srvpath>/tmp/tcp/videopath/files</srvpath><srvpathbak>/tmp/tcp/surfdata2bak</srvpathbak><andchild>false</andchild><clientpath>/home/wu/cpp/files</clientpath><timetvl>10</timetvl><timeout>50</timeout><pname>getvideo_surfdata</pname>\"";
    get_xml.pop_back();
    get_xml += string("<matchname>") + filename3 + string("</matchname>\"");
    cout << get_xml << endl;
    _getfile(clientfd, get_xml);
}

// 获取系统时间（聊天信息需要添加时间信息）
string getCurrentTime()
{
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm *ptm = localtime(&tt);
    char date[60] = {0};
    sprintf(date, "%d-%02d-%02d %02d:%02d:%02d",
            (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
            (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
    return std::string(date);
}
