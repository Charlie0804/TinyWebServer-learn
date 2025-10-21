#include "httprequest.h"
using namespace std;

// 默认处理的HTML界面
const unordered_set<string> HttpRequest::DEFAULT_HTML{
    "/index", "/register", "/login", "/welcome", "/video", "/picture",
};

// 页面标签映射
const unordered_map<string, int> HttpRequest::DEFAULT_HTML_TAG{
    // 注册页面，登录页面
    {"/register.html", 0},
    {"/login,html", 1},
};

// 初始化函数
void HttpRequest::Init()
{
    // 清空字符串
    method_ = path_ = version_ = body_ = "";
    // 重置状态机
    state_ = REQUEST_LINE;
    header_.clear();
    post_.clear();
}

//  保活查询
bool HttpRequest::IsKeepAlive() const
{
    if (header_.count("Connection") == 1)
    {
        return header_.find("Connection")->second == "keep-alive" && version_ == "1.1";
    }
    return false;
}

// 解析入口
bool HttpRequest::parse(Buffer* buff)
{
    // HTTP行分隔符
    const char CRLF[] = "\r\n";
    if (buff->ReadableBytes() <= 0)
    {
        return false;
    }

    while (buff->ReadableBytes() && state_ != FINISH)
    {
        // 1. 查找行结束位置
        const char* lineEnd = search(buff.Peak(), buff.BeginWriteConst(), CRLF, CRLF + 2);
        std::string line(buff.Peak(), lineEnd);

        // 2. 根据状态机处理不同部分
        switch (state_)
        {
            case REQUEST_LINE:
                if (!ParseRequestLine(line))
                {
                    return false;  // 解析请求行
                }
                ParsePath();  // 处理路径
                break;
            case HEADERS:
                ParseHeader_(line);  // 解析请求头
                if (buff.ReadableBytes() <= 2)
                {
                    state_ = FINISH;  // 无请求体
                }
                break;
            case BODY:
                ParseBody_(line);  // 解析请求体
                break;
            default:
                break;
        }
        if (lineEnd == buff.BeginWrite())
        {
            break;
        }

        // 3. 移动缓冲区读取位置
        buff.RetrieveUntil(lineEnd + 2);
    }
    LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());
    return true;
}

// 解析路径
void HttpRequest::ParsePath_()
{
    if (path_ == "/")
    {
        path_ = "/index.html";  // 默认首页
    }
    else
    {
        // 处理默认HTML页面
        for (auto& item : DEFAULT_HTML)
        {
            if (path_ == item)
            {
                path_ += ".html";
                break;
            }
        }
    }
}

bool HttpRequest::ParseRequestLine_(const string& line)
{
    regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    smatch subMatch;
    if (regex_match(line, subMatch, patten))
    {
        method_ = subMatch[1];
        path_ = subMatch[2];
        version_ = subMatch[3];
        state_ = HEADERS;
        return true;
    }
    LOG_ERROR("RequestLine Error");
    return false;
}

void HttpRequest::ParseHeader_(const string& line)
{
    regex patten("^([^:]*): ?(.*)$");
    smatch subMatch;
    if (regex_match(line, subMatch, patten))
    {
        header_[subMatch[1]] = subMatch[2];
    }
    else
    {
        state_ = BODY;
    }
}

void HttpRequest::ParseBody_(const string& line)
{
    body_ = line;
    ParseBody_();
    state_ = FINISH;
    LOG_DEBUG("Body:%s, len:%d", line.c_str(), line.size());
}

int HttpRequest::ConverHex(char ch)
{
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    return ch - '0';
}

