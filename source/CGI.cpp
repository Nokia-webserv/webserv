#include "CGI.hpp"
#include "Router.hpp"
#include <iostream>
#include <ctime>
#include <poll.h>
#include <cstdlib>
#include "ClientConnection.hpp"

extern char **environ;

CGI::CGI(const HttpRequest & request, HttpResponse & response, RouteDecision decision, const std::string& cgi_extension, 
    ClientConnection& client)
:   extension(cgi_extension),
    interpreterPath(decision.location->cgi_handlers.find(cgi_extension)->second),
    request(request),
    response(response),
    decision(decision),
    myPtrInMyClient(client.cgi), // NULL
    myClient(client),
    pendingPIDs(client.pendingPIDs),
    childPid(-2),
    bytesSent(0),
    responseReady(false),
    error_code(this->decision.errorCode),
    termination_status(false),
    startTime(std::time(NULL))
{
    pipeRead[0] = -1, pipeRead[1] = -1; 
    pipeWrite[0] = -1, pipeWrite[1] = -1;
    cgiInputBuffer.clear();
    cgiInputBuffer.append(request.getBody());
    setArgv(request, *decision.location);
    setEnvp(request, *decision.server);
}

CGI::~CGI()
{
    if (childPid != -2 && !termination_status)
    {
        kill(childPid, SIGKILL);
        if (reapProcess())
            pendingPIDs.erase(childPid);
    }

    cleanUp();
}

void CGI::setArgv(const HttpRequest & request, const LocationConfig & location)
{
    // resolves script path according to specific subject requirements
    scriptPath = Router::buildPath(request.getPath(), location);
    file = scriptPath.substr(scriptPath.find_last_of("/") + 1); // script.py

    argv.push_back(const_cast<char*>(interpreterPath.c_str()));
    argv.push_back(const_cast<char*>(file.c_str()));
    argv.push_back(NULL);
}

// extracts env path from environ
std::string getEnvPath()
{
    const std::string prefix = "PATH=";
    for (size_t i = 0; environ[i]; ++i)
    {
        std::string entry(environ[i]);
        if (entry.compare(0, prefix.size(), prefix) == 0)
            return entry;
    }
    return "PATH=/usr/bin:/bin";
}

std::string matchServerName(const std::map<std::string, std::string>& headers, const std::vector<std::string> serverNames)
{
    std::map<std::string, std::string>::const_iterator it = headers.find("host");
    if (it == headers.end())
        return "server";

    std::string hostHeader = it->second;
    
    // strip the port: "example.com:8080" -> "example.com"
    std::string host = hostHeader;
    size_t colon = host.find(':');
    if (colon != std::string::npos)
        host = host.substr(0, colon);
    host = strTolower(host);

    for (size_t i = 0; i < serverNames.size(); i++)
    {
        if (strTolower(serverNames[i]) == host)
            return serverNames[i];
    }

    return serverNames.size() > 0 ? serverNames[0] : "server";
}

int matchServerPort(const std::map<std::string, std::string>& headers, const std::vector<Listen>& hostPorts)
{
    std::map<std::string, std::string>::const_iterator it = headers.find("host");
    if (it == headers.end())
        return 80;

    std::string hostHeader = it->second;
    // extract the port from "example.com:8080"
    int port = 80;                          // no port in Host means HTTP default
    size_t colon = hostHeader.find(':');
    if (colon != std::string::npos)
    {
        std::string strPort = hostHeader.substr(colon + 1);
        IntLongOverflow converted = strToIntLongOverflow("int", strPort);
        if (converted.overflow == 1)
            port = converted.number;
    }

    for (size_t i = 0; i < hostPorts.size(); i++)
        if (hostPorts[i].port == port)
            return port;                    // matched

    if (!hostPorts.empty())
        return hostPorts[0].port;              // no match: first port
    return port;                            // no ports configured: use what the client sent
}

