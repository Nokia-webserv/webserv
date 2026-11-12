#include "Router.hpp"

class CGI {
    public:
        CGI(const HttpRequest& request, HttpResponse& response, const LocationConfig& location, const std::string& cgi_extension);
        
        HttpResponse & response;
        std::string extension;
        std::string interpreterPath;
        std::string scriptPath;
        std::string argv;
        std::vector<const char*> envp;

    private:
        //void setPathname(const LocationConfig& location, const std::string& cgi_extension);
        void resolveScriptPath(const HttpRequest & request, const LocationConfig & location);
        void setArgv(const HttpRequest & request, const LocationConfig & location);
        void setEnvp(const HttpRequest & request);
};