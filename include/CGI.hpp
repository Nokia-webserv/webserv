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

#define D_ERROR_MSG "Status: 500 Internal Server Error" // specific error message sent from child (CGI) to parent (server)

/* This class represents each isntance of a CGI script in http request */
class CGI {
    public:
        CGI(const HttpRequest& request, HttpResponse& response, RouteDecision decision, const std::string& cgi_extension, 
                CGI*& myPtrInMyClient, std::map<int, int>& pendingPIDs);
        ~CGI();
        
        const std::string extension;
        const std::string interpreterPath; // e.g. /usr/bin/python3
        const HttpRequest request;
        HttpResponse response;
        RouteDecision decision;
        CGI*& myPtrInMyClient; // ref to client class member holding ptr to this object
        std::map<int, int>& pendingPIDs; // contains pids not correctly terminated during CGI destructions
        
        int getFd() const;
        int getErrorCode() const;
        time_t getStartTime() const;
        
        void setResponseReady(); // marks response ready for placing in send queue
        bool isResponseReady() const;
        
        void runScript(); // runs CGI
        void processOutput(); // processes CGI result
        bool reapProcess(); // cleans up child process
        void cleanUp(); // cleans up the object (pipes, ptrs)

    private:
        int pipeRead[2]; // pipe where CGI reads from
        int pipeWrite[2]; // pipe where CGI writes to
        int pid; // process id
        std::string scriptPath; // e.g. www/scripts/script.py
        std::vector<char*> argv; // for execve()
        std::vector<std::string> envp_str; // for execve()
        std::vector<char*> envp; // for execve()
        std::string cgiOutputBuffer; // collects partial CGI output
        bool responseReady; // indicates http response completion
        int& error_code;
        bool termination_status; // indicates subprocess termination success
        time_t startTime; // holds CGI start time to enable timeout termination
            
        void setArgv(const HttpRequest & request, const LocationConfig & location);
        void setEnvp(const HttpRequest & request);
        void readOutput();
};