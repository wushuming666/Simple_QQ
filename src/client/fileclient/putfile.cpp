#include "putfile.hpp"
#include "json.hpp"
using json = nlohmann::json;

putfile::putfile()
{

}

bool putfile::setlogfile(string strlogfile)
{
    if (logfile.Open(strlogfile.c_str(), "a+") == false)
    {
        return false;
    }
    return true;
}
bool putfile::setxml(string strxml)
{
    this->strxml = strxml; 
    if (_xmltoarg(strxml.c_str())==false) return false;
    return true;
}
bool putfile::login()
{
    PActive.AddPInfo(starg.timeout,starg.pname);  // 把进程的心跳信息写入共享内存。

    // 向服务端发起连接请求。
    if (TcpClient.ConnectToServer(starg.ip,starg.port)==false)
    {
        logfile.Write("TcpClient.ConnectToServer(%s,%d) failed.\n",starg.ip,starg.port); EXIT(-1);
    }

    // 登录业务。告诉云端是上传还是下载
    if (Login(strxml.c_str())==false) { logfile.Write("Login() failed.\n"); EXIT(-1); }

    return true;
}

putfile::~putfile()
{
    logfile.Close();
    TcpClient.Close();
}

// 登录业务。 
bool putfile::Login(const char *argv)    
{
    memset(strsendbuffer,0,sizeof(strsendbuffer));
    memset(strrecvbuffer,0,sizeof(strrecvbuffer));
    
    SPRINTF(strsendbuffer,sizeof(strsendbuffer),"%s<clienttype>1</clienttype>",argv);
    logfile.Write("发送：%s\n",strsendbuffer);
    if (TcpClient.Write(strsendbuffer)==false) return false; // 向服务端发送请求报文。

    if (TcpClient.Read(strrecvbuffer,20)==false) return false; // 接收服务端的回应报文。
    logfile.Write("接收：%s\n",strrecvbuffer);

    logfile.Write("登录(%s:%d)成功。\n",starg.ip,starg.port); 

    return true;
}

void putfile::EXIT(int sig)
{
    logfile.Write("程序退出，sig=%d\n\n",sig);

    exit(0);
}

bool putfile::_xmltoarg(const char *strxmlbuffer)
{
    memset(&starg,0,sizeof(struct st_arg));

    GetXMLBuffer(strxmlbuffer,"ip",starg.ip);
    if (strlen(starg.ip)==0) { logfile.Write("ip is null.\n"); return false; }

    GetXMLBuffer(strxmlbuffer,"port",&starg.port);
    if ( starg.port==0) { logfile.Write("port is null.\n"); return false; }

    GetXMLBuffer(strxmlbuffer,"ptype",&starg.ptype);
    if ((starg.ptype!=1)&&(starg.ptype!=2)) { logfile.Write("ptype not in (1,2).\n"); return false; }

    GetXMLBuffer(strxmlbuffer,"clientpath",starg.clientpath);
    if (strlen(starg.clientpath)==0) { logfile.Write("clientpath is null.\n"); return false; }

    GetXMLBuffer(strxmlbuffer,"clientpathbak",starg.clientpathbak);
    if ((starg.ptype==2)&&(strlen(starg.clientpathbak)==0)) { logfile.Write("clientpathbak is null.\n"); return false; }

    GetXMLBuffer(strxmlbuffer,"andchild",&starg.andchild);

    GetXMLBuffer(strxmlbuffer,"matchname",starg.matchname);
    if (strlen(starg.matchname)==0) { logfile.Write("matchname is null.\n"); return false; }

    GetXMLBuffer(strxmlbuffer,"srvpath",starg.srvpath);
    if (strlen(starg.srvpath)==0) { logfile.Write("srvpath is null.\n"); return false; }

    GetXMLBuffer(strxmlbuffer,"timetvl",&starg.timetvl);
    if (starg.timetvl==0) { logfile.Write("timetvl is null.\n"); return false; }

    // 扫描本地目录文件的时间间隔，单位：秒。
    // starg.timetvl没有必要超过30秒。
    if (starg.timetvl>30) starg.timetvl=30;

    // 进程心跳的超时时间，一定要大于starg.timetvl，没有必要小于50秒。
    GetXMLBuffer(strxmlbuffer,"timeout",&starg.timeout);
    if (starg.timeout==0) { logfile.Write("timeout is null.\n"); return false; }
    if (starg.timeout<50) starg.timeout=50;

    GetXMLBuffer(strxmlbuffer,"pname",starg.pname,50);
    if (strlen(starg.pname)==0) { logfile.Write("pname is null.\n"); return false; }

    return true;
}