// prepares environmental arguments for CGI process
void CGI::setEnvp(const HttpRequest & request, const ServerConfig& server)
{
    typedef std::map<std::string, std::string> HeaderMap;
    const HeaderMap &headers = request.getHeaders();

    envp_str.clear();
    envp.clear();

    // 1. meta-variables, set explicitly
    envp_str.push_back("REQUEST_METHOD=" + request.getMethod());
    envp_str.push_back("QUERY_STRING=" + request.getRawQuery());
    envp_str.push_back("SCRIPT_NAME=" + request.getPath());
    envp_str.push_back("PATH_INFO="   + request.getPath());
    envp_str.push_back("SCRIPT_FILENAME=" + scriptPath);
    envp_str.push_back("SERVER_PROTOCOL=" + request.getVersion());
    envp_str.push_back("SERVER_NAME=" + matchServerName(request.getHeaders(), server.server_names));
    envp_str.push_back("SERVER_PORT=" + toString(matchServerPort(request.getHeaders(), server.hostPorts)));
    envp_str.push_back("REQUEST_URI=" + request.getPath() + (request.getQuery().empty() ? "" : "?" + request.getQuery()));
    envp_str.push_back("REMOTE_ADDR=" + myClient.getIp());
    envp_str.push_back("REMOTE_PORT=" + toString(myClient.getPort()));
    envp_str.push_back(getEnvPath());

    // 2. CONTENT_LENGTH / CONTENT_TYPE (no HTTP_ prefix)
    HeaderMap::const_iterator it = headers.find("content-length");
    if (it != headers.end())
        envp_str.push_back("CONTENT_LENGTH=" + it->second);
    else
        envp_str.push_back("CONTENT_LENGTH=" + toString(request.getBody().size()));

    it = headers.find("content-type");
    if (it != headers.end())
        envp_str.push_back("CONTENT_TYPE=" + it->second);

    // 3. every other header becomes HTTP_NAME=value
    for (it = headers.begin(); it != headers.end(); ++it)
    {
        std::string name = it->first;
        for (size_t i = 0; i < name.size(); i++)
        {
            if (name[i] == '-')
                name[i] = '_';
            else
                name[i] = std::toupper(static_cast<unsigned char>(name[i]));
        }
        if (name == "CONTENT_TYPE" || name == "CONTENT_LENGTH" || name == "PROXY")
            continue;
        envp_str.push_back("HTTP_" + name + "=" + it->second);
    }

    // 4. build the char* array only now, when envp_str is final
    for (size_t i = 0; i < envp_str.size(); i++)
        envp.push_back(const_cast<char *>(envp_str[i].c_str()));
    envp.push_back(NULL);
}

// cleans up a finished child process so the operating system can release the resources it was still holding
bool CGI::reapProcess()
{
    int status;
    // checks subprocess status, reapes zombie process (terminated but awawiting parent's read)
    int result = waitpid(childPid, &status, WNOHANG); // non-blocking mode
    if (result == childPid)
        termination_status = true;
    else
    {
        termination_status = false;
        // records unreaped pid in the server
        pendingPIDs.insert(std::make_pair(childPid, 0)); // ignores duplicates
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
            std::exit(EXIT_FAILURE);
        }
        close(pipeRead[0]); // after dup no pipe end is needed
        close(pipeRead[1]);
        close(pipeWrite[1]);
        close(pipeWrite[0]);
        
        // enables relative path file access (as per subject requirement)
        dir = scriptPath.substr(0, scriptPath.find_last_of("/"));
        if (chdir(dir.c_str()) == -1)
            std::exit(EXIT_FAILURE);

        execve(interpreterPath.c_str(), &argv[0], &envp[0]); // executes the CGI script and terminates child process
   
        /* execve error */
        // execve terminates child process. reaching this line indicates execve failure
        // handling is same as for dup error
        std::exit(EXIT_FAILURE);
    }
    
    /* parent's path */

    // closes unused pipe ends and overrides saved fds to avoid double close
    close(pipeRead[0]);
    pipeRead[0] = -1;
    close(pipeWrite[1]);
    pipeWrite[1] = -1;

    childPid = pid;
    // records itslef in client -> adds to poll
    myPtrInMyClient = this;

    // sets fds to non-blocking mode
    for (int i = 0; i < 2; i++)
    {
        int fd = !i ? pipeWrite[0] : pipeRead[1];

        if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) // set new set of flags + check success
        {
            // crafts error msg
            std::ostringstream fd_ss;
            fd_ss << fd;
            std::string error_msg = "Non-blocking mode for fd #" + fd_ss.str() + " failed";
            std::cerr << error_msg << std::endl;
            // sets error code
            error_code = 500;
            return;
        }
    }

    // if no body to write -> closes immediately
    if (cgiInputBuffer.empty())
        closeReading();
}

