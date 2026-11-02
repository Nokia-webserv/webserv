/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   tempServerConfig.hpp                               :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: damohame <damohame@student.42berlin.d>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/02 15:32:51 by damohame          #+#    #+#             */
/*   Updated: 2026/10/26 13:05:36 by damohame         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVERCONFIG_HPP
#define SERVERCONFIG_HPP

#include <string>

// the simplest form of the class to cover the networking 
// listening - bind
class ServerConfig {

    public:
        std::string host;
        int port;
        
        ServerConfig();
        ServerConfig(std::string host, int port);
        ~ServerConfig();

};

#endif

