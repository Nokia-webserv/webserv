#pragma once
#include <string>
#include <map>

class HttpResponse {
    private:
        std::string _version;
        int _statusCode;
        std::string _reasonPhrase;
        std::map<std::string, std::string> _headers;
        std::string _body;
    public:
        //OCF
        HttpResponse();
        HttpResponse(const HttpResponse& other);
        HttpResponse& operator=(const HttpResponse& other);
        ~HttpResponse();
        //getters are not needed
        //setters
        void setVersion(const std::string& version);
        void setStatusCode(int statusCode);
        void setReasonPhrase(const std::string& reasonPhrase);
        void setHeader(const std::string& key, const std::string& value);
        void setBody(const std::string& body);

        //serialize (opposite of parse. it is enough to just turn
        //all fields into one string.)
        std::string serialize() const;
};