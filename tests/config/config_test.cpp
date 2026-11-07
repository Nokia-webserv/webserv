#include "Configs.hpp"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

static std::string numberToString(size_t value)
{
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

static void printVector(const std::string &name,
                        const std::vector<std::string> &values)
{
    for (size_t i = 0; i < values.size(); ++i)
        std::cout << name << "[" << i << "]=" << values[i] << "\n";
}

static void printConfig(const std::vector<ServerConfig> &servers)
{
    std::cout << "servers=" << servers.size() << "\n";

    for (size_t i = 0; i < servers.size(); ++i)
    {
        const ServerConfig &server = servers[i];
        
        for (size_t j = 0; j < server.hostPorts.size(); ++j)
        {
            std::cout << "server[" << i << "].host[" << j << "]=" << server.hostPorts[j].host << "\n";
            std::cout << "server[" << i << "].port[" << j << "]=" << server.hostPorts[j].port << "\n";
        }

        printVector("server[" + numberToString(i) + "].server_name",
                    server.server_names);
        std::cout << "server[" << i << "].client_max_body_size="
                  << server.client_max_body_size << "\n";
        std::cout << "server[" << i << "].client_max_header_size="
                  << server.client_max_header_size << "\n";
        std::cout << "server[" << i << "].client_max_response_size="
                  << server.client_max_response_size << "\n";
        std::cout << "server[" << i << "].connections_max_number="
                  << server.connections_max_number << "\n";
        std::cout << "server[" << i << "].error_pages="
                  << server.error_pages.size() << "\n";

        for (std::map<int, std::string>::const_iterator it =
                 server.error_pages.begin();
             it != server.error_pages.end(); ++it)
        {
            std::cout << "server[" << i << "].error_page[" << it->first
                      << "]=" << it->second << "\n";
        }

        std::cout << "server[" << i << "].locations="
                  << server.locations.size() << "\n";

        for (size_t j = 0; j < server.locations.size(); ++j)
        {
            const LocationConfig &location = server.locations[j];
            std::string prefix = "server[" + numberToString(i)
                               + "].location[" + numberToString(j) + "]";

            std::cout << prefix << ".path=" << location.path << "\n";
            std::cout << prefix << ".root=" << location.root << "\n";
            std::cout << prefix << ".index=" << location.index << "\n";
            std::cout << prefix << ".autoindex="
                      << (location.autoindex ? "on" : "off") << "\n";
            printVector(prefix + ".method", location.methods);
            std::cout << prefix << ".client_max_body_size="
                      << location.client_max_body_size << "\n";
            std::cout << prefix << ".client_max_header_size="
                      << location.client_max_header_size << "\n";
            std::cout << prefix << ".upload_store="
                      << location.upload_store << "\n";
            std::cout << prefix << ".has_redirect="
                      << (location.has_redirect ? "true" : "false") << "\n";
            std::cout << prefix << ".redirect_code="
                      << location.redirect_code << "\n";
            std::cout << prefix << ".redirect_url="
                      << location.redirect_url << "\n";
            std::cout << prefix << ".cgi_handlers="
                      << location.cgi_handlers.size() << "\n";

            for (std::map<std::string, std::string>::const_iterator it =
                     location.cgi_handlers.begin();
                 it != location.cgi_handlers.end(); ++it)
            {
                std::cout << prefix << ".cgi[" << it->first << "]="
                          << it->second << "\n";
            }
        }
    }
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: config_test <config-file>\n";
        return 2;
    }

    try
    {
        Configs configs(argv[1]);
        printConfig(configs.getParsedConfigs());
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << "\n";
        return 1;
    }

    return 0;
}
