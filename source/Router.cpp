/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Router.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: damohame <damohame@student.42berlin.d>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/11/06 22:50:16 by damohame          #+#    #+#             */
/*   Updated: 2026/11/12 00:40:02 by damohame         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Router.hpp"
#include "HttpResponse.hpp"
#include <cctype>
#include <sstream>
#include <fstream>

Router::Router()
{}

Router::~Router()
{}

// we have the request path and the paths in the config file
// we match the location of the request in the server if available
// example :
// in the server config file
//locations[0].path = "/"
//locations[1].path = "/images"
//locations[2].path = "/uploads"

// request path
// path = "/images/cat.jpg";
// we need to get the longest correct paths
// not the exact path
const LocationConfig* Router::matchLocation(const std::string& path, const ServerConfig& server) const
{
    // to get the best longest match
    const LocationConfig* best = NULL;
    size_t longest = 0;
    
    for (size_t i = 0; i <server.locations.size(); i++)
    {
        // temp to save the location config of the server(config file)
        const LocationConfig& location = server.locations[i];
        
        // Check if the location path is a prefix of the request path
        if (path.compare(0, location.path.size(), location.path) == 0)
        {
            if (location.path.size() > longest)
            {
                best = &location;
                longest = location.path.size();
            }
        }
    }
    return best;
}

// nginx behavior is Empty Methods = No Restrictions
// but we will only allow Get as a design choice if no methods
bool Router::isMethodAllowed(const std::string &method, const LocationConfig &location) const
{
    // if the method = get allow it
    // by default get is allowed if no methods
    if (location.methods.empty())
        return method == "GET";

    for (size_t i = 0; i < location.methods.size(); i++)
    {
        if (location.methods[i] == method)
            return (true);
    }
    return (false);
}

/*
server {
    listen 8080;
    server_name cats.com;
}

server {
    listen 8080;
    server_name dogs.com;
}
server {
    listen 8080;

}
*/

// Nginx Default Server: When multiple servers
// share the same IP and port,
// Nginx matches the Host header to server_name.
// If no match exists, it uses the first server by default,
const ServerConfig* Router::selectServer(const HttpRequest& request, const Listener& listener) const
{
    // Get all ServerConfigs associated with this listener
    const std::vector<const ServerConfig*>& configs = listener.getConfigs();
    
    if (configs.empty())
        return NULL;
   
    // ex :
    // _headers = {"host", "cats.com:8080"},
    const std::map<std::string, std::string>& headers = request.getHeaders();
    // find the value of the Host header "cars.com:8080"
    std::map<std::string, std::string>::const_iterator it = headers.find("host");
    
    // if no headers return default server
    if (it == headers.end() || it->second.empty())
        return configs[0];

    
    // cats.com:8080
    std::string host = it->second;
    // to get the host 
    size_t colon = host.find(':');
    if (colon != std::string::npos)
        host = host.substr(0, colon);

    // host = cats.com
    // all to lowercase
    for (size_t i = 0; i < host.size(); i++)
        host[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(host[i])));

    // time to compare with the server config
    for (size_t i = 0; i <configs.size(); i++)
    {
        const ServerConfig* virtualServer = configs[i];
        
        // one virtual server can have multiple server_names
        for (size_t j = 0; j < virtualServer->server_names.size(); j++)
        {
            std::string name = virtualServer->server_names[j];
            
            // to lower case
            for (size_t k = 0; k < name.size(); k++)
                name[k] = static_cast<char>(std::tolower(static_cast<unsigned char>(name[k])));
        
            // compare 
            if (host == name)
                return (virtualServer);
        }
    }
    return configs[0];
}

