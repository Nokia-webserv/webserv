#include "HttpResponse.hpp"
#include <sstream>

HttpResponse::HttpResponse() : _statusCode(0) {}
HttpResponse::HttpResponse(const HttpResponse& other) : _version(other._version), _statusCode(other._statusCode), _reasonPhrase(other._reasonPhrase), _headers(other._headers), _body(other._body) {}
HttpResponse& HttpResponse::operator=(const HttpResponse& other) {
    if (this != &other) {
        _version = other._version;
        _statusCode = other._statusCode;
        _reasonPhrase = other._reasonPhrase;
        _headers = other._headers;
        _body = other._body;
    }
    return *this;
}
HttpResponse::~HttpResponse() {}

//getters are not needed
//setters
void HttpResponse::setVersion(const std::string& version) {
    _version = version;
}
void HttpResponse::setStatusCode(int statusCode) {
    _statusCode = statusCode;
}
void HttpResponse::setReasonPhrase(const std::string& reasonPhrase) {
    _reasonPhrase = reasonPhrase;
}
void HttpResponse::setHeader(const std::string& key, const std::string& value) {
    _headers[key] = value;
}
void HttpResponse::setBody(const std::string& body) {
    _body = body;
}

//serialize (opposite of parse. we need to turn all fields into one string for sending.)
//format:
// HTTP/1.1 200 OK\r\n
// Header: value\r\n
// Header: value\r\n
// ...headers
// \r\n
// body
std::string HttpResponse::serialize() const {
    std::stringstream ss;
    ss << _version << " " << _statusCode << " " << _reasonPhrase << "\r\n";
    
    //attach all headers
    for (std::map<std::string, std::string>::const_iterator it = _headers.begin(); it != _headers.end(); ++it)
    {
    ss << it->first << ": " << it->second << "\r\n";
    }
    
    ss << "\r\n" << _body;
    return ss.str();
}