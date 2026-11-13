#pragma once

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "LocationConfig.hpp"


/* This class represents each isntance of a CGI script in http request */
class CGI {
    public:
        CGI(const HttpRequest& request, HttpResponse& response, const LocationConfig& location, const std::string& cgi_extension, std::map<int, CGI*>& cgiMap);
        
        const std::string extension;
        const std::string interpreterPath; // e.g. /usr/bin/python3
        const std::string& getScriptPath() const;
        const std::string& getArgv() const;
        const std::vector<const char*>& getEnvp() const;
        const std::string& getBuffer() const;
        HttpResponse& response;
        bool isResponseReady() const;
        std::map<int, CGI*>& clientsCGImap; // contains all actively running CGI scripts per client

        void runScript();
        void processOutput();
        

    private:
        int cgi_fd; // parent pipe reading end
        std::string scriptPath; // e.g. www/scripts/script.py
        std::string argv; // for execve()
        std::vector<const char*> envp; // for execve()
        std::string cgiOutputBuffer; // collects partial CGI output
        bool responseReady; // indicates complete http response
            

        void resolveScriptPath(const HttpRequest & request, const LocationConfig & location); // resolves path according to configs
        void setArgv(const HttpRequest & request, const LocationConfig & location);
        void setEnvp(const HttpRequest & request);
        void recordInClient() const;
        void readOutput();
        void buildResponse();
};