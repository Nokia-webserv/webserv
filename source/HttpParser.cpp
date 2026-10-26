#include "HttpParser.hpp"

HttpParser::HttpParser(std::string & recvBuffer)
: _status(INCOMPLETE_HEAD), _error(NO_ERROR), _scanPosition(0), _recvBuffer(recvBuffer), _result(_status, _error)
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
    _scanPosition = 0;
    _strExtractedRequest.clear();
    _parsedRequest.clear();
}

HttpRequest & HttpParser::getRequest()
{
    return _parsedRequest;
}

HttpRequestStatus HttpParser::_checkHead()
{
    // read buffer
    // validate the read part (incremental framing validation)
    // save where it stopped in _scanPosition
    // return the status (COMPLETE, INCOMPLETE_HEAD, ERROR) accordingly
    return (HttpRequestStatus)0;
}

void HttpParser::_extractHead()
{
    // cut the read buffer where the complete head ends and paste it to _strExtractedRequest
    // reset _scanPosition
}

HttpRequestStatus HttpParser::_parseHead()
{
    // parse and validate the head (structural and semantic validation)
    // return COMPLETE on success and ERROR otherwise
    return (HttpRequestStatus)0;
}

bool HttpParser::_hasBody()
{
    // determine if http request has body
    return 0;
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


