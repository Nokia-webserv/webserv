#pragma once

#include "HttpRequest.hpp"

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
        HttpParser();

        HttpParsingResult run(std::string & recvBuffer); // main function
        void reset(); // for reuse
        HttpRequest & shareRequest(); // getter of the http request, but allows to modify it - used by ClientConnection
        // to initialize a reference to HttpRequest
    
    private:
        HttpRequestStatus _checkHead(std::string & recvBuffer); // checks if head part is complete
        HttpRequestStatus _checkBody(std::string & recvBuffer); // checks if body is complete
        void _extractHead(std::string & recvBuffer); // extracts the head part from recvBuffer
        void _extractBody(std::string & recvBuffer); // extreacts body from recvBuffer
        HttpRequestStatus _parseHead(); // parses and validates the head
        HttpRequestStatus _parseBody(); // parses and validates the body
        bool _hasBody(); // checks if http request has body

        HttpRequestStatus _status;
        HttpRequestError _error;
        size_t _scanPosition; // for incremental recvBuffer scanning
        std::string _strExtractedRequest; // buffer for the http request extracted from recvBuffer
        HttpRequest _parsedRequest;
        HttpParsingResult _result; // value returned to TCP connection layer
};

