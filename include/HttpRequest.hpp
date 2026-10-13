#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include <string>
#include <map>

class HttpRequest {
    private:
        std::string _method;
        std::string _path;
        std::string _query;
        std::string _version;
        std::map<std::string, std::string> _headers;
        std::string _body;
    public:
        //OCF
        HttpRequest();
        HttpRequest(const HttpRequest& other);
        HttpRequest& operator=(const HttpRequest& other);
        ~HttpRequest();

        //getters
        std::string getMethod() const;
        std::string getPath() const;
        std::string getQuery() const;
        std::string getVersion() const;
        const std::map<std::string, std::string>& getHeaders() const;
        std::string getHeader(const std::string& key) const;
        std::string getBody() const;

        //setters
        void setMethod(const std::string& method);
        void setPath(const std::string& path);
        void setQuery(const std::string& query);
        void setVersion(const std::string& version);
        void setHeader(const std::string& key, const std::string& value);
        void setBody(const std::string& body);
};

#endif