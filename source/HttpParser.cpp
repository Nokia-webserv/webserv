#include "HttpParser.hpp"

HttpParser::HttpParser(std::string & recvBuffer)
: _status(INCOMPLETE_HEAD), _error(NO_ERROR), _scanPosition(0), _recvBuffer(recvBuffer), _result(_status, _error),
_r1(false), _n1(false), _r2(false), _n2(false)
{}

HttpParsingResult HttpParser::run()
{   
    if (_status == INCOMPLETE_HEAD) // recvBuffer hasn't received a complete head yet
    {
        _status = _checkHead(); // scan recvBuffer to check if the head is complete
        if (_status == COMPLETE) // head is complete
        {
            _extractHead(); // move head section from recvBuffer to http parser's buffer
            _status = _parseHead(); // parsing of the head section
            if (_status == COMPLETE && _hasBody()) // determining if http request has body
                _status = INCOMPLETE_BODY;
        }
    }
    
    if (_status == INCOMPLETE_BODY)
    {
        _status = _checkBody();
        if (_status == COMPLETE)
        {
            _extractBody();
            _status = _parseBody();
        }
    }
    
    return _result;
}

void HttpParser::reset()
{
    _status = INCOMPLETE_HEAD;
    _error = NO_ERROR;
    _scanPosition = 0;
    _strExtractedRequest.clear();
    _parsedRequest.clear();
    _r1 = false;
    _n1 = false;
    _r2 = false;
    _n2 = false;
}

HttpRequest & HttpParser::getRequest()
{
    return _parsedRequest;
}

// header max size!
HttpRequestStatus HttpParser::_checkHead()
{
    size_t pos = 0;
    for (pos = _scanPosition; pos < _recvBuffer.size(); pos++)
    {
        char c = _recvBuffer[pos];
        switch (c)
        {
            case '\r':
            {
                if (!_r1)
                    _r1 = true;
                else if (_n1 && !_r2)
                    _r2 = true;
                else
                {
                    _status = ERROR;
                    _error = BAD_REQUEST;
                }
                break;
            }
            case '\n':
            {
                if (_r1 && !_n1)
                    _n1 = true;
                else if (_r2 && !_n2)
                {
                    _n2 = true;
                    _status = COMPLETE;
                }
                else
                {
                    _status = ERROR;
                    _error = BAD_REQUEST;
                }
                break;
            }
            default:
            {
                if ((_r1 && !_n1) || (_r2 && !_n2))
                {
                    _status = ERROR;
                    _error = BAD_REQUEST;
                }
                _r1 = false;
                _n1 = false;
                _r2 = false;
                _n2 = false;
                break;
            }
        }
        if (_status == COMPLETE || _status == ERROR)
            break;
    }
    _scanPosition = pos;
    return _status;
}

void HttpParser::_extractHead()
{
    std::string head = _recvBuffer.substr(0, _scanPosition + 1);
    _strExtractedRequest.append(head);
    _recvBuffer.erase(0, _scanPosition + 1);
    _scanPosition = 0;
}

HttpRequestStatus HttpParser::_parseHead()
{
    if (!_parseRequestLine() || !_parseHeaders())
    {
        _error = BAD_REQUEST;
        return ERROR;
    }
    return COMPLETE;
}

bool extractToken(std::string & source, std::string delimiter, std::string & destination)
{
    size_t dLen = delimiter.size();
    
    if (!dLen)
    {
        destination.append(source);
        source.clear();
        return true;
    }
    
    size_t dPos = source.find(delimiter);
    
    if (dPos == std::string::npos)
        return false;

    destination = source.substr(0, dPos);
    source.erase(0, dPos + dLen);
    return true;
}

bool HttpParser::_parseRequestLine()
{
    std::string requestLine;

    if (!extractToken(_strExtractedRequest, "\r\n", requestLine))
        return false;

    std::string temp;
    
    if (!extractToken(requestLine, " ", temp))
        return false;
    _parsedRequest.setMethod(temp);

    std::string pathAndQuery;
    if (!extractToken(requestLine, " ", pathAndQuery))
        return false;

    if (extractToken(pathAndQuery, "?", temp))
    {
        _parsedRequest.setPath(temp);
        _parsedRequest.setQuery(pathAndQuery);
    }
    else
        _parsedRequest.setPath(pathAndQuery);

    _parsedRequest.setVersion(requestLine);
    requestLine.clear();

    return _validateRequestLine();
}

