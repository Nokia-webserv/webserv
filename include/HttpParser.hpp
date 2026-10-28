#pragma once

#include "HttpRequest.hpp"
#include "utils.hpp"
#include <iostream>

enum HttpRequestStatus
{
    INCOMPLETE_HEAD = 0,
    INCOMPLETE_BODY = 1, // client connection layer only needs to know if it's incomplete -> can be checked as < 2
    COMPLETE,
    ERROR
};

// error codes
enum HttpRequestError
{
    NO_ERROR,
    BAD_REQUEST
    //to be added on later stages
};

// value returned to TCP connection layer after a parser call
struct HttpParsingResult
{
    const HttpRequestStatus & status; 
    const HttpRequestError & error;

    // struct constructor - initializes fields with relevant references -> no need to modify them before return
    HttpParsingResult(const HttpRequestStatus & status, const HttpRequestError & error)
    : status(status), error(error)
    {}
};

class HttpParser {
    public:
        HttpParser(std::string & recvBuffer);

        HttpParsingResult run(); // main function
        void reset(); // for reuse
        HttpRequest & getRequest(); // getter of the http request that allows to modify it - used by ClientConnection
        // to initialize a reference to HttpRequest
        void setStatus(HttpRequestStatus status); // setter (for testing)

        static const unsigned long tempClientMaxHeaderSize = 200;
    
    private:
        HttpRequestStatus _checkHead(); // checks if head part is complete
        HttpRequestStatus _checkBody(); // checks if body is complete
        void _extractHead(); // extracts the head part from recvBuffer
        void _extractBody(); // extreacts body from recvBuffer
        HttpRequestStatus _parseHead(); // parses and validates the head
        HttpRequestStatus _parseBody(); // parses and validates the body
        bool _hasBody(); // checks if http request has body
        bool _parseRequestLine(); // parses request line
        bool _validateRequestLine() const; // validates request line
        bool _parseHeaders(); // parses headers
        bool _validateHeaders(std::string name, std::string value) const; // validates headers
        bool _headersPostValidation() const; // provides additional validation on headers

        HttpRequestStatus _status;
        HttpRequestError _error;
        size_t _scanPosition; // for incremental recvBuffer scanning
        std::string & _recvBuffer; // reference to recvBuffer for easier access
        std::string _strExtractedRequest; // buffer for the http request extracted from recvBuffer
        HttpRequest _parsedRequest;
        HttpParsingResult _result; // value returned to TCP connection layer

        // booleans for CRLF validation
        bool _r1;
        bool _n1;
        bool _r2;
        bool _n2;
};



