/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Router.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: damohame <damohame@student.42berlin.d>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/11/06 22:50:16 by damohame          #+#    #+#             */
/*   Updated: 2026/11/10 00:54:40 by damohame         ###   ########.fr       */
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
        result.type = RouteDecision::ROUTE_REDIRECT;
        result.errorCode = 0;
        return result;
    }
    
    // 5- CGI 
    // example:
    // ".py" -> "/usr/bin/python3"
    // ".php" -> "/usr/bin/php-cgi"
    // to be later implemented

    
    // 6- method handler one request at a time
    if (request.getMethod() == "GET")
        handleGet(request, response);
    else if (request.getMethod() == "POST")
        handlePost(request, response);
    else if (request.getMethod() == "DELETE")
        handleDelete(request, response);

    return result;
}


void Router::handleError(const HttpRequest& request, const RouteDecision& result, HttpResponse& response)
{
    (void)request;
    (void)result;
    (void)response;
    // later
}

void Router::handleGet(const HttpRequest& request, HttpResponse& response)
{
    (void)request;
    (void)response;
    //to be implemented: Get handler
}
void Router::handlePost(const HttpRequest& request, HttpResponse& response)
{
    (void)request;
    (void)response;
    //to be implemeneted:  post handler;
}
void Router::handleDelete(const HttpRequest& request, HttpResponse& response)
{
    (void)request;
    (void)response;
    // to be implemented: Delete handler
}
