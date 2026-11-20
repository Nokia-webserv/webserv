#include "CGI.hpp"
#include "Router.hpp"
#include <iostream>

CGI::CGI(const HttpRequest & request, HttpResponse & response, const LocationConfig & location, const std::string & cgi_extension, CGI*& myPtrInMyClient)
:   extension(cgi_extension),
    interpreterPath(location.cgi_handlers.find(cgi_extension)->second),
    response(response),
    myPtrInMyClient(myPtrInMyClient), // NULL
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
    scriptPath = Router::buildPath(request.getPath(), location);
    std::cout << "Script path: " << scriptPath << "\n";

    argv += interpreterPath + " " + interpreterPath + " " + scriptPath;
    std::cout << "argv=\"" << argv << "\"\n";
}

// prepares environmental arguments for execve (implemented as per current needs)
void CGI::setEnvp(const HttpRequest & request/*, std::vector<std::string> envp_required*/) /*in case other cgi scripts will be supported*/
{
    // size_t s = envp_required.size();
    //for (size_t i = 0; i < s; i++) {
    envp_str.push_back ("\"REQUEST_METHOD=" + request.getMethod() + "\"");
    envp_str.push_back("\"PATH=" + request.getPath() + "\"");
    if (request.getHeaders().find("content-length") != request.getHeaders().end())
        envp_str.push_back("\"CONTENT_LENGTH=" + request.getHeaders().find("content-length")->second + "\"");
    
    envp.push_back(envp_str[0].c_str());
    envp.push_back(envp_str[1].c_str());
    if (envp_str.size() == 3)
        envp.push_back(envp_str[2].c_str());
    envp.push_back(NULL);
    //}
    std::cout << "envp: " << envp[0] << " " << envp[1]; if (envp[2]) std::cout << " " << envp[2]; std::cout << "\n";
}

void CGI::runScript()
{
    // create pipes, fork, etc, and run execve
    // save cgi_fd (= parent listening fd)
    std::cout << "Running CGI script (not implemented)\n";
    // if run successfully -> record in client -> add to poll
    myPtrInMyClient = this;
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

int CGI::getFd() const
{
    return cgi_fd;
}