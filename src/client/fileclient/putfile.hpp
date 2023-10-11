#ifndef PUTFILE
#define PUTFILE
#include "_public.h"

class putfile
{
public:
    putfile(); // log文件 待解析的xml
    ~putfile();

    bool setlogfile(string strlogfile);
    bool setxml(string strxml);
    
    bool login();

    void EXIT(int sig);

    // 把xml解析到参数starg结构中。
    bool _xmltoarg(const char *strxmlbuffer);

    bool Login(const char *argv);    // 登录业务。

    // 文件上传的主函数，执行一次文件上传的任务。
    bool _tcpputfiles(vector<string>&path);
    bool bcontinue=true;   // 如果调用_tcpputfiles发送了文件，bcontinue为true，初始化为true。

    // 把文件的内容发送给对端。
    bool SendFile(const int sockfd,const char *filename,const int filesize);

    struct st_arg
    {
        int  clienttype;          // 客户端类型，1-上传文件；2-下载文件。
        char ip[31];              // 服务端的IP地址。
        int  port;                // 服务端的端口。
        int  ptype;               // 文件上传成功后本地文件的处理方式：1-删除文件；2-移动到备份目录。
        char clientpath[301];     // 本地文件存放的根目录。
        char clientpathbak[301];  // 文件成功上传后，本地文件备份的根目录，当ptype==2时有效。
        bool andchild;            // 是否上传clientpath目录下各级子目录的文件，true-是；false-否。
        char matchname[301];      // 待上传文件名的匹配规则，如"*.TXT,*.XML"。
        char srvpath[301];        // 服务端文件存放的根目录。
        int  timetvl;             // 扫描本地目录文件的时间间隔，单位：秒。
        int  timeout;             // 进程心跳的超时时间。
        char pname[51];           // 进程名，建议用"tcpputfiles_后缀"的方式。
    }; 
    struct st_arg starg;  // 程序运行的参数结构体。

private:
    char strrecvbuffer[1024];   // 发送报文的buffer。
    char strsendbuffer[1024];   // 接收报文的buffer。
    CLogFile logfile;
    string strxml;
    CTcpClient TcpClient;
    CPActive PActive;  // 进程心跳。
};

#endif