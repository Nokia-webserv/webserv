#include "HttpRequest.hpp"

//OCF
HttpRequest::HttpRequest(): _headerSize(0)  {}

HttpRequest::HttpRequest(const HttpRequest& other) 
:   _method(other._method), 
    _path(other._path), 
    _query(other._query),
    _rawQuery(other._rawQuery),
    _version(other._version), 
    _headers(other._headers), 
    _body(other._body),
    _headerSize(other._headerSize)
    {}

HttpRequest& HttpRequest::operator=(const HttpRequest& other) {
    if (this != &other) {
        _method = other._method;
        _path = other._path;
        _query = other._query;
        _rawQuery = other._rawQuery;
        _version = other._version;
        _headers = other._headers;
        _body = other._body;
        _headerSize = other._headerSize;
    }
    return *this;
}
HttpRequest::~HttpRequest() {}

void HttpRequest::clear()
{
    _method.clear();
    _path.clear();
    _query.clear();
    _rawQuery.clear();
    _version.clear();
    _headers.clear();
    _body.clear();
    _headerSize = 0;
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
std::string HttpRequest::getRawQuery() const {
    return _rawQuery;
}
std::string HttpRequest::getVersion() const {
    return _version;
}
const std::map<std::string, std::string>& HttpRequest::getHeaders() const {
    return _headers;
}

// std::string HttpRequest::getHeader(const std::string& key) const {
//     std::map<std::string, std::string>::const_iterator it = _headers.find(strTolower(key));

//     if (it != _headers.end()) {
//         return it->second;
//     }

//     return "";
// } implementaion has a bug: value can be empty, so returning "" does not distinguish whether the key was found or not
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
void HttpRequest::setRawQuery(const std::string& rawQuery) {
    _rawQuery = rawQuery;
}
void HttpRequest::setVersion(const std::string& version) {
    _version = version;
}
void HttpRequest::setHeader(const std::string& key, const std::string& value) {
    _headers[strTolower(key)] = value;
}
void HttpRequest::setBody(const std::string& body) {
    _body = body;
}
void HttpRequest::setHeaderSize(size_t size){
    _headerSize = size;
}
size_t HttpRequest::getHeaderSize() const{
    return (_headerSize);
}