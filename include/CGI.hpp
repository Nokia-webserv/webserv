#pragma ocnce

#include "Router.hpp"

class CGI {
    public:
        CGI(const HttpRequest& request, HttpResponse& response, const LocationConfig& location, const std::string& cgi_extension);
        
        const std::string extension;
        const std::string interpreterPath;
        const std::string& getScriptPath();
        const std::string& getArgv();
        const std::vector<const char*>& getEnvp();
        const std::string& getBuffer();
        HttpResponse& response;
        bool isResponseReady();

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
        void readOutput();
        void buildResponse();
        
};