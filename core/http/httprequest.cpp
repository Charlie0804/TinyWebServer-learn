#include "httprequest.h"
using namespace std;

// Ĭ�ϴ����HTML����
const unordered_set<string> HttpRequest::DEFAULT_HTML{
    "/index", "/register", "/login", "/welcome", "/video", "/picture",
};

// ҳ���ǩӳ��
const unordered_map<string, int> HttpRequest::DEFAULT_HTML_TAG{
    // ע��ҳ�棬��¼ҳ��
    {"/register.html", 0},
    {"/login,html", 1},
};

// ��ʼ������
void HttpRequest::Init()
{
    // ����ַ���
    method_ = path_ = version_ = body_ = "";
    // ����״̬��
    state_ = REQUEST_LINE;
    header_.clear();
    post_.clear();
}

//  �����ѯ
bool HttpRequest::IsKeepAlive() const
{
    if (header_.count("Connection") == 1)
    {
        return header_.find("Connection")->second == "keep-alive" && version_ == "1.1";
    }
    return false;
}

// �������
bool HttpRequest::parse(Buffer* buff)
{
    // HTTP�зָ���
    const char CRLF[] = "\r\n";
    if (buff->ReadableBytes() <= 0)
    {
        return false;
    }

    while (buff->ReadableBytes() && state_ != FINISH)
    {
        // 1. �����н���λ��
        const char* lineEnd = search(buff.Peak(), buff.BeginWriteConst(), CRLF, CRLF + 2);
        std::string line(buff.Peak(), lineEnd);

        // 2. ����״̬������ͬ����
        switch (state_)
        {
            case REQUEST_LINE:
                if (!ParseRequestLine(line))
                {
                    return false;  // ����������
                }
                ParsePath();  // ����·��
                break;
            case HEADERS:
                ParseHeader_(line);  // ��������ͷ
                if (buff.ReadableBytes() <= 2)
                {
                    state_ = FINISH;  // ��������
                }
                break;
            case BODY:
                ParseBody_(line);  // ����������
                break;
            default:
                break;
        }
        if (lineEnd == buff.BeginWrite())
        {
            break;
        }

        // 3. �ƶ���������ȡλ��
        buff.RetrieveUntil(lineEnd + 2);
    }
    LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());
    return true;
}

// ����·��
void HttpRequest::ParsePath_()
{
    if (path_ == "/")
    {
        path_ = "/index.html";  // Ĭ����ҳ
    }
    else
    {
        // ����Ĭ��HTMLҳ��
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
            // ��ֵ���еĿո�Ϊ+����%20
            case '+':
                body_[i] = ' ';
                break;
            case '%':
                num = ConverHex(body_[i + 1]) * 16 + ConverHex(body_[i + 2]);
                body_[i + 2] = num % 10 + '0';
                body_[i + 1] = num / 10 + '0';
                i += 2;
                break;
            // ��ֵ�����ӷ�
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
    /* ��ѯ�û������� */
    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());
    LOG_DEBUG("%s", order);

    if (mysql_query(sql, order))
    {
        // ��ѯʧ�ܣ������ͷŽ����
        LOG_ERROR("Query failed: %s", mysql_error(sql));
        return false;
    }
    // ��ȡ��ѯ�����
    res = mysql_store_result(sql);
    // ��ȡ������е��ֶ���
    j = mysql_num_fields(res);
    // ��ȡ��������ֶ���Ϣ��Ԫ���ݣ�
    fields = mysql_fetch_fields(res);

    // �ӽ�����л�ȡ��һ������
    while (MYSQL_ROW row = mysql_fetch_row(res))
    {
        LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);
        string password(row[1]);
        /* ע����Ϊ �� �û���δ��ʹ��*/
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

    // �ͷŽ�����ڴ�
    mysql_free_result(res);

    /* ע����Ϊ �� �û���δ��ʹ��*/
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
