#include "HttpParser.hpp"

HttpParser::HttpParser(std::string & recvBuffer)
: _status(INCOMPLETE_HEAD), _error(NO_ERROR), _scanPosition(0), _recvBuffer(recvBuffer), _result(_status, _error),
_r1(false), _n1(false), _r2(false), _n2(false)
{}

HttpParsingResult HttpParser::run()
{   
    std::cout << "_status = " << _status << " (0 - incompl. head, 1 - incompl. body, 2 - complete, 3 - error)\n"; // Debug
    if (_status == INCOMPLETE_HEAD) // recvBuffer hasn't received a complete head yet
    {
        _status = _checkHead(); // scan recvBuffer to check if the head is complete
        std::cout << "head _status = " << _status << " (0 - incompl. head, 1 - incompl. body, 2 - complete, 3 - error)\n"; // Debug
        if (_status == COMPLETE) // head is complete
        {
            _extractHead(); // move head section from recvBuffer to http parser's buffer
            _status = _parseHead(); // parsing of the head section
            if (_status == COMPLETE && _hasBody()) // determining if http request has body
                _status = INCOMPLETE_BODY;
        }
    }
    
    std::cout << "_status = " << _status << " (0 - incompl. head, 1 - incompl. body, 2 - complete, 3 - error)\n"; // Debug

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
    std::cout << "checking the head\n\n"; // Debug
    std::cout << "start _scan position = " << _scanPosition << "\n";
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
    std::cout << "end _scan position = " << _scanPosition << "\n"; // Debug
    return _status;
}

void HttpParser::_extractHead()
{
    std::cout << "extracting the head\n\n";
    std::cout << "recvBuffer state before:\n";
    std::cout << "---\n";
    std::cout << _recvBuffer;
    std::cout << "\n---\n";
    std::cout << "http buffer state before:\n";
    std::cout << "---\n";
    std::cout << _strExtractedRequest;
    std::cout << "\n---\n";

    std::string head = _recvBuffer.substr(0, _scanPosition + 1);
    _strExtractedRequest.append(head);
    _recvBuffer.erase(0, _scanPosition + 1);
    _scanPosition = 0;

    std::cout << "recvBuffer state after:\n";
    std::cout << "---\n";
    std::cout << _recvBuffer;
    std::cout << "\n---\n";
    std::cout << "http buffer state after:\n";
    std::cout << "---\n";
    std::cout << _strExtractedRequest;
    std::cout << "\n---\n\n";    
}

HttpRequestStatus HttpParser::_parseHead()
{
    std::cout << "parsing the head\n\n"; // Debug
    
    if (!_parseRequestLine() || !_parseHeaders())
    {
        std::cout << "Head validation failed\n"; // Debug
        std::cout << "_status = " << _status << " (0 - incompl. head, 1 - incompl. body, 2 - complete, 3 - error)\n\n"; // Debug
        _error = BAD_REQUEST;
        return ERROR;
    }
    std::cout << "Head validation successful\n"; // Debug
    std::cout << "_status = " << _status << " (0 - incompl. head, 1 - incompl. body, 2 - complete, 3 - error)\n\n"; // Debug
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
    std::cout << "parsing request line\n\n"; // Debug
    
    std::string requestLine;

    if (!extractToken(_strExtractedRequest, "\r\n", requestLine))
        return false;
    std::cout << "Extracted request line: " << requestLine << "\n\n"; // Debug
    
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

    std::cout << "Result:\n"; // Debug
    std::cout << "Method: " << _parsedRequest.getMethod() << "\n";
    std::cout << "Path: " << _parsedRequest.getPath() << "\n";
    std::cout << "Query: " << _parsedRequest.getQuery() << "\n";
    std::cout << "Version: " << _parsedRequest.getVersion() << "\n\n";

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

bool HttpParser::_parseHeaders()
{
    std::cout << "parsing headers\n\n"; // Debug
    
    std::string name;
    std::string value;

    while (_strExtractedRequest != "\r\n")//!? dangerous but guaranteed. checkHead should guarantee a proper ending with \r\n\r\n so infinite loop must be excluded
    {
        std::cout << "http buffer state:\n"; // Debug
        std::cout << "---\n";
        std::cout << _strExtractedRequest;
        std::cout << "\n---\n";
        
        if (!extractToken(_strExtractedRequest, ":", name))
            return false;
        if (!extractToken(_strExtractedRequest, "\r\n", value))
            return false;

        std::cout << "extracting tokens\n\n"; // Debug
        std::cout << "Name: \"" << name << "\"\n";
        std::cout << "Value: \"" << value << "\"\n";

        trimStr(value);

        std::cout << "Trimmed value: \"" << value << "\"\n\n";

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


