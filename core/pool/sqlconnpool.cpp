#include "sqlconnpool.h"

SqlConnPool* SqlConnPool::Instance()
{
    static SqlConnPool pool;
    return &pool;
}

// ��ʼ��
void SqlConnPool::Init(const char* host, int part, const char* user, const char* pwd, const char* dbName,
                       int connSize = 10)
{
    assert(connSize > 0);
    for (int i = 0; i < connSize; ++i)
    {
        MYSQL* conn = nullptr;
        // ��ʼ��һ��MySQL���Ӷ���
        conn = mysql_init(conn);
        if (!conn)
        {
            LOG_ERROR("mysql_init error");
            assert(conn);
        }
        // ������MySQL��������ʵ������
        conn = mysql_real_connect(conn, host, user, pwd, dbName, 0, nullptr, 0);
        if (!conn)
        {
            LOG_ERROR("mysql_real_connect error");
        }
        connQue_.emplace(conn);
    }
    MAX_CONN_ = connSize;
    // ��ʼ��POSIX�ź���
    // (�ź��������ĵ�ַ��0����ʾ�ź������̳̼߳乲���ź����ĳ�ʼֵ)
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
    // ��ȡ�ź�����P������
    // ����ź���ֵ > 0��������1������
    // ����ź���ֵ = 0�������ȴ�ֱ���ź���ֵ > 0
    sem_wait(&semId_);  // -1

    // ��������������
    lock_guard<mutex> locker(mtx_);

    // ��������
    conn = connQue_.front();
    connQue_.pop();
    return conn;
}

void SqlConnPool::FreeConn(MYSQL* conn)
{
    assert(conn);
    lock_guard<mutex> locker(mtx_);
    connQue_.push(conn);
    // �ͷ��ź�����V������
    // �ź���ֵ +1
    sem_post(&semId_);  // +1
}

void SqlConnPool::ClosePool()
{
    lock_guard<mutex> locker(mtx_);
    while (!connQue_.empty())
    {
        auto conn = connQue_.front();
        connQue_.pop();
        // �ر����ݿ����Ӳ��ͷ���Դ
        mysql_close(conn);
    }
    // ע������˳���ȹر������������
    // ����MySQL�ͻ��˿�ʹ�õ�ȫ����Դ
    mysql_library_end();
}

int SqlConnPool::GetFreeConnCount()
{
    lock_guard<mutex> locker(mtx_);
    return connQue_.size();
}
