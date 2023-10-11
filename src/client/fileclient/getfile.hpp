#ifndef GETFILE
#define GETFILE

#include "_public.h"

class getfile
{
public:
    getfile();
    ~getfile();

    bool setlogfile(string strlogfile);
    bool setxml(string strxml);

    bool login();

    // 程序退出和信号2、15的处理函数。
    void EXIT(int sig);

    // 把xml解析到参数starg结构中。
    bool _xmltoarg(const char *strxmlbuffer);

    bool Login(const char *argv);    // 登录业务。

    // 文件下载的主函数。
    void _tcpgetfiles();

    // 接收文件的内容。
    bool RecvFile(const int sockfd,const char *filename,const char *mtime,int filesize);

private:
    CLogFile logfile;
    char strrecvbuffer[1024];   // 发送报文的buffer。
    char strsendbuffer[1024];   // 接收报文的buffer。
    string strxml;
    CTcpClient TcpClient;
    CPActive PActive;  // 进程心跳。

    int ok_times;

    // 程序运行的参数结构体。
    struct st_arg
    {
        int  clienttype;          // 客户端类型，1-上传文件；2-下载文件。
        char ip[31];              // 服务端的IP地址。
        int  port;                // 服务端的端口。
        int  ptype;               // 文件下载成功后服务端文件的处理方式：1-删除文件；2-移动到备份目录。
        char srvpath[301];        // 服务端文件存放的根目录。
        char srvpathbak[301];     // 文件成功下载后，服务端文件备份的根目录，当ptype==2时有效。
        bool andchild;            // 是否下载srvpath目录下各级子目录的文件，true-是；false-否。
        char matchname[301];      // 待下载文件名的匹配规则，如"*.TXT,*.XML"。
        char clientpath[301];     // 客户端文件存放的根目录。
        int  timetvl;             // 扫描服务端目录文件的时间间隔，单位：秒。
        int  timeout;             // 进程心跳的超时时间。
        char pname[51];           // 进程名，建议用"tcpgetfiles_后缀"的方式。
    };
    struct st_arg starg;
};


#endif