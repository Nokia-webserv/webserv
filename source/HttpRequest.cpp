#include "HttpRequest.hpp"

//OCF
HttpRequest::HttpRequest() {}
HttpRequest::HttpRequest(const HttpRequest& other) : _method(other._method), _path(other._path), _query(other._query), _version(other._version), _headers(other._headers), _body(other._body) {}
HttpRequest& HttpRequest::operator=(const HttpRequest& other) {
    if (this != &other) {
        _method = other._method;
        _path = other._path;
        _query = other._query;
        _version = other._version;
        _headers = other._headers;
        _body = other._body;
    }
    return *this;
}
HttpRequest::~HttpRequest() {}

void HttpRequest::clear()
{
    _method.clear();
    _path.clear();
    _query.clear();
    _version.clear();
    _headers.clear();
    _body.clear();
}

//getters
std::string HttpRequest::getMethod() const {
    return _method;
}
std::string HttpRequest::getPath() const {
    return _path;
}
std::string HttpRequest::getQuery() const {
    return _query;
}
std::string HttpRequest::getVersion() const {
    return _version;
}
const std::map<std::string, std::string>& HttpRequest::getHeaders() const {
    return _headers;
}

std::string keyTolower(std::string key)
{
    std::string keyLowerCase;

    for (size_t i = 0; i < key.size(); i++)
        keyLowerCase.append(1, std::tolower(key.c_str()[i]));
    
    return keyLowerCase;
}

std::string HttpRequest::getHeader(const std::string& key) const {
    std::map<std::string, std::string>::const_iterator it = _headers.find(keyTolower(key));

    if (it != _headers.end()) {
        return it->second;
    }

    return "";
}
std::string HttpRequest::getBody() const {
    return _body;
}

//setters
void HttpRequest::setMethod(const std::string& method) {
    _method = method;
}
void HttpRequest::setPath(const std::string& path) {
    _path = path;
}
void HttpRequest::setQuery(const std::string& query) {
    _query = query;
}
void HttpRequest::setVersion(const std::string& version) {
    _version = version;
}
void HttpRequest::setHeader(const std::string& key, const std::string& value) {
    _headers[keyTolower(key)] = value;
}
void HttpRequest::setBody(const std::string& body) {
    _body = body;
}