#include "CGI.hpp"

CGI::CGI(const HttpRequest & request, HttpResponse & response, const LocationConfig & location, const std::string & cgi_extension)
:   extension(cgi_extension),
    interpreterPath(location.cgi_handlers.find(cgi_extension)->second),
    response(response),
    responseReady(false)
{
    setArgv(request, location);
    setEnvp(request);
}

void CGI::resolveScriptPath(const HttpRequest & request, const LocationConfig & location)
{
    scriptPath += location.root;
    if (scriptPath.size() && scriptPath[scriptPath.size()-1] != '/')
        scriptPath += '/';
    std::string tempPath = request.getPath();
    if (tempPath.size() && tempPath[0] == '/')
        scriptPath.append(tempPath.substr(1));
    else
        scriptPath += tempPath;
}

void CGI::setArgv(const HttpRequest & request, const LocationConfig & location)
{
    resolveScriptPath(request, location);

    argv += interpreterPath + " " + interpreterPath + " " + scriptPath;
}

void CGI::setEnvp(const HttpRequest & request/*, std::vector<std::string> envp_required*/) /*in case other cgi scripts will be supported*/
{
    std::string tempEnvp[3];

    // size_t s = envp_required.size();
    //for (size_t i = 0; i < s; i++) {
    tempEnvp[0] = "\"REQUEST_METHOD=\"" + request.getMethod() + "\"";
    tempEnvp[1] = "\"CONTENT_LENGTH=\"" + request.getMethod() + "\"";
    tempEnvp[2] = "\"PATH=\"" + request.getMethod() + "\"";

    envp.push_back(tempEnvp[0].c_str());
    envp.push_back(tempEnvp[1].c_str());
    envp.push_back(tempEnvp[2].c_str());
    envp.push_back(NULL);
    //}
}

void CGI::processOutput()
{
    // readOutput();
    // buildResponse();
        // responseReady = true;
}

bool CGI::isResponseReady()
{
    return responseReady;
}