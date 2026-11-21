/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Router.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: damohame <damohame@student.42berlin.d>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/11/06 22:25:55 by damohame          #+#    #+#             */
/*   Updated: 2026/11/20 15:18:33 by damohame         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include "ServerConfig.hpp"
#include "HttpRequest.hpp"
#include "Listener.hpp"
#include "HttpResponse.hpp"
#include "HttpParser.hpp"
#include <string>

class ClientConnection;

struct RouteDecision
{
    enum RouteType
    {
        ROUTE_SERVE,
        ROUTE_REDIRECT,
        ROUTE_CGI,
        ROUTE_ERROR

    };
    
    RouteType type;
    int errorCode;
    const ServerConfig* server;
    const LocationConfig* location;

    RouteDecision() : type(ROUTE_SERVE), errorCode(0), server(NULL), location(NULL)
    {}
};

class Router
{
    
    public:
        Router();
        ~Router();

        RouteDecision route(ClientConnection& client, HttpResponse& response);
        void handleParserError(const HttpRequest& request, const Listener &listener, HttpRequestError error, HttpResponse& response);
        void handleError(const HttpRequest& request, const RouteDecision& result, HttpResponse& response);
        const ServerConfig* selectServer(const HttpRequest& request, const Listener& listener) const;
        // to enable calling from CGI object
        static std::string buildPath(const std::string& requestPath, const LocationConfig& location);

    private:

        const LocationConfig* matchLocation(const std::string& path, const ServerConfig& server) const;
        bool isMethodAllowed(const std::string &method, const LocationConfig &location) const;
        
        bool readHtmlPages(const std::string& path, std::string& content);
        std::string getErrorReason(int code) const;
        void setAllowedMethodHeader(const LocationConfig& location, HttpResponse& response);

        bool hasCGIextension(const std::string & path, const std::map<std::string, std::string> & extensionList, std::string & ext) const;
                
        void handleCGI(const HttpRequest& request, HttpResponse& response, RouteDecision& result, const std::string& extension, ClientConnection& client);
        void handleGet(const HttpRequest& request, HttpResponse& response, RouteDecision& result);
        void handlePost(const HttpRequest& request, HttpResponse& response, RouteDecision& result);
        void handleDelete(const HttpRequest& request, HttpResponse& response, RouteDecision& result);
        
        void handleRedirect(const HttpRequest& request, int statusCode, const std::string& targetUrl, HttpResponse& response);
        std::string getRedirectCode(int code) const;
        
        void setResponseBody(HttpResponse& response, const std::string& path, int statusCode,
            const std::string& reason);
        
        int parserErrorToStatus(HttpRequestError error);


        bool isBodySizeAllowed(const HttpRequest& request, const ServerConfig& server, const LocationConfig& location) const;
        bool isHeaderSizeAllowed(const HttpRequest& request, const ServerConfig& server, const LocationConfig& location) const;

        //common methods helpers
        void handleFileError(int errorNum, const HttpRequest& request, HttpResponse& response, RouteDecision& result);
        void fillResponse(
            HttpResponse& response, 
            const std::string& version, 
            int statusCode, 
            const std::map<std::string, std::string>& headers, 
            const std::string& body);
        std::string getReasonPhrase(int statusCode);
        std::string getContentType(const std::string& path);

        //GET method helpers
        void serveFile(const HttpRequest& request, HttpResponse& response, RouteDecision& result, std::string& path);
        void serveDir(const HttpRequest& request, HttpResponse& response, RouteDecision& result, std::string& path);
        std::string generateFilesList(std::string& dirPath, std::string& requestPath);
        std::string urlEncodePath(std::string path);
        std::string escapeHtml(std::string input);

        //POST method helpers
        std::string getFileFormat(const std::string& type);
        void handleMultipart(const HttpRequest& request, HttpResponse& response, RouteDecision& result);
        void handleMultipartError(const HttpRequest& request, HttpResponse& response, RouteDecision& result);
        void handlePart(const std::string& part, const HttpRequest& request, HttpResponse& response, RouteDecision& result);
        void saveFile(const std::string& format, const std::string& data, const HttpRequest& request, HttpResponse& response, RouteDecision& result);
};
