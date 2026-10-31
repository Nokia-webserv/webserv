#pragma once

#include "HttpRequest.hpp"
#include "utils.hpp"
#include <iostream>
#include <algorithm>
#include <sstream>

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
    BAD_REQUEST,                 //400
    CONTENT_TOO_LARGE,           // 413
    HEADER_FIELDS_TOO_LARGE,     // 431
    NOT_IMPLEMENTED,             // 501
    HTTP_VERSION_NOT_SUPPORTED   // 505
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

        static const long tempClientMaxBodySize = 2000;
        static const unsigned long tempMaxChunkSizeLine = 1024;
        static const unsigned long tempClientMaxHeaderSize = 1024;
        static const unsigned long tempMaxTrailerSize = 1024;
    
    private:
        HttpRequestStatus _checkHead(); // checks if head part is complete
        HttpRequestStatus _checkBody(); // checks if body is complete
        void _extractHead(); // extracts the head part from recvBuffer
        void _extractBody(); // extreacts body from recvBuffer
        HttpRequestStatus _parseHead(); // parses and validates the head
        HttpRequestStatus _parseBody(); // parses and validates the body
        bool _hasBody(); // checks if http request has body
        bool _parseRequestLine(); // parses request line
        bool _validateRequestLine(); // validates request line
        bool _parseHeaders(); // parses headers
        bool _validateHeaders(std::string name, std::string value) const; // validates headers
        bool _headersPostValidation(); // provides additional validation on headers

        HttpRequestStatus _status;
        HttpRequestError _error;
        size_t _scanPosition; // for incremental recvBuffer scanning
        std::string & _recvBuffer; // reference to recvBuffer for easier access
        std::string _strExtractedRequest; // buffer for the http request extracted from recvBuffer
        HttpRequest _parsedRequest;
        HttpParsingResult _result; // value returned to TCP connection layer


        HttpRequestStatus _chunkBodySize(); 
        HttpRequestStatus _chunkBodyData();
        HttpRequestStatus _chunkBodyDataCRLF();
        HttpRequestStatus _chunkBodyTrailers();

        // booleans for CRLF validation
        bool _r1;
        bool _n1;
        bool _r2;
        bool _n2;

        // body

        // to distinguish between the body types
        enum BodyFraming
        {
            NONE,            //no body
            CONTENT_LENGTH,  //in case there is content-length
            CHUNKED          // it is chunked
        };

        BodyFraming _bodyFraming;
        size_t _bodyBytesRemaining;
        bool _setBodyFraming();
        std::string _strExtractedBody; // buffer for the body extracted from recvBuffer
        size_t _bodyBytesToExtract;    // number of Bytes available to extraction

        // to define the current state of the Chunk
        /* example of a chunk
            "5\r\n" 
            "HELLO\r\n"
            "3\r\n"
            "CAT\r\n"
            "0\r\n"
            "\r\n"*/
        enum ChunkState
        {
            SIZE,           // in hexadecimal
            DATA,
            DATA_CRLF,
            TRAILERS,
            COMPLETED
        };
        ChunkState _chunkState;
        size_t _chunkBytesRemaining;
        size_t _trailerBytesReceived;

};
