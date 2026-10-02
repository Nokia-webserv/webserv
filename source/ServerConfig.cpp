/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerConfig.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: damohame <damohame@student.42berlin.d>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/02 15:37:07 by damohame          #+#    #+#             */
/*   Updated: 2026/10/02 16:25:07 by damohame         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ServerConfig.hpp"

// default contrustor with default values
ServerConfig::ServerConfig()
    : host("127.0.0.1"), port(8080)
{
}

// useful to test multiple server config 
ServerConfig::ServerConfig(std::string host, int port)
    : host(host), port(port)
{

}

ServerConfig::~ServerConfig()
{
}
