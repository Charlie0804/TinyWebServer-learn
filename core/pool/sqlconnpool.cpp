#include "sqlconnpool.h"

SqlConnPool* SqlConnPool::Instance()
{
    static SqlConnPool pool;
    return &pool;
}

// 初始化
void SqlConnPool::Init(const char* host, int part, const char* user, const char* pwd, const char* dbName,
                       int connSize = 10)
{
    assert(connSize > 0);
    for (int i = 0; i < connSize; ++i)
    {
        MYSQL* conn = nullptr;
        // 初始化一个MySQL连接对象
        conn = mysql_init(conn);
        if (!conn)
        {
            LOG_ERROR("mysql_init error");
            assert(conn);
        }
        // 建立到MySQL服务器的实际连接
        conn = mysql_real_connect(conn, host, user, pwd, dbName, 0, nullptr, 0);
        if (!conn)
        {
            LOG_ERROR("mysql_real_connect error");
        }
        connQue_.emplace(conn);
    }
    MAX_CONN_ = connSize;
    // 初始化POSIX信号量
    // (信号量变量的地址，0：表示信号量在线程程间共享，信号量的初始值)
    sem_init(&semId_, 0, MAX_CONN_);
}

MYSQL* SqlConnPool::GetConn()
{
    MYSQL* conn = nullptr;
    if (connQue_.empty())
    {
        LOG_WARN("SqlConnPool busy!");
        return nullptr;
    }
    // 获取信号量（P操作）
    // 如果信号量值 > 0，立即减1并返回
    // 如果信号量值 = 0，阻塞等待直到信号量值 > 0
    sem_wait(&semId_);  // -1

    // 互斥锁保护队列
    lock_guard<mutex> locker(mtx_);

    // 操作队列
    conn = connQue_.front();
    connQue_.pop();
    return conn;
}

void SqlConnPool::FreeConn(MYSQL* conn)
{
    assert(conn);
    lock_guard<mutex> locker(mtx_);
    connQue_.push(conn);
    // 释放信号量（V操作）
    // 信号量值 +1
    sem_post(&semId_);  // +1
}

void SqlConnPool::ClosePool()
{
    lock_guard<mutex> locker(mtx_);
    while (!connQue_.empty())
    {
        auto conn = connQue_.front();
        connQue_.pop();
        // 关闭数据库连接并释放资源
        mysql_close(conn);
    }
    // 注意清理顺序，先关闭连接再清理库
    // 清理MySQL客户端库使用的全局资源
    mysql_library_end();
}

int SqlConnPool::GetFreeConnCount()
{
    lock_guard<mutex> locker(mtx_);
    return connQue_.size();
}