// 1- select the virtual server
// 2 match the location
// 3- if the method allowed
// 4- check redirect 
// 5- check CGI
// 6- methods handler
// - Httpresponse
// - error handler
RouteDecision Router::route(const HttpRequest& request, const Listener& listener, HttpResponse& response)
{
    RouteDecision result;

    // 1- virtual server selection
    result.server = selectServer(request, listener);
    if (result.server == NULL)
    {
        result.type = RouteDecision::ROUTE_ERROR;
        result.errorCode = 500;
        handleError(request, result, response);
        return result;
    }

    // 2- select the location
    result.location = matchLocation(request.getPath(), *result.server);
    if (result.location == NULL)
    {
        result.type = RouteDecision::ROUTE_ERROR;
        result.errorCode = 404;
        handleError(request, result, response);
        return result;
    }

    // 3- check if method is allowed
    if (!isMethodAllowed(request.getMethod(), *result.location))
    {
        result.type = RouteDecision::ROUTE_ERROR;
        result.errorCode = 405;
        handleError(request, result, response);
        return result;
    }
    
    // 4- check the redirect
    if (result.location->has_redirect)
    {
        //result.type = RouteDecision::ROUTE_REDIRECT;
        //result.errorCode = 0;
        // next pr
        // build the redirect response
       // handleRedirect(request, *result.location, response);
        //return result;
    }
    
    // 5- CGI 
    // example:
    // ".py" -> "/usr/bin/python3"
    // ".php" -> "/usr/bin/php-cgi"
    // to be later implemented

    
    // 6- method handler one request at a time
    if (request.getMethod() == "GET")
        handleGet(request, response, *result.location);
    else if (request.getMethod() == "POST")
        handlePost(request, response, *result.location);
    else if (request.getMethod() == "DELETE")
        handleDelete(request, response, *result.location);

    return result;
}

// prepare the response for error_pages and redirect
void Router::setResponseBody(HttpResponse& response, const std::string& path, int statusCode,
    const std::string& reason)
{
    std::string content;

    if (readHtmlPages(path, content))
    {
        response.setHeader("Content-Type", "text/html; charset=utf-8");
    }
    else
    {
        std::ostringstream text;
        text << statusCode << " " << reason << "\n";

        content = text.str();
        response.setHeader("Content-Type", "text/plain; charset=utf-8");
    }

    response.setBody(content);

    std::ostringstream length;
    length << content.size();

    response.setHeader("Content-Length", length.str());
}


// to get the error reason depending on the error
std::string Router::getErrorReason(int code) const
{
    switch (code)
    {
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 408: return "Request Timeout";
        case 413: return "Payload Too Large";
        case 414: return "URI Too Long";
        case 431: return "Request Header Fields Too Large";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 504: return "Gateway Timeout";
        case 505: return "HTTP Version Not Supported";
        default:  return "Unknown Error";
    }
}


//in case of 405 (method not allowed)
// we send the response with the allowed ones
/*
HTTP/1.1 405 Method Not Allowed
Allow: GET, POST
Content-Type: text/html; charset=utf-8
Content-Length: ...
*/
void Router::setAllowedMethodHeader(const LocationConfig& location, HttpResponse& response)
{
    std::string allowedMethods;
    
    // default, get allowed
    if (location.methods.empty())
        allowedMethods = "GET";

    else
    {
        // loop on the methods to add them together
        // Ex: ["GET", "POST"] -> "GET, POST"
        for (size_t i = 0; i < location.methods.size(); i++)
        {
            if (!allowedMethods.empty())
                allowedMethods += ", ";
            
            allowedMethods += location.methods[i];
        }
    }
    response.setHeader("Allow", allowedMethods);
}

// this function read the Html pages .html and return true
bool Router::readHtmlPages(const std::string& path, std::string& content)
{
    // open the error page file as it is in binary mode
    std::ifstream file(path.c_str(), std::ios::binary);

    // if could not open return false
    if (!file.is_open())
        return false;

    // create an ostring to store the file content
    std::ostringstream buffer;
    // read the entire file 
    buffer << file.rdbuf();

    // if any error occured return false
    if (file.bad())
        return false;

    // copy the file content 
    content = buffer.str();
    return true;
}

// one function to handle the error
// example of the configfile