void HttpRequest::ParsePost_()
{
    if (method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded")
    {
        ParseFromUrlencoded_();
        if (DEFAULT_HTML_TAG.count(path_))
        {
            int tag = DEFAULT_HTML_TAG.find(path_)->second;
            LOG_DEBUG("Tag:%d", tag);
            if (tag == 0 || tag == 1)
            {
                bool isLogin = (tag == 1);
                if (tag == 0 || tag == 1)
                {
                    bool isLogin = (tag == 1);
                    if (UserVerify(post_["username"], post_["passward"], isLogin))
                    {
                        path_ = "/welcome.html";
                    }
                    else
                    {
                        path_ = "/error.html";
                    }
                }
            }
        }
    }
}

void HttpRequest::ParseFromUrlencoded_()
{
    if (body_.size() == 0)
    {
        return;
    }

    string key, value;
    int num = 0;
    int n = body_.size();
    int i = 0, j = 0;

    for (; i < n; i++)
    {
        char ch = body_[i];
        switch (ch)
        {
            // key
            case '=':
                key = body_.substr(j, i - j);
                j = i + 1;
                break;
            // 键值对中的空格换为+或者%20
            case '+':
                body_[i] = ' ';
                break;
            case '%':
                num = ConverHex(body_[i + 1]) * 16 + ConverHex(body_[i + 2]);
                body_[i + 2] = num % 10 + '0';
                body_[i + 1] = num / 10 + '0';
                i += 2;
                break;
            // 键值对连接符
            case '&':
                value = body_.substr(j, i - j);
                j = i + 1;
                post_[key] = value;
                LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
                break;
            default:
                break;
        }
    }
    assert(j <= i);
    if (post_.count(key) == 0 && j < i)
    {
        value = body_.substr(j, i - j);
        post_[key] = value;
    }
}

bool HttpRequest::UserVerify(const string& name, const string& pwd, bool isLogin)
{
    if (name == "" || pwd == "")
    {
        return false;
    }
    LOG_INFO("Verify name:%s pwd:%s", name.c_str(), pwd.c_str());
    MYSQL* sql;
    SqlConnRAII(&sql, SqlConnPool::Instance());
    assert(sql);

    bool flag = false;
    unsigned int j = 0;
    char order[256] = {0};
    MYSQL_FIELD* fields = nullptr;
    MYSQL_RES* res = nullptr;

    if (!isLogin)
    {
        flag = true;
    }
    /* 查询用户及密码 */
    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());
    LOG_DEBUG("%s", order);

    if (mysql_query(sql, order))
    {
        // 查询失败，无需释放结果集
        LOG_ERROR("Query failed: %s", mysql_error(sql));
        return false;
    }
    // 获取查询结果集
    res = mysql_store_result(sql);
    // 获取结果集中的字段数
    j = mysql_num_fields(res);
    // 获取结果集的字段信息（元数据）
    fields = mysql_fetch_fields(res);

    // 从结果集中获取下一行数据
    while (MYSQL_ROW row = mysql_fetch_row(res))
    {
        LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);
        string password(row[1]);
        /* 注册行为 且 用户名未被使用*/
        if (isLogin)
        {
            if (pwd == password)
            {
                flag = true;
            }
            else
            {
                flag = false;
                LOG_INFO("pwd error!");
            }
        }
        else
        {
            flag = false;
            LOG_INFO("user used!");
        }
    }

    // 释放结果集内存
    mysql_free_result(res);

    /* 注册行为 且 用户名未被使用*/
    if (!isLogin && flag == true)
    {
        LOG_DEBUG("regirster!");
        bzero(order, 256);
        snprintf(order, 256, "INSERT INTO user(username, password) VALUES('%s','%s')", name.c_str(), pwd.c_str());
        LOG_DEBUG("%s", order);
        if (mysql_query(sql, order))
        {
            LOG_DEBUG("Insert error!");
            flag = false;
        }
        flag = true;
    }
    // SqlConnPool::Instance()->FreeConn(sql);
    LOG_DEBUG("UserVerify success!!");
    return flag;
}

std::string HttpRequest::path() const { return path_; }

std::string& HttpRequest::path() { return path_; }
std::string HttpRequest::method() const { return method_; }

std::string HttpRequest::version() const { return version_; }

std::string HttpRequest::GetPost(const std::string& key) const
{
    assert(key != "");
    if (post_.count(key) == 1)
    {
        return post_.find(key)->second;
    }
    return "";
}

std::string HttpRequest::GetPost(const char* key) const
{
    assert(key != nullptr);
    if (post_.count(key) == 1)
    {
        return post_.find(key)->second;
    }
    return "";
}
