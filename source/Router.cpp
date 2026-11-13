/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   router.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: atammur <atammur@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/11/06 22:50:16 by damohame          #+#    #+#             */
/*   Updated: 2026/11/13 18:33:29 by atammur          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Router.hpp"
#include "HttpResponse.hpp"
#include <cctype>
#include <iostream>
#include <sstream>
#include <fstream>
#include <sys/stat.h>
#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h> 
#include "utils.hpp"

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
        handleGet(request, response, result);
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

void Router::handleGet(const HttpRequest& request, HttpResponse& response, RouteDecision& result)
{
    //  1.build path from URL and locationConfig
    std::string path = buildPath(request.getPath(), *result.location);

    //  2.check whats inside the path. we use sys/stat lib for that. 
    // (stat() is specifically allowed in subject)
    // S_ISREG (is regular file) and S_ISDIR are macros for st_mode from sys/stat.
    struct stat info;
    
    //if cant get file info. handle file opening error
    if (stat(path.c_str(), &info) == -1) {
        handleFileError(errno, request, response, result);
        return ;
    }
    //if file
    else if (S_ISREG(info.st_mode))
        serveFile(request, response, result, path);  
    //if directory
    else if (S_ISDIR(info.st_mode))
        serveDir(request, response, result, path);
    //if no file nor dir
    else {
        result.type = RouteDecision::ROUTE_ERROR;
        result.errorCode = 403;
        handleError(request, result, response);
    }
}

void Router::serveFile(const HttpRequest& request, HttpResponse& response, RouteDecision& result, std::string& path) {
    //read and fill response
        int fd = open(path.c_str(), O_RDONLY);
        
        if (fd == -1) {
            handleFileError(errno, request, response, result);
            return ;
        }
        else
        {
            std::string body;
            char buf[1000];
            ssize_t bytesRead;
            
            while ((bytesRead = read(fd, buf, sizeof(buf))) > 0) {
                body.append(buf, bytesRead);
            }
            if (bytesRead == -1) {
                close(fd);
                //after read() we are not allowed to use errno, so we pass generic -1.
                handleFileError(-1, request, response, result);
                return ;
            }
            //if successfully read, check file format and fill headers map to pass to response.
            std::map<std::string, std::string> headers;
            headers["Content-Type"] = getContentType(path);
            headers["Content-Length"] = toString(body.size());
            //get vector of headers
            fillResponse(response, request.getVersion(), 200, headers, body);
            close(fd);
        }
        return ;
}

void Router::serveDir(const HttpRequest& request, HttpResponse& response, RouteDecision& result, std::string& path) {
    std::string requestPath = request.getPath();
    
    //if url doesnt end with '/'
    if (!requestPath.empty() && requestPath[requestPath.size() - 1] != '/') {
        //redirect to url with '/'
        result.type = RouteDecision::ROUTE_REDIRECT;
        handleRedirect(request, 301, requestPath + "/", response);
        return ;
    }
    
    //check index only if it is in config and is not empty
    if (!result.location->index.empty()) {
        std::string indexPath = path;
        //create a new path to index file
        if (indexPath[indexPath.size() - 1] != '/')
            indexPath.append("/");
        indexPath.append(result.location->index);
        
        //check index file
        struct stat fileInfo;
        
        int indexStatus = stat(indexPath.c_str(), &fileInfo);
        if (indexStatus == -1 && errno != ENOENT && errno != ENOTDIR) {
            handleFileError(errno, request, response, result);
            return ;
        }
        //if we have index file
        if (indexStatus == 0 && S_ISREG(fileInfo.st_mode)) {
            // just serve the file
            serveFile(request, response, result, indexPath);
            return ;
        }
        //if we have non regular index file
        else if (indexStatus == 0 && !S_ISREG(fileInfo.st_mode)) {
            result.type = RouteDecision::ROUTE_ERROR;
            result.errorCode = 403;
            handleError(request, result, response);
            return ;
        }
    }
    //if no index file is set up in config OR we couldnt find it
    //if autoindex on -> generate files list
    if (result.location->autoindex) {
        try {
            std::string body = generateFilesList(path, requestPath);
            std::map<std::string, std::string> headers;
            headers["Content-Type"] = "text/html";
            headers["Content-Length"] = toString(body.size());
            fillResponse(response, request.getVersion(), 200, headers, body);
        }
        catch (int errNum) {
            handleFileError(errNum, request, response, result);
        }
    }
    //if autoindex off -> handle error response 403
    else {
        result.type = RouteDecision::ROUTE_ERROR;
        result.errorCode = 403;
        handleError(request, result, response);
    }
}

//our server needs to generate a html body with the list of dir entry for response.
//here we use dirent.h - format of directory entries. 
//opendir(), readdir() and closedir() are specifically allowed in subject.
std::string Router::generateFilesList(std::string& dirPath, std::string& requestPath) {
    std::ostringstream filesList;
    filesList << "<html><body><ul>\n";
    //open directory for reading entries names
    DIR* dir = opendir(dirPath.c_str());
    if (dir == NULL) {
        throw errno;
    }
    
    //add parent directory to html (like nginx)
    if (requestPath != "/")
        filesList << "<li><a href=\"../\">../</a></li>\n";
        
    //for each entry create html link like this <li><a href="/images/cat.jpg">cat.jpg</a></li>
    struct dirent* entry;
    while (true) {
        errno = 0; //set errno to 0 according to readdir() manpage
        entry = readdir(dir);
        
        if (entry == NULL) {
            //check for errors
            if (errno != 0) {
                int errNum = errno; //protect errno from closedir()
                closedir(dir);
                throw errNum;
            }
            break;
        }

        //skip . and ..
        std::string filename = entry->d_name;
        if (filename == "." || filename == "..")
            continue;

        filesList   << "<li><a href=\""
                    //TODO: issue 115 << escapeHtml(urlEncodePath(requestPath + filename))
                    << requestPath << filename
                    << "\">" 
                    //TODO: issue 115 << escapeHtml(filename)
                    << filename
                    << "</a></li>\n";
    }
    if (closedir(dir) == -1)
        throw errno;
    filesList << "</ul></body></html>";
    return filesList.str();
}

std:: string Router::escapeHtml(std::string filename) {
    
}

//we decode it in request parser, need to encode back for response
std:: string Router::urlEncodePath(std::string path) {
    std::string encoded;
    std::string allowed = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789-._~/";
    
    //if normal chars - we copy them straight
    for (size_t i = 0; i < path.size(); i++) {
        if (allowed.find(path[i]) != std::string::npos) {
            encoded += path[i];
            continue;
        }

        //if need to encode - we encode back to hex
        std::ostringstream ss;

        ss << "%" 
        << std::hex 
        << std::uppercase 
        << std::setfill('0') //fill with leading 0 if needed
        << std::setw(2) //gotta be 2 digits
        << static_cast<int>(static_cast<unsigned char>(path[i])); //unsigned char for UTF-8
        encoded += ss.str();
    }
    return encoded;
}

// according to subject:
// requestPath:   /images/holiday/cat.png
// location.path: /images
// location.root: ./www/pictures
// result: ./www/pictures/holiday/cat.png

// Resolves a VALIDATED(!) request URI against the matched
// location root. URI normalization is handled by Router (issue #107)
// can be used for DELETE; POST possibly partly due to upload_store directive
std::string Router::buildPath(const std::string& requestPath, const LocationConfig& location) {
    std::string path;
    std::string pathFromRequest;

    path.append(location.root);
    
    //add slash to path if doesnt end with one
    if (!path.empty() && path[path.size() - 1] != '/')
        path.append("/");
        
    pathFromRequest = requestPath.substr(location.path.size());
    //remove leading slash from request path if has one
    if (!pathFromRequest.empty() && pathFromRequest[0] == '/')
        pathFromRequest.erase(0, 1);
    
    path.append(pathFromRequest);
    return path;
}

void Router::handleFileError(int errorNum, const HttpRequest& request, HttpResponse& response, RouteDecision& result) {
    if (errorNum == ENOENT || errorNum == ENOTDIR)
            result.errorCode = 404;
    else if (errorNum == EACCES)
        result.errorCode = 403;
    else {
        //log to server and prepare a response
        std::cerr << "File operation failed. ErrorNum: " << errorNum << std::endl;
        result.errorCode = 500;
    }
    result.type = RouteDecision::ROUTE_ERROR;
    handleError(request, result, response);
}

// std::string _version;
// int _statusCode;
// std::string _reasonPhrase;
// std::map<std::string, std::string> _headers;
// std::string _body;
//router method?
void Router::fillResponse(
    HttpResponse& response, 
    const std::string& version, 
    int statusCode, 
    const std::map<std::string, std::string>& headers, 
    const std::string& body) {
    response.setVersion(version);
    response.setStatusCode(statusCode);
    response.setReasonPhrase(getReasonPhrase(statusCode));
    for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it) {
        response.setHeader(it->first, it->second);
    }
    response.setBody(body);
}

//router method
std::string Router::getReasonPhrase(int statusCode) {
    std::map<int, std::string> reasons;
    
    reasons[200] = "OK";
    reasons[201] = "Created";
    reasons[204] = "No Content";

    std::map<int, std::string>::const_iterator it = reasons.find(statusCode);
    
    if (it != reasons.end())
        return it->second;

    return "";
}

std::string Router::getContentType(const std::string& path) {
    //we use nginx MIME.types most valuable(imo) types as inspiration
    std::map<std::string, std::string> fileFormat;

    // text and scripts
    fileFormat["html"] = "text/html";
    fileFormat["htm"] = "text/html";
    fileFormat["css"] = "text/css";
    fileFormat["js"] = "text/javascript";
    fileFormat["txt"] = "text/plain";
    fileFormat["json"] = "application/json";

    // images
    fileFormat["png"] = "image/png";
    fileFormat["jpg"] = "image/jpeg";
    fileFormat["jpeg"] = "image/jpeg";
    fileFormat["gif"] = "image/gif";
    fileFormat["svg"] = "image/svg+xml";
    fileFormat["webp"] = "image/webp";
    fileFormat["ico"] = "image/x-icon";

    // fonts
    fileFormat["woff"] = "font/woff";
    fileFormat["woff2"] = "font/woff2";
    fileFormat["ttf"] = "font/ttf";
    fileFormat["otf"] = "font/otf";

    // video
    fileFormat["mp4"] = "video/mp4";
    fileFormat["webm"] = "video/webm";

    // audio
    fileFormat["mp3"] = "audio/mpeg";
    fileFormat["wav"] = "audio/wav";
    fileFormat["ogg"] = "audio/ogg";
    fileFormat["m4a"] = "audio/mp4";
    fileFormat["aac"] = "audio/aac";

    //find the last '.'
    std::string::size_type dot = path.find_last_of('.');
    std::string::size_type slash = path.find_last_of('/');
    
    //if theres no '.' or its directory name - return generic file type (nginx uses it as default/ unknown type in nginx.conf)
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return "application/octet-stream";
    
    //get the formatName
    std::string formatName = path.substr(dot + 1);
    formatName = strTolower(formatName);
    
    //find in format type table
    std::map<std::string, std::string>::const_iterator it = fileFormat.find(formatName);
    //return generic type if not found
    if (it == fileFormat.end())
        return "application/octet-stream";
    
    return it->second;
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
