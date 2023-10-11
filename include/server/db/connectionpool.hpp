// #pragma once
/*
实现连接池模块
*/

#include <string>
#include <queue>
#include <mutex>
#include <atomic>
#include <thread>
#include <memory>
#include <functional>
#include <condition_variable>
using namespace std;
#include "db.hpp"

//线程安全的单例模式
class ConnectionPool
{
public:
	//获取连接池对象实例
	static ConnectionPool* getConnectionPool();
	//给外部提供接口, 从连接池中获取一个可用的空闲连接
	//用智能指针管理外部的释放工作
	shared_ptr<MySQL> getConnection();
private:
	//单例#1 构造函数私有化
	ConnectionPool();
	//从配置文件中加载配置项
	bool loadConfigFile();
	//运行在独立的线程中,专门负责生产新连接A
	void produceConnectionTask();
	//扫描超过maxIdleTime时间的空闲连接, 进行对应的连接回收
	void scannerConnectionTask();

	string _ip;				//mysql的ip地址
	unsigned short _port;	//mysql的端口号 3306
	string _username;		//mysql登录用户名
	string _dbname;			//mysql的名称
	string _passward;		//登录密码
	int _initSize;			//连接池的初始连接量
	int _maxSize;			//连接池的最大连接量
	int _maxIdleTime;		//连接池最大空闲时间
	int _connectionTimeOut; //连接池获取连接的超时时间

	queue<MySQL*> _connectionQue; //存储mysql连接的队列
	mutex _queueMutex;		//维护连接队列的线程互斥安全锁
	atomic_int _connectionCnt; //记录所创建的connection连接的总数量
	condition_variable cv;	//设置条件变量, 用于连接生产者线程和消费者线程的通信
};