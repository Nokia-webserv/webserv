#pragma once

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "LocationConfig.hpp"
#include "Router.hpp"
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>   

#define D_ERROR_MSG "CGI_ERROR"

/* This class represents each isntance of a CGI script in http request */
class CGI {
    public:
        CGI(const HttpRequest& request, HttpResponse& response, RouteDecision& result, const std::string& cgi_extension, std::map<int, CGI*>& cgiMap);
        
        const std::string extension;
        const std::string interpreterPath; // e.g. /usr/bin/python3
        std::map<int, CGI*>& clientsCGImap; // contains all actively running CGI scripts per client
        const HttpRequest& request;
        HttpResponse& response;
        RouteDecision& result;
        
        int getFd() const;
        int getPid() const;
        const std::string& getScriptPath() const;
        const std::vector<const char*>& getArgv() const;
        const std::vector<const char*>& getEnvp() const;
        const std::string& getBuffer() const;
        int getErrorCode() const;
        int** getPipes();
        
        void setResponseReady();

        bool isResponseReady() const;
        
        void runScript();
        int processOutput();
        void handleError();
        void cleanUp(int* pipe_ends[4]); 

    private:
        int pipeRead[2];
        int pipeWrite[2];
        // int cgi_read_fd; // parent pipe reading end
        // int cgi_write_fd; // parent pipe writing end
        int pid;
        std::string scriptPath; // e.g. www/scripts/script.py
        std::vector<char*> argv; // for execve()
        std::vector<char*> envp; // for execve()
        std::string cgiOutputBuffer; // collects partial CGI output
        bool responseReady; // indicates complete http response
        int& error_code;
            

        void resolveScriptPath(const HttpRequest & request, const LocationConfig & location); // resolves path according to configs
        void setArgv(const HttpRequest & request, const LocationConfig & location);
        void setEnvp(const HttpRequest & request);
        void recordInClient() const;
        void readOutput();
};