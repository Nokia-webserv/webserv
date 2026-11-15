#include "CGI.hpp"
#include <iostream>
#include <ctime>
#include <poll.h>

CGI::CGI(const HttpRequest & request, HttpResponse & response, RouteDecision& decision, const std::string& cgi_extension, std::map<int, CGI*>& cgiMap)
:   extension(cgi_extension),
    interpreterPath(decision.location->cgi_handlers.find(cgi_extension)->second),
    request(request),
    response(response),
    decision(decision),
    clientsCGImap(cgiMap),
    pid(-2),
    responseReady(false),
    error_code(decision.errorCode),
    startTime(std::time(NULL)),
    termination_status(none)
{
    pipeRead[0] = -1, pipeRead[1] = -1; 
    pipeWrite[0] = -1, pipeWrite[1] = -1;
    setArgv(request, *decision.location);
    setEnvp(request);
}

CGI::~CGI()
{
    if (termination_status != terminated)
        termianteChildProcess();
    if (termination_status != terminated)
        std::cerr << "Failure to correctly terminate child process. PID: " << pid << "\n";
    cleanUp(false, getPipes());
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

    argv.push_back(const_cast<char*>(interpreterPath.c_str()));
    argv.push_back(const_cast<char*>(interpreterPath.c_str()));
    argv.push_back(const_cast<char*>(scriptPath.c_str()));
    argv.push_back(NULL);
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
    
    envp.push_back(const_cast<char*>(tempEnvp[0].c_str()));
    envp.push_back(const_cast<char*>(tempEnvp[1].c_str()));
    if (!tempEnvp[2].empty())
        envp.push_back(const_cast<char*>(tempEnvp[2].c_str()));
    envp.push_back(NULL);
    //}
    std::cout << "envp: " << envp[0] << " " << envp[1]; if (envp[2]) std::cout << " " << envp[2]; std::cout << "\n";
}

void CGI::termianteChildProcess()
{
    kill(pid, SIGKILL);
    int status;
    int result = waitpid(pid, &status, WNOHANG);
    if (result != pid)
    {
        if (result == -1)
        {
            switch (errno)
            {
                case EINTR: termination_status = interrupred; return;
                case ECHILD: termination_status = reapedAlready; return;
                default: termination_status = otherError; return;
            }
        }
        termination_status = stillRunning;
        return;
    }
    termination_status = terminated;
}

void CGI::cleanUp(bool eraseFromMap, int* pipe_ends[4]) 
{
    if (eraseFromMap)
    {
        std::map<int, CGI*>::iterator int_cgi = clientsCGImap.find(pipeWrite[0]);
        if (int_cgi != clientsCGImap.end())
            clientsCGImap.erase(int_cgi->first);
    }

    for (int i = 0; i < 4; i++)
    {
        if (*pipe_ends[i] != -1)
        {
            close(*pipe_ends[i]);
            *pipe_ends[i] = -1;
        }
    }
}

void CGI::runScript()
{
    if (pipe(pipeRead) == -1 || pipe(pipeWrite) == -1)
    {
        error_code = 500;
        return;
    }

    pid_t pid = fork();
    if (pid == -1)
    {
        error_code = 500;
        return;
    }
    else if (pid == 0)
    {
        if (dup2(pipeRead[0], STDIN_FILENO) == -1 || dup2(pipeWrite[1], STDOUT_FILENO) == -1)
        {
            std::string buf = D_ERROR_MSG;
            write(pipeWrite[1], buf.c_str(), buf.size());
            while (true)
                poll(NULL, 0, -1);
        }
        close(pipeRead[0]);
        close(pipeRead[1]);
        close(pipeWrite[1]);
        close(pipeWrite[0]);
        execve(scriptPath.c_str(), &argv[0], &envp[0]);
        std::string buf = D_ERROR_MSG; // define
        write(STDOUT_FILENO, buf.c_str(), buf.size());
        while (true)
            poll(NULL, 0, -1);
    }
    close(pipeRead[0]);
    pipeRead[0] = -1;
    close(pipeWrite[1]);
    pipeWrite[1] = -1;
    this->pid = pid; 
    clientsCGImap[pipeWrite[0]] = this;
}

// reads until EOF then builds response
int CGI::processOutput()
{
    // readOutput();
        // check for error, change error code, return
    // after EOF:
    // buildResponse();
        // responseReady = true;
    // cleanup cgi object: close fd, remove from cgi map
}

bool CGI::isResponseReady() const
{
    return responseReady;
}

void CGI::setResponseReady()
{
    responseReady = true;
}

int** CGI::getPipes()
{
    int* pipe_ends[4] = {&pipeRead[0], &pipeRead[1], &pipeWrite[0], &pipeWrite[1]};
    return pipe_ends;
}

time_t CGI::getStartTime() const
{
    return startTime;
}

terminationStatus CGI::getTerminationStatus() const
{
    return termination_status;
}