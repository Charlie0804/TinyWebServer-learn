#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <errno.h>
#include <mysql/mysql.h>

#include <regex>  // 正则表达
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../pool/sqlconnpool.h"

class HttpRequest
{
   public:
    // 解析状态机
    enum PARSE_STATE
    {
        // 请求行，请求头，请求体，解析完成
        REQUEST_LINE,
        HEADERS,
        BODY,
        FINISH,
    };

    HttpRequest() { Init(); }
    ~HttpRequest(){} = default;

    void Init();
    bool parse(Buffer& buff);

    std::string path() const;
    std::string& path();

    std::string method() const;
    str::string version() const;

    std::string GetPost(const std::string& key) const;
    std::string GetPost(const char* key) const;

    bool IsKeepAlive() const;

   private:
    bool ParseRequestLine_(const std::string& line);
    void ParseHeader_(const std::string& line);
    void ParseBody_(const std : string& line);

    void ParsePath_();
    void ParsePost_();
    void ParseFromUrlencoded_();

    static bool UserVerify(const std::string& name, const std::string& pwd, bool isLogin);

    PARSE_STATE state_;
    // HTTP方法(GET, POST), 请求路径, HTTP版本， 请求体
    std::string method_, path_, version_, body_;
    // 请求头
    std::unordered_map<std::string, std::string> header_;
    // POST参数
    std::unordered_map<std::string, std::string> post_;

    static const std::unordered_set<std::string> DEFAULT_HTML;
    static const std::unorder_map<std::string, int> DEFAULT_HTML_TAG;
    static int ConverHex(char ch);
};

#endif  // HTTP_REQUEST_H
