# Webserv HTTP Specification

## Scope

Webserv implements a subset of HTTP.

Reference baseline:
- HTTP/1.0 is suggested by the subject
- HTTP/1.1 requests should still be accepted where needed for real browser/curl compatibility

## Methods

Required:
- `GET`
- `POST`
- `DELETE`

## Request Structure

```text
METHOD URI HTTP_VERSION\r\n
Header-Name: value\r\n
Header-Name: value\r\n
\r\n
[optional body]
```

Parse into:
- method
- path
- query
- version
- headers
- body

## Response Structure

```text
HTTP_VERSION STATUS_CODE REASON_PHRASE\r\n
Header-Name: value\r\n
Header-Name: value\r\n
\r\n
[optional body]
```

Generate from:
- version
- status code
- reason phrase
- headers
- body

## Relevant Headers

Support at least:

```text
Content-Length
Content-Type
Host
Transfer-Encoding
Location
```

Headers should be parsed generically as:

```text
name: value
```

rather than by hardcoding only known names.

## Status Codes

Implement the status codes needed by actual Webserv behavior.

Typical set:

```text
200 OK
201 Created
204 No Content
301 Moved Permanently
302 Found
400 Bad Request
403 Forbidden
404 Not Found
405 Method Not Allowed
413 Payload Too Large
500 Internal Server Error
501 Not Implemented
```

## Suggested Classes

### `HttpRequest`

```cpp
class HttpRequest {
private:
    std::string method;
    std::string path;
    std::string query;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;
};
```

### `HttpResponse`

```cpp
class HttpResponse {
private:
    std::string version;
    int statusCode;
    std::string reasonPhrase;
    std::map<std::string, std::string> headers;
    std::string body;
};
```

## Parsing Flow

```text
raw bytes
-> request line
-> headers
-> empty line
-> optional body
-> HttpRequest
```

## Response Building Flow

```text
HttpResponse
-> status line
-> headers
-> empty line
-> body
-> send()
```
