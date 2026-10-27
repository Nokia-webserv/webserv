#include "HttpParser.hpp"

HttpParser::HttpParser(std::string & recvBuffer)
: _status(INCOMPLETE_HEAD), _error(NO_ERROR), _scanPosition(0), _recvBuffer(recvBuffer), _result(_status, _error),
_r1(false), _n1(false), _r2(false), _n2(false)
{}

HttpParsingResult HttpParser::run()
{   
        // DEBUG START
        // std::cout << "_status = " << _status << "\n(0: incompl. head, 1: incompl. body, 2: complete, 3: error)\n\n";
        // DEBUG END
    
    if (_status == INCOMPLETE_HEAD) // recvBuffer hasn't received a complete head yet
    {
        _status = _checkHead(); // scan recvBuffer to check if the head is complete

            // DEBUG START
            // std::cout << "head _status = " << _status << "\n(0: incompl. head, 1: incompl. body, 2: complete, 3: error)\n\n";
            // DEBUG END

        if (_status == COMPLETE) // head is complete
        {
            _extractHead(); // move head section from recvBuffer to http parser's buffer
            _status = _parseHead(); // parsing of the head section
            if (_status == COMPLETE && _hasBody()) // determines if http request has body
                _status = INCOMPLETE_BODY;
        }
    }
    
        // DEBUG START
        // std::cout << "current _status = " << _status << "\n(0: incompl. head, 1: incompl. body, 2: complete, 3: error)\n\n";
        // DEBUG END

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

// checks if head is complete and validates CRLF endings
HttpRequestStatus HttpParser::_checkHead()
{
        // DEBUG START
        // std::cout << "checking the head\n\n";
        // std::cout << "start _scan position = " << _scanPosition << "\n";
        // DEBUG END
    
    size_t pos = 0;
    for (pos = _scanPosition; pos < _recvBuffer.size(); pos++) // takes last scan position for incremental scanning
    {
        unsigned long tempClientMaxHeaderSize = 200;
        char c = _recvBuffer[pos];
        switch (c)
        {
            // CRLF line ending means
            // \n can appear only after \r
            // \r for the first time can not appear only after \r, for the second time can appear only after \n
            case '\r':
            {
                if (!_r1)
                    _r1 = true; // no \r met before
                else if (_n1 && !_r2)
                    _r2 = true; // first \r active, so second \r valid only after \n
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
                    _n1 = true; // valid only after \r
                else if (_r2 && !_n2)
                {
                    _n2 = true; // valid only after second \r
                    _status = COMPLETE; // CRLFCRLF detected -> header is complete
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
                if ((_r1 && !_n1) || (_r2 && !_n2)) // \r is not followed by \n
                {
                    _status = ERROR;
                    _error = BAD_REQUEST;
                }
                _r1 = false; // CRLF preceeded another char -> header is not complete, but the current line is complete -> needs reset
                _n1 = false;
                _r2 = false;
                _n2 = false;
                break;
            }
        }
        if (pos >= tempClientMaxHeaderSize)
        {
            _status = ERROR;
            _error = BAD_REQUEST;
        }
        if (_status == COMPLETE || _status == ERROR)
            break;
    }
    _scanPosition = pos;
        
        // DEBUG START
        // std::cout << "end _scan position = " << _scanPosition << "\n";
        // DEBUG END
    
    return _status;
}

void HttpParser::_extractHead()
{
        // DEBUG START
        // std::cout << "extracting the head\n\n"; // Debug
        // std::cout << "recvBuffer state before:\n";
        // std::cout << "---\n";
        // std::cout << _recvBuffer;
        // std::cout << "\n---\n";
        // std::cout << "http buffer state before:\n";
        // std::cout << "---\n";
        // std::cout << _strExtractedRequest;
        // std::cout << "\n---\n";
        // DEBUG END

    std::string head = _recvBuffer.substr(0, _scanPosition + 1);
    _strExtractedRequest.append(head);
    _recvBuffer.erase(0, _scanPosition + 1);
    _scanPosition = 0;

        // DEBUG START
        // std::cout << "recvBuffer state after:\n"; // Debug
        // std::cout << "---\n";
        // std::cout << _recvBuffer;
        // std::cout << "\n---\n";
        // std::cout << "http buffer state after:\n";
        // std::cout << "---\n";
        // std::cout << _strExtractedRequest;
        // std::cout << "\n---\n\n";   
        // DEBUG END 
    }

HttpRequestStatus HttpParser::_parseHead()
{
        // DEBUG START
        // std::cout << "parsing the head\n\n";
        // DEBUG END
    
    if (!_parseRequestLine() || !_parseHeaders())
    {
            // DEBUG START
            // std::cout << "Head validation failed\n";
            // std::cout << "_status = " << _status << "\n(0: incompl. head, 1: incompl. body, 2: complete, 3: error)\n\n";
            // DEBUG END

        _error = BAD_REQUEST;
        return ERROR;
    }
    
        // DEBUG START
        // std::cout << "Head validation successful\n"; // Debug
        // std::cout << "_status = " << _status << "\n(0: incompl. head, 1: incompl. body, 2: complete, 3: error)\n\n";
        // DEBUG END
    
    return COMPLETE;
}

// extracts part of a source string from beginning until delimiter, erasing that part with delimiter from the source
bool extractToken(std::string & source, std::string delimiter, std::string & destination)
{
    size_t dLen = delimiter.size();
    
    // if delimiter is empty -> extract the whole source string
    if (!dLen)
    {
        destination.append(source);
        source.clear();
        return true;
    }
    
    size_t dPos = source.find(delimiter);
    
    // if delimiter is not found -> leave unchanged
    if (dPos == std::string::npos)
        return false;

    destination = source.substr(0, dPos);
    source.erase(0, dPos + dLen);
    return true;
}

bool HttpParser::_parseRequestLine()
{
        // DEBUG START
        // std::cout << "parsing request line\n\n";
        // DEBUG END
    
    std::string requestLine;

    // Request line ends with CRLF; if no CRLF found -> request is malformed
    if (!extractToken(_strExtractedRequest, "\r\n", requestLine))
        return false;

            // DEBUG START
            // std::cout << "Extracted request line: " << requestLine << "\n\n";
            // DEBUG END
    
    std::string temp;
    
    // Request line is delimited with spaces; if no space found -> request is malformed
    if (!extractToken(requestLine, " ", temp))
        return false;
    _parsedRequest.setMethod(temp);

    std::string pathAndQuery;
    // if no space -> malformed request
    if (!extractToken(requestLine, " ", pathAndQuery)) 
        return false;

    // Path and query are delimited with '?'
    if (extractToken(pathAndQuery, "?", temp))
    {
        _parsedRequest.setPath(temp);
        _parsedRequest.setQuery(pathAndQuery);
    }
    // if no '?' is present -> pathAndQuery remains unchanged, containing only path
    else
        _parsedRequest.setPath(pathAndQuery);
    

    _parsedRequest.setVersion(requestLine); // assigns rest to version; unexpected values are validated later
    requestLine.clear();

        // DEBUG START
        // std::cout << "Result:\n";
        // std::cout << "Method: " << _parsedRequest.getMethod() << "\n";
        // std::cout << "Path: " << _parsedRequest.getPath() << "\n";
        // std::cout << "Query: " << _parsedRequest.getQuery() << "\n";
        // std::cout << "Version: " << _parsedRequest.getVersion() << "\n\n";
        // DEBUG END

    return _validateRequestLine();
}

bool HttpParser::_validateRequestLine() const
{
    if (_parsedRequest.getMethod() != "POST" && _parsedRequest.getMethod() != "GET" && _parsedRequest.getMethod() != "DELETE")
        return false; // strict correspondance only allowed

    if (_parsedRequest.getPath().empty() || _parsedRequest.getPath()[0] != '/')
        return false; // empty path or relative path -> malformed request

    if (_parsedRequest.getVersion() != "HTTP/1.0" && _parsedRequest.getVersion() != "HTTP/1.1")
        return false;

    return true;
}

bool HttpParser::_parseHeaders()
{
        // DEBUG START
        // std::cout << "parsing headers\n\n";
        // DEBUG END
    
    std::string name;
    std::string value;

    while (_strExtractedRequest != "\r\n") // checkHead guarrantees CRLF structural validity -> CRLF will always remain in the end
    {
            // DEBUG START
            // std::cout << "http buffer state:\n";
            // std::cout << "---\n";
            // std::cout << _strExtractedRequest;
            // std::cout << "\n---\n";
            // DEBUG END
        
        // no ':' -> malformed request
        if (!extractToken(_strExtractedRequest, ":", name)) 
            return false;
        // after header name, CRLF expected, otherwise malformed request
        if (!extractToken(_strExtractedRequest, "\r\n", value)) 
            return false;

                // DEBUG START
                // std::cout << "extracting tokens\n\n";
                // std::cout << "Name: \"" << name << "\"\n";
                // std::cout << "Value: \"" << value << "\"\n";
                // DEBUG END

        trimStr(value); // trimm leading and trailing spaces

            // DEBUG START
            // std::cout << "Trimmed value: \"" << value << "\"\n\n";
            // DEBUG END

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
        name.find_first_not_of(charsAllowedInName) != std::string::npos || // unallowed chars found in header name
        value.find_first_of("\r\n") != std::string::npos || // CR or LF found in header value
        (
            strTolower(name) == "content-length" && 
            (value.find_first_not_of("0123456789") != std::string::npos || value.empty()) // content-length value is non-numeric or empty
        )
        )
            return false;
    
    // *using strTolower as header names should be case-insensitive
    if (strTolower(name) == "host" || strTolower(name) == "content-length")
    {
        const std::map<std::string, std::string> & headers = _parsedRequest.getHeaders();
        if (headers.find(strTolower(name)) != headers.end())
            return false; // Host and Content-length must not be duplicated
    }
    
    return true;
}

bool HttpParser::_headersPostValidation() const
{
        // DEBUG START
        // std::cout << "headers post validation\n\n";
        // DEBUG END
    
    const std::map<std::string, std::string> & headers = _parsedRequest.getHeaders();
    
    if (headers.find("content-length") != headers.end())
    {   
        long tempClientMaxBodySize = 2000;

        // convert content-length value to long and check for overflow
        IntLongOverflow length = strToIntLongOverflow("long", headers.find("content-length")->second);
        if (length.overflow != 1 || length.number > tempClientMaxBodySize) // if number overflows or is higher than client_max_body_size
            return false;
    }

    // both headers within the same request -> malformed
    if (headers.find("content-length") != headers.end() && headers.find("transfer-encoding") != headers.end())
        return false;

    if (_parsedRequest.getVersion() == "HTTP/1.1")
    {
        // for HTTP/1.1 Host must be present
        if (headers.find("host") == headers.end()) 
            return false;
        // and must not be empty
        if (headers.find("host")->second.empty()) 
            return false;
    }
    return true;
}

bool HttpParser::_hasBody()
{
    const std::map<std::string, std::string> & headers = _parsedRequest.getHeaders();

    if (headers.find("content-length") == headers.end() || strToIntLongOverflow("long", headers.find("content-length")->second).number == 0)
        return false;
    return true;
}

HttpRequestStatus HttpParser::_checkBody()
{
    // read buffer
    // validate the read part (incremental framing validation)
    // save where it stopped in _scanPosition
    // return the status (COMPLETE, INCOMPLETE_BODY, ERROR) accordingly
    return (HttpRequestStatus)2;
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
    return (HttpRequestStatus)2;
}

void HttpParser::setStatus(HttpRequestStatus status)
{
    _status = status;
}


