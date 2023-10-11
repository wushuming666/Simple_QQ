#include "filepathmodel.hpp"
#include "connectionpool.hpp"
#include <iostream>
#include <unistd.h>

using namespace std;

bool Filepathmodel::insert(Filepath &fpath)
{
    // 1 组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into UserFile(id, filePath) values('%d', '%s')",
            fpath.getId(), fpath.getPath().c_str());

    ConnectionPool* cp = ConnectionPool::getConnectionPool();
    shared_ptr<MySQL>mysql = cp->getConnection();
    if (mysql->update(sql))
    {   
        // 获取插入成功的用户数据生成的主键id
        // fpath.setId(mysql_insert_id(mysql->getConnection()));
        return true;
    }   
    
    return false;
}

