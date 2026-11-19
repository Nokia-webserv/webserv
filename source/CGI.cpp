#include "CGI.hpp"
#include "Router.hpp"
#include <iostream>
#include <ctime>
#include <poll.h>

CGI::CGI(const HttpRequest & request, HttpResponse & response, RouteDecision decision, const std::string& cgi_extension, 
    CGI*& myPtrInMyClient, std::map<int, int>& pendingPIDs)
:   extension(cgi_extension),
    interpreterPath(decision.location->cgi_handlers.find(cgi_extension)->second),
    request(request),
    response(response),
    decision(decision),
    myPtrInMyClient(myPtrInMyClient), // NULL
    pendingPIDs(pendingPIDs),
    pid(-2),
    responseReady(false),
    error_code(this->decision.errorCode),
    termination_status(false),
    startTime(std::time(NULL))
{
    pipeRead[0] = -1, pipeRead[1] = -1; 
    pipeWrite[0] = -1, pipeWrite[1] = -1;
    setArgv(request, *decision.location);
    setEnvp(request);
    std::cout << "Building CGI object\n";
}

CGI::~CGI()
{
    std::cout << "Destructing CGI object\n";
    if (pid != -2 && !termination_status)
    {
        kill(pid, SIGKILL);
        if (reapProcess())
            pendingPIDs.erase(pid);
    }

    cleanUp();
}

void CGI::setArgv(const HttpRequest & request, const LocationConfig & location)
{
    // resolves script path according to specific subject requirements
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
    
    envp.push_back(const_cast<char*>(envp_str[0].c_str()));
    envp.push_back(const_cast<char*>(envp_str[1].c_str()));
    if (envp_str.size() == 3)
        envp.push_back(const_cast<char*>(envp_str[2].c_str()));
    envp.push_back(NULL);
    //}
}

// cleans up a finished child process so the operating system can release the resources it was still holding
bool CGI::reapProcess()
{
    std::cout << "Reaping child process\n";
    int status;
    // checks subprocess status, reapes zombie process (terminated but awawiting parent's read)
    int result = waitpid(pid, &status, WNOHANG); // non-blocking mode
    if (result == pid)
        termination_status = true;
    else
    {
        termination_status = false;
        // records unreaped pid in the server
        pendingPIDs.insert(std::make_pair(pid, 0)); // ignores duplicates
    }
    return termination_status;
}

void CGI::cleanUp() 
{
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

    // erases from the client
    myPtrInMyClient = NULL;
    std::cout << "Cleaning up CGI object\n";
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
        close(pipeRead[0]); // after dup no pipe end is needed
        close(pipeRead[1]);
        close(pipeWrite[1]);
        close(pipeWrite[0]);

        execve(scriptPath.c_str(), &argv[0], &envp[0]); // executes the CGI script and terminates child process
        
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
    // records itslef in client -> adds to poll
    myPtrInMyClient = this;
    std::cout << "Process forked successfully. PID:" << pid << "\n";
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
    // std::cout << "process Output not implemented\n";
    // error_code = 500;
    char buf[100];
    read(pipeWrite[0], buf, 100);
    std::cout << buf << "\n";
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

int CGI::getErrorCode() const
{
    return error_code;
}

int CGI::getFd() const
{
    return pipeWrite[0]; // fd used in poll
}