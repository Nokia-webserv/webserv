#pragma once

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "LocationConfig.hpp"
#include "Router.hpp"
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <cerrno>

#define D_ERROR_MSG "CGI_ERROR" // specific error message sent from child (CGI) to parent (server)

enum terminationStatus
{
    none,
    terminated,
    stillRunning,
    interrupted,
    reapedAlready,
    otherError
};

/* This class represents each isntance of a CGI script in http request */
class CGI {
    public:
        CGI(const HttpRequest& request, HttpResponse& response, RouteDecision decision, const std::string& cgi_extension, std::map<int, CGI*>& cgiMap);
        ~CGI();
        
        const std::string extension;
        const std::string interpreterPath; // e.g. /usr/bin/python3
        std::map<int, CGI*>& clientsCGImap; // contains all actively running CGI scripts per client
        const HttpRequest request;
        HttpResponse response;
        RouteDecision decision;
        
        int getFd() const;
        int getErrorCode() const;
        time_t getStartTime() const;
        terminationStatus getTerminationStatus() const;
        
        void setResponseReady(); // marks response ready for placing in send queue
        bool isResponseReady() const;
        
        void runScript(); // runs CGI
        void processOutput(); // processes CGI result
        void terminateChildProcess();
        void cleanUp(bool easeFromMap);

    private:
        int pipeRead[2]; // pipe where CGI reads from
        int pipeWrite[2]; // pipe where CGI writes to
        int pid;
        std::string scriptPath; // e.g. www/scripts/script.py
        std::vector<char*> argv; // for execve()
        std::vector<char*> envp; // for execve()
        std::string cgiOutputBuffer; // collects partial CGI output
        bool responseReady; // indicates complete http response
        int& error_code;
        terminationStatus termination_status;
        time_t startTime;
            
        void resolveScriptPath(const HttpRequest & request, const LocationConfig & location); // resolves path according to configs
        void setArgv(const HttpRequest & request, const LocationConfig & location);
        void setEnvp(const HttpRequest & request);
        void readOutput();
};