bool HttpParser::_validateRequestLine() const
{
    if (_parsedRequest.getMethod() != "POST" && _parsedRequest.getMethod() != "GET" && _parsedRequest.getMethod() != "DELETE")
        return false;

    if (_parsedRequest.getPath().empty() || _parsedRequest.getPath()[0] != '/')
        return false;

    if (_parsedRequest.getVersion() != "HTTP/1.0" && _parsedRequest.getVersion() != "HTTP/1.1")
        return false;

    return true;
}

void trimValue(std::string & value)
{
    while (!value.empty() && value.find_first_of(" \t") == 0)
        value.erase(0, 1);
    while (!value.empty() && (value[value.size()-1] == ' ' || value[value.size()-1] == '\t'))
        value.erase(value.size() - 1);
}

bool HttpParser::_parseHeaders()
{
    std::string name;
    std::string value;

    while (_strExtractedRequest != "\r\n")//!? dangerous but guaranteed. checkHead should guarantee a proper ending with \r\n\r\n so infinite loop must be excluded
    {
        if (!extractToken(_strExtractedRequest, ":", name))
            return false;
        if (!extractToken(_strExtractedRequest, "\r\n", value))
            return false;
        trimValue(value);

        if (!_validateHeaders(name, value))
            return false;
        _parsedRequest.setHeader(name, value);
    }

    return _headersPostValidation();
}

bool HttpParser::_validateHeaders(std::string name, std::string value) const
{
    std::string charsAllowedInName =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789"
        "!#$%&'*+-.^_`|~";

    if 
        (
        name.empty() ||
        name.find_first_not_of(charsAllowedInName) != std::string::npos ||
        value.find_first_of("\r\n") != std::string::npos ||
        (
            strTolower(name) == "content-length" && 
            (value.find_first_not_of("0123456789") != std::string::npos || value.empty())
        )
        )
            return false;

    if (strTolower(name) == "host" || strTolower(name) == "content-length")
    {
        const std::map<std::string, std::string> & headers = _parsedRequest.getHeaders();
        if (headers.find(strTolower(name)) != headers.end()) 
            return false;
    }
    
    return true;
}

bool HttpParser::_headersPostValidation() const
{
    const std::map<std::string, std::string> & headers = _parsedRequest.getHeaders();
    
    if (headers.find("content-length") != headers.end())
    {   
        long tempClientMaxBodySize = 2000;
        
        IntLongOverflow length = strToIntLongOverflow(headers.find("content-length")->second, "long");
        if (length.overflow != 1 || length.number > tempClientMaxBodySize)
            return false;
    }

    if (headers.find("content-length") != headers.end() && headers.find("transfer-encoding") != headers.end())
        return false;

    if (_parsedRequest.getVersion() == "HTTP/1.1")
    {
        if (headers.find("host") == headers.end())
            return false;
        if (headers.find("host")->second.empty())
            return false;
    }
    return true;
}

bool HttpParser::_hasBody()
{
    const std::map<std::string, std::string> & headers = _parsedRequest.getHeaders();

    if (headers.find("content-length") == headers.end() || strToIntLongOverflow(headers.find("content-length")->second, "long").number == 0)
        return false;
    return true;
}

HttpRequestStatus HttpParser::_checkBody()
{
    // read buffer
    // validate the read part (incremental framing validation)
    // save where it stopped in _scanPosition
    // return the status (COMPLETE, INCOMPLETE_BODY, ERROR) accordingly
    return (HttpRequestStatus)0;
}

void HttpParser::_extractBody()
{
    // cut the read buffer where the complete body ends and paste it to _strExtractedRequest
    // reset _scanPosition
}

HttpRequestStatus HttpParser::_parseBody()
{
    // parse and validate the body (structural and semantic validation)
    // return COMPLETE on success and ERROR otherwise
    return (HttpRequestStatus)0;
}

void HttpParser::setStatus(HttpRequestStatus status)
{
    _status = status;
}