/*server {
    listen 8080;
    error_page 404 /errors/404.html;

    location / {
        root ./www;
    }
}*/
// fisrt if there is custom error_pages
// if there is default error pages
// simple text form
void Router::handleError(const HttpRequest& request, const RouteDecision& result, HttpResponse& response)
{
    // get the HTTP reason phrase based on the code sent
    std::string reason;
    reason = getErrorReason(result.errorCode);
    
    // set the basic http info
    // example:
    // HTTP/1.1 404 Not Found
    response.setVersion(request.getVersion());
    response.setStatusCode(result.errorCode);
    response.setReasonPhrase(reason);

    if (result.errorCode == 405 && result.location != NULL)
        setAllowedMethodHeader(*result.location, response);
        
    // Store the error page content and check if can open
    std::string content;
    bool isPageOpen = false;

    // check if the server configuration has an error page
    // error_page 400 /errors/404.html

    if(result.server != NULL)
    {
        // iterator for the error_pages in the serverConfig
        // to check if the error code exists
        // and get its path
        // ex: -> error_pages = {404: "/errors/404.html"}
        std::map<int, std::string>::const_iterator it = result.server->error_pages.find(result.errorCode);
        
        // if the error_page for this code esixt
        if (it != result.server->error_pages.end())
        {
            // get the error page path but without the root
            std::string page = it->second;

            // find the location that matches the error page URI
            // /errors/404.html matches /
            const LocationConfig* errorLocation = matchLocation(page, *result.server);
            
            // if found
            if (errorLocation != NULL)
            {
                // get the root directory then add it to the path
                std::string root = errorLocation->root;
                std::string fullErrorPath = root + page;

                // read the error page for the cutomized 
                isPageOpen = readHtmlPages(fullErrorPath, content);    

            }
        }

    }

    // load the custom page 
    if (isPageOpen)
    {
        response.setBody(content);
        response.setHeader("Content-Type", "text/html; charset=utf-8");

        std::ostringstream length;
        length << content.size();
        response.setHeader("Content-Length", length.str());
    }
    // no custom error page
    // load the default or the error reason
    else
    {
        std::ostringstream path;
        path << "./www/errors/" << result.errorCode << ".html";

        setResponseBody(response, path.str(), result.errorCode, reason);
    }
}

int Router::parserErrorToStatus(HttpRequestError error)
{
    switch (error)
    {
        case BAD_REQUEST:
            return (400);
        case CONTENT_TOO_LARGE:
            return (413);
        case HEADER_FIELDS_TOO_LARGE:
           return (431);
        case NOT_IMPLEMENTED:
            return (501);
        case HTTP_VERSION_NOT_SUPPORTED:
            return (505);
        default:
            return (400);
    }
}

// public function to handle the errors in the HttpParser
void Router::handleParserError(const HttpRequest& request, const Listener &listener, HttpRequestError error, HttpResponse& response)
{
    // to reuse handleError
    // we need to pass a RouteDecision 
    RouteDecision result;
    
    result.type = RouteDecision::ROUTE_ERROR;
    result.errorCode = parserErrorToStatus(error);
    result.server = selectServer(request, listener);

    // the original is const and we need to modify the version
    // if not valid
    // local copy
    HttpRequest errorRequest = request;
    
    // to use a supported version in response
    if (errorRequest.getVersion() != "HTTP/1.0" && errorRequest.getVersion() != "HTTP/1.1")
        errorRequest.setVersion("HTTP/1.1");

    handleError(errorRequest, result, response);
    response.setHeader("Connection", "close");
    
}


void Router::handleGet(const HttpRequest& request, HttpResponse& response, const LocationConfig& location)
{
    (void)request;
    (void)response;
    (void) location;
    //to be implemented: Get handler
}
void Router::handlePost(const HttpRequest& request, HttpResponse& response, const LocationConfig& location)
{
    (void)request;
    (void)response;
    (void) location;
    //to be implemeneted:  post handler;
}
void Router::handleDelete(const HttpRequest& request, HttpResponse& response, const LocationConfig& location)
{
    (void)request;
    (void)response;
    (void) location;
    // to be implemented: Delete handler
}
