#pragma once

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "LocationConfig.hpp"

class CGI {
    public:
        CGI(const HttpRequest& request, HttpResponse& response, const LocationConfig& location, const std::string& cgi_extension, std::map<int, CGI*>& cgiMap);
        
        const std::string extension;
        const std::string interpreterPath;
        const std::string& getScriptPath() const;
        const std::string& getArgv() const;
        const std::vector<const char*>& getEnvp() const;
        const std::string& getBuffer() const;
        HttpResponse& response;
        bool isResponseReady() const;
        std::map<int, CGI*>& clientsCGImap;

        void runScript();
        void processOutput();
        

    private:
        std::string scriptPath;
        std::string argv;
        std::vector<const char*> envp;
        std::string cgiOutputBuffer;
        bool responseReady;
            

        void resolveScriptPath(const HttpRequest & request, const LocationConfig & location);
        void setArgv(const HttpRequest & request, const LocationConfig & location);
        void setEnvp(const HttpRequest & request);
        void recordInClient() const;
        void readOutput();
        void buildResponse();
};