// writes to CGI input
void CGI::provideInput()
{
    ssize_t bts = write(pipeRead[1], cgiInputBuffer.c_str() + bytesSent, cgiInputBuffer.size() - bytesSent);
    if (bts > 0)
        bytesSent += bts;
    if (bytesSent == cgiInputBuffer.size())
        closeReading();
}

// reads until EOF then builds response
void CGI::processOutput()
{
    int readbts = readOutput();
    if (readbts == -1)
    {
        error_code = 500;
        return;
    }
    else if (readbts > 0)
        return;
    
    if (cgiOutputBuffer == D_ERROR_MSG)
    {
        error_code = 500;
        return;
    }
    // builds response
    if (!buildResponse())
        error_code = 500;
}

// handles optional status header (present if other than 200)
bool buildFirstLine(std::string str, HttpResponse& response)
{
    size_t pos = str.find(' ');
    std::string codeStr = (pos == std::string::npos) ? str : str.substr(0, pos);

    IntLongOverflow result = strToIntLongOverflow("int", codeStr);
    if (result.overflow != 1)
        return false;
    response.setStatusCode(result.number);

    response.setReasonPhrase(pos == std::string::npos ? "" : str.substr(pos + 1));
    return true;
}

bool buildHeaders(std::string str, HttpResponse& response)
{
    size_t pos = str.find(':');
    if (pos == std::string::npos)
        return false;

    std::string key = str.substr(0, pos);
    std::string value = str.substr(pos + 1);
    trimStr(key);
    trimStr(value);

    // handles optional status header
    if (strTolower(key) == "status")
        return buildFirstLine(value, response);

    response.setHeader(key, value);
    return true;
}

bool CGI::buildResponse()
{
    // server should assume 200 at the beginning as per RFC
    response.setStatusCode(200);
    response.setReasonPhrase("OK");
    response.setVersion(request.getVersion());

    while (true)
    {
        size_t pos = cgiOutputBuffer.find('\n');
        if (pos == std::string::npos)
            return false;                       // no blank line: malformed output

        std::string line = cgiOutputBuffer.substr(0, pos);
        cgiOutputBuffer.erase(0, pos + 1);

        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);        // strip the optional \r

        if (line.empty())
            break;                              // blank line: headers are over

        if (!buildHeaders(line, response))
            return false;
    }

    std::string body = cgiOutputBuffer;         // everything left is the body
    response.setBody(body);

    if (!response.checkHeader("content-length"))
        response.setHeader("Content-Length", toString(body.size()));
    setResponseReady();
    return true;
}

int CGI::readOutput()
{
    char	buffer[4096];
	ssize_t	bytesReceived;

	bytesReceived = read(pipeWrite[0], buffer, sizeof(buffer));
	if (bytesReceived > 0)
	{
		cgiOutputBuffer.append(buffer, static_cast<std::string::size_type>(bytesReceived));
		return (1);
	}
	// EOF detection
    else if (bytesReceived == 0)
        return 0;
    return (-1);
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

int CGI::getFd(char end) const
{
    return end == 'w' ? pipeWrite[0] : pipeRead[1];
}

bool CGI::isReadOpen() const
{
    int fd = pipeRead[1];
    return fd == -1 ? false : true;
}

void CGI::closeReading()
{
    int fd = pipeRead[1];
    if (fd != -1)
    {
        close(fd);
        pipeRead[1] = -1;
    }
}