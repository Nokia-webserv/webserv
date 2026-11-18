#include "CGI.hpp"
#include "Router.hpp"
#include <iostream>
#include <ctime>
#include <poll.h>

CGI::CGI(const HttpRequest & request, HttpResponse & response, RouteDecision decision, const std::string& cgi_extension, CGI*& myPtrInMyClient)
:   extension(cgi_extension),
    interpreterPath(decision.location->cgi_handlers.find(cgi_extension)->second),
    request(request),
    response(response),
    myPtrInMyClient(myPtrInMyClient), // NULL
    decision(decision),
    pid(-2),
    responseReady(false),
    error_code(this->decision.errorCode),
    termination_status(none),
    startTime(std::time(NULL))
{
    pipeRead[0] = -1, pipeRead[1] = -1; 
    pipeWrite[0] = -1, pipeWrite[1] = -1;
    setArgv(request, *decision.location);
    setEnvp(request);
}

CGI::~CGI()
{
    if (pid != -2 && termination_status != terminated)
        terminateChildProcess();
    if (pid != -2 && termination_status != terminated)
        std::cerr << "Failure to correctly terminate child process. PID: " << pid << "\n";
    cleanUp(false);
}

// resolves path according to configs
void CGI::resolveScriptPath(const HttpRequest & request, const LocationConfig & location)
{
    scriptPath += location.root;
    // ensures slash at the beginning
    if (scriptPath.empty() || scriptPath[scriptPath.size()-1] != '/')
        scriptPath += '/'; 
    std::string tempPath = request.getPath();
    // eliminates double-slash
    if (tempPath.size() && tempPath[0] == '/')
        scriptPath.append(tempPath.substr(1));
    else 
        scriptPath += tempPath;
}

void CGI::setArgv(const HttpRequest & request, const LocationConfig & location)
{
    scriptPath = Router::buildPath(request.getPath(), location);
    std::cout << "Script path: " << scriptPath << "\n";

    argv.push_back(const_cast<char*>(interpreterPath.c_str()));
    argv.push_back(const_cast<char*>(interpreterPath.c_str()));
    argv.push_back(const_cast<char*>(scriptPath.c_str()));
    argv.push_back(NULL);
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
}

void CGI::terminateChildProcess()
{
    // sends immediate termination signal (doesn't guarrantee termination)
    kill(pid, SIGKILL);
    int status;
    // checks subprocess status, reapes zombie process (terminated but awawiting parent's read), non-blocking mode
    int result = waitpid(pid, &status, WNOHANG);
    if (result != pid)
    {
        /* process termination error */
        if (result == -1)
        {
            switch (errno)
            {
                case EINTR: termination_status = interrupted; return;
                case ECHILD: termination_status = reapedAlready; return;
                default: termination_status = otherError; return;
            }
        }
        termination_status = stillRunning;
        return;
    }
    termination_status = terminated;
}

void CGI::cleanUp(bool eraseFromMap) 
{
    // allows correct destructor call from ClientConnection
    /* if (eraseFromMap)
    {
        std::map<int, CGI*>::iterator int_cgi = clientsCGImap.find(pipeWrite[0]);
        if (int_cgi != clientsCGImap.end())
            clientsCGImap.erase(int_cgi->first);
    } */

    // closes all open pipe ends
    int* pipe_ends[4] = {&pipeRead[0], &pipeRead[1], &pipeWrite[0], &pipeWrite[1]};
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
    /* creates pipes and assigns fds */
    if (pipe(pipeRead) == -1 || pipe(pipeWrite) == -1)
    {
        error_code = 500;
        return;
    }

    /* Test 2: pipe error */
    // the following block will replace the block above
    /* std::cout << "=== TEST: Imitating pipe error ===\n";
    error_code = 500;
    return; */

    /* clones current process */
    pid_t pid = fork();
    
    /* fork error */
    if (pid == -1) // no child process created
    {
        error_code = 500;
        return;
    }

    /* child's path */
    else if (pid == 0) // child sees pid as 0
    {
        // redirects pipe fds to standard streams (CGI can use only stdin and stdout)
        if (dup2(pipeRead[0], STDIN_FILENO) == -1 || dup2(pipeWrite[1], STDOUT_FILENO) == -1)
        {
            /* dup error */
            std::string buf = D_ERROR_MSG; // specific error message
            // writes to parent listening end -> triggers POLLIN -> parent reads the output and handles the error
            write(pipeWrite[1], buf.c_str(), buf.size());
            // freezes child process to disallow further code execution 
            while (true)
                poll(NULL, 0, -1); 
        }

        /* Test 3: error in child proces */
        // the following block will replace the block above
        /* std::cout << "=== TEST: Imitating error in subprocess ===\n";
        std::string buf1 = D_ERROR_MSG;
        write(pipeWrite[1], buf1.c_str(), buf1.size());
        while (true)
            poll(NULL, 0, -1);
        std::cout << "Must not reach here\n";  */

        close(pipeRead[0]); // after dup no pipe end is needed
        close(pipeRead[1]);
        close(pipeWrite[1]);
        close(pipeWrite[0]);

        //execve(scriptPath.c_str(), &argv[0], &envp[0]); // executes the CGI script and terminates child process

        /* execve error */
        // execve terminates child process. reaching this line indicates execve failure
        // handling path is same as for dup error
        std::string buf = D_ERROR_MSG; 
        write(STDOUT_FILENO, buf.c_str(), buf.size());
        while (true)
            poll(NULL, 0, -1);
    }
    
    /* parent's path */
    // closes unused pipe ends and overrides saved fds to avoid double close
    close(pipeRead[0]);
    pipeRead[0] = -1;
    close(pipeWrite[1]);
    pipeWrite[1] = -1;
    this->pid = pid;
    // if run successfully -> record in client -> add to poll
    myPtrInMyClient = this;
}

// reads until EOF then builds response
void CGI::processOutput()
{
    // readOutput();
        // check for error, change error code, return
    // after EOF:
    // buildResponse();
        // responseReady = true;
    // cleanup cgi object: close fd, remove from cgi map
    std::cout << "process Output not implemented\n";
    error_code = 500;
}

bool CGI::isResponseReady() const
{
    return responseReady;
}

void CGI::setResponseReady()
{
    responseReady = true;
}

time_t CGI::getStartTime() const
{
    return startTime;
}

terminationStatus CGI::getTerminationStatus() const
{
    return termination_status;
}

int CGI::getErrorCode() const
{
    return error_code;
}

int CGI::getFd() const
{
    return pipeWrite[1]; // fd used in poll
}