#include "usermodel.hpp"
#include "connectionpool.hpp"
#include <iostream>
#include <unistd.h>

using namespace std;

// User表的增加方法
bool UserModel::insert(User &user)
{
    // 1 组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into User(name, password, state) values('%s', '%s', '%s')",
            user.getName().c_str(), user.getPwd().c_str(), user.getState().c_str());

    ConnectionPool* cp = ConnectionPool::getConnectionPool();
    shared_ptr<MySQL>mysql = cp->getConnection();
    if (mysql->update(sql))
    {
        // 获取插入成功的用户数据生成的主键id
        user.setId(mysql_insert_id(mysql->getConnection()));
        return true;
    }
    

    return false;
}

// 根据用户号码查询用户信息
User UserModel::query(int id)
{

// FILE *file;
// const char *filename = "/project/wsm/server/server.log";    
// file = fopen(filename, "a");

// fprintf(file, "pid = %d  enter query\n", getpid());

    char sql[1024] = {0};
    sprintf(sql, "select * from User where id = %d", id);

    ConnectionPool* cp = ConnectionPool::getConnectionPool();

    shared_ptr<MySQL>mysql = cp->getConnection();

    MYSQL_RES *res = mysql->query(sql);

// if (res == nullptr)
// {
//     fprintf(file, "pid = %d  res == nullptr", getpid());
// }

    if (res != nullptr)
    {
        MYSQL_ROW row = mysql_fetch_row(res);
        if (row != nullptr)
        {
            User user;
            user.setId(atoi(row[0]));
            user.setName(row[1]);
            user.setPwd(row[2]);
            user.setState(row[3]);

// fprintf(file, "query pid = %d, id = %d, name = %s, pwd = %s, state = %s\n", getpid(), atoi(row[0]), row[1], row[2], row[3]);
// fprintf(file, "query pid = %d, id = %d, name = %s \n", getpid(), user.getId(), user.getName().c_str());



            mysql_free_result(res);
            return user;
        }
    }
// fclose(file);
    return User();
}

// 更新用户的状态信息
bool UserModel::updateState(User user)
{
    // 1 组装sql语句
    char sql[1024] = {0};

    sprintf(sql, "update User set state = '%s' where id = %d",
            user.getState().c_str(), user.getId());

    ConnectionPool* cp = ConnectionPool::getConnectionPool();
    shared_ptr<MySQL>mysql = cp->getConnection();
    if (mysql->update(sql))
    {
        return true;
    }

    return false;
}

// 重置用户的状态信息
void UserModel::resetState()
{
    // 1 组装sql语句
    char sql[1024] = {0};

    sprintf(sql, "update User set state = 'offline' where state = 'online'");

    ConnectionPool* cp = ConnectionPool::getConnectionPool();
    shared_ptr<MySQL>mysql = cp->getConnection();
    mysql->update(sql);
}