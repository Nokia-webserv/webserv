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
std::string HttpRequest::getHeader(std::string& key) const {
    std::map<std::string, std::string>::const_iterator it = _headers.find(key);

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
    _headers[key] = value;
}
void HttpRequest::setBody(const std::string& body) {
    _body = body;
}