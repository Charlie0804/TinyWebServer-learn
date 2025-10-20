#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H

#include <mysql/mysql.h>
#include <semaphore.h>

#include <mutex>
#include <queue>
#include <string>
#include <thread>

#include "../log/log.h"

class SqlConnPool
{
   public:
    static* GetConn();

    MYSQL* Getconn();
    void FreeConn(MYSQL* conn);

    void Init(const char* host, int port, const char* user, const char* pwd, const char* dbName, int connSize);

    void ClosePool();

   private:
    SqlConnPool() = default;
    ~SqlConnPool() { ClosePool(); }

    int MAX_CONN_;

    std::queue<MYSQL*> connQue_;
    std::mutex mtx_;
    sem_t semId;
};

// 资源在对象构造初始化，资源在对象析构时释放
class SqlConnRAII
{
   public:
    SqlConnRAII(MYSQL** sql, SqlConnPool* connpool)
    {
        assert(connpool);
        *sql = connpool->GetConn();
        sql_ = *sql;
        connpool_ = connpool;
    }

    ~SqlConnRAII()
    {
        if (sql_)
        {
            connpool_->FreeConn(sql_);
        }
    }

   private:
    MYSQL* sql_;
    SqlConnPool* connpool_;
};

#endif  // SQLCONNPOOL_H