// 文件上传的主函数，执行一次文件上传的任务。
bool putfile::_tcpputfiles(vector<string>&vecpath)
{
    //   logfile.Write("enter _tcpputfiles\n");
    CDir Dir;
    bcontinue=false;
    // 调用OpenDir()打开starg.clientpath目录。
    if (Dir.OpenDir(starg.clientpath,starg.matchname,10000,starg.andchild)==false)
    {
        logfile.Write("Dir.OpenDir(%s) 失败。\n",starg.clientpath); return false;
    }

    int delayed=0;        // 未收到对端确认报文的文件数量。
    int buflen=0;         // 用于存放strrecvbuffer的长度。
    string path;

    while (true)
    {
        memset(strsendbuffer,0,sizeof(strsendbuffer));
        memset(strrecvbuffer,0,sizeof(strrecvbuffer));
        path = "";

        // 遍历目录中的每个文件，调用ReadDir()获取一个文件名。
        if (Dir.ReadDir()==false) break;

        bcontinue=true;
        // logfile.Write("_tcpputfiles while(true)\n");
        logfile.Write("<filename>%s\n", Dir.m_FullFileName);

        // 将全路径组装起来
        path = string(Dir.m_FullFileName);
        vecpath.push_back(path);

        // 把文件名、修改时间、文件大小组成报文，发送给对端。
        SNPRINTF(strsendbuffer,sizeof(strsendbuffer),1000,"<filename>%s</filename><mtime>%s</mtime><size>%d</size>",Dir.m_FullFileName,Dir.m_ModifyTime,Dir.m_FileSize);

        // logfile.Write("strsendbuffer=%s\n",strsendbuffer);
        if (TcpClient.Write(strsendbuffer)==false)
        {
            logfile.Write("TcpClient.Write() failed.\n"); return false;
        }

        // 把文件的内容发送给对端。
        logfile.Write("send %s(%d) ...",Dir.m_FullFileName,Dir.m_FileSize);
        if (SendFile(TcpClient.m_connfd,Dir.m_FullFileName,Dir.m_FileSize)==true)
        {
            logfile.WriteEx("ok.\n"); delayed++;
        }
        else
        {
            logfile.WriteEx("failed.\n"); TcpClient.Close(); return false;
        }

        PActive.UptATime();

        // 接收对端的确认报文。
        while (delayed>0)
        {
            memset(strrecvbuffer,0,sizeof(strrecvbuffer));
            if (TcpRead(TcpClient.m_connfd,strrecvbuffer,&buflen,-1)==false) break;
            logfile.Write("strrecvbuffer=%s\n",strrecvbuffer);

            // 删除或者转存本地的文件。
            delayed--;
        }
    }

    // 继续接收对端的确认报文。
    while (delayed>0)
    {
        memset(strrecvbuffer,0,sizeof(strrecvbuffer));
        if (TcpRead(TcpClient.m_connfd,strrecvbuffer,&buflen,10)==false) break;
        logfile.Write("strrecvbuffer=%s\n",strrecvbuffer);

        // 删除或者转存本地的文件。
        delayed--;
    }

    //   logfile.Write("bcontinue = %d\n", bcontinue);
    return true;
}

// 把文件的内容发送给对端。
bool putfile::SendFile(const int sockfd,const char *filename,const int filesize)
{
    int  onread=0;        // 每次调用fread时打算读取的字节数。 
    int  bytes=0;         // 调用一次fread从文件中读取的字节数。
    char buffer[1000];    // 存放读取数据的buffer。
    int  totalbytes=0;    // 从文件中已读取的字节总数。
    FILE *fp=NULL;

    // 以"rb"的模式打开文件。
    if ( (fp=fopen(filename,"rb"))==NULL )  return false;

    while (true)
    {
        memset(buffer,0,sizeof(buffer));

        // 计算本次应该读取的字节数，如果剩余的数据超过1000字节，就打算读1000字节。
        if (filesize-totalbytes>1000) onread=1000;
        else onread=filesize-totalbytes;

        // 从文件中读取数据。
        bytes=fread(buffer,1,onread,fp);

        // 把读取到的数据发送给对端。
        if (bytes>0)
        {
        if (Writen(sockfd,buffer,bytes)==false) { fclose(fp); return false; }
        }

        // 计算文件已读取的字节总数，如果文件已读完，跳出循环。
        totalbytes=totalbytes+bytes;

        if (totalbytes==filesize) break;
    }

    fclose(fp);

    return true;
}




