#include "CGI.hpp"
#include <iostream>

CGI::CGI(const HttpRequest & request, HttpResponse & response, const LocationConfig & location, const std::string & cgi_extension, std::map<int, CGI*>& cgiMap)
:   extension(cgi_extension),
    interpreterPath(location.cgi_handlers.find(cgi_extension)->second),
    response(response),
    clientsCGImap(cgiMap),
    cgi_fd(-1),
    responseReady(false)
{
    std::cout << "Constructing CGI object\n";
    setArgv(request, location);
    setEnvp(request);
}

// resolves path according to configs
void CGI::resolveScriptPath(const HttpRequest & request, const LocationConfig & location)
{
    scriptPath += location.root;
    if (scriptPath.empty() || scriptPath[scriptPath.size()-1] != '/')
        scriptPath += '/'; // ensures slash at the beginning
    std::string tempPath = request.getPath();
    if (tempPath.size() && tempPath[0] == '/')
        scriptPath.append(tempPath.substr(1));
    else // eliminates double-slash
        scriptPath += tempPath;
}

void CGI::setArgv(const HttpRequest & request, const LocationConfig & location)
{
    resolveScriptPath(request, location);
    std::cout << "Script path: " << scriptPath << "\n";

    argv += interpreterPath + " " + interpreterPath + " " + scriptPath;
    std::cout << "argv=\"" << argv << "\"\n";
}

// prepares environmental arguments for execve (implemented as per current needs)
void CGI::setEnvp(const HttpRequest & request/*, std::vector<std::string> envp_required*/) /*in case other cgi scripts will be supported*/
{
    std::string tempEnvp[3];

    // size_t s = envp_required.size();
    //for (size_t i = 0; i < s; i++) {
    tempEnvp[0] = "\"REQUEST_METHOD=" + request.getMethod() + "\"";
    tempEnvp[1] = "\"PATH=" + request.getPath() + "\"";
    if (request.getHeaders().find("content-length") != request.getHeaders().end())
        tempEnvp[2] = "\"CONTENT_LENGTH=" + request.getHeaders().find("content-length")->second + "\"";
    else
        tempEnvp[2] = "";
    
    envp.push_back(tempEnvp[0].c_str());
    envp.push_back(tempEnvp[1].c_str());
    if (!tempEnvp[2].empty())
        envp.push_back(tempEnvp[2].c_str());
    envp.push_back(NULL);
    //}
    std::cout << "envp: " << envp[0] << " " << envp[1]; if (envp[2]) std::cout << " " << envp[2]; std::cout << "\n";
}

void CGI::runScript()
{
    // create pipes, fork, etc, and run execve
    // save cgi_fd (= parent listening fd)
    // record parent listening fd in cgiMap (to add it to poll)
    std::cout << "Running CGI script (not implemented)\n";
}

// reads until EOF then builds response
void CGI::processOutput()
{
    // readOutput();
    // after EOF:
    // buildResponse();
        // responseReady = true;
    // cleanup cgi object: close fd, remove from cgi map
}

bool CGI::isResponseReady() const
{
    return responseReady;
}