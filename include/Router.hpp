/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Router.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: damohame <damohame@student.42berlin.d>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/11/06 22:25:55 by damohame          #+#    #+#             */
/*   Updated: 2026/11/11 14:06:18 by damohame         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include "ServerConfig.hpp"
#include "HttpRequest.hpp"
#include "Listener.hpp"
#include "HttpResponse.hpp"
#include <string>

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

        RouteDecision route(const HttpRequest& request, const Listener& listener, HttpResponse& response);

    private:

        const LocationConfig* matchLocation(const std::string& path, const ServerConfig& server) const;
        bool isMethodAllowed(const std::string &method, const LocationConfig &location) const;
        const ServerConfig* selectServer(const HttpRequest& request, const Listener& listener) const;
        
        bool readErrorPages(const std::string& path, std::string& content);
        std::string getErrorReason(int code) const;
        void setAllowedMethodHeader(const LocationConfig& location, HttpResponse& response);
        
        void handleError(const HttpRequest& request, const RouteDecision& result, HttpResponse& response);
        
        void handleGet(const HttpRequest& request, HttpResponse& response, const LocationConfig& location);
        void handlePost(const HttpRequest& request, HttpResponse& response, const LocationConfig& location);
        void handleDelete(const HttpRequest& request, HttpResponse& response, const LocationConfig& location);
        
};

