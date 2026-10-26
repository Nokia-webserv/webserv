#pragma once

#include <string>
#include <vector>
#include <map>

class LocationConfig
{
    public:
        std::string path; // /demo
        std::string root; // www/demo
        std::string index; // index.html
        std::vector<std::string> methods; // GET POST DELETE

        bool autoindex; // on
        long client_max_body_size; // 10m
        
        std::string upload_store; // www/uploads
        

        bool has_redirect; // true
        int redirect_code; // 301
        std::string redirect_url; // /new-demo

        std::map<std::string, std::string> cgi_handlers; // .py = /usr/bin/python3; 
                                                        // .sh =  /bin/bash;  
                                                        // .cgi = "";

        LocationConfig();
        ~LocationConfig();
        
};

/*location /demo {
    root www/demo;
    index index.html;

    methods GET POST DELETE;

    autoindex on;

    client_max_body_size 10m;

    upload_store www/uploads;

    return 301 /new-demo;

    cgi_extension .py /usr/bin/python3;
    cgi_extension .sh /bin/bash;
    cgi_extension .cgi;
}*/
