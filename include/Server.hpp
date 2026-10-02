/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: damohame <damohame@student.42berlin.d>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/02 16:24:47 by damohame          #+#    #+#             */
/*   Updated: 2026/10/02 16:33:24 by damohame         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVER_HPP
#define SERVER_HPP

#include "ServerConfig.hpp"
#include <vector>

class Server {

    private:
        std::vector<ServerConfig> configs;

    public:
        Server(const std::vector<ServerConfig>& configs);
        
};

#endif