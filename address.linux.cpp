/*
    ......... 2016 Ivan Mahonin

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <cstring>
#include <cstdio>

#include <algorithm>

#include <netdb.h>
#include <arpa/inet.h>

#include "address.h"
#include "log.h"

bool Address::fromString(const std::string &address) {
	size_t pos = address.find(":");
	std::string host;
	std::string port;
	if (pos != std::string::npos) {
		host = address.substr(0, pos);
		port = address.substr(pos+1);
	} else {
		host = "0.0.0.0";
		port = address;
	}

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(atoi(port.c_str()));

    bool isIp = true;
    for(int i = 0; i < (int)host.size(); ++i)
    	if (host[i] != '.' && (host[i] < '0' || host[i] > '9'))
    		isIp = false;

    hostent *he;
	in_addr **addr_list;
    if ( !isIp
      && (he = gethostbyname(host.c_str()))
      && (addr_list = (struct in_addr **)he->h_addr_list)
	  && (*addr_list) )
    {
        // read resolved ip
       	addr.sin_addr = **addr_list;
    } else {
        // try read ip
    	int addr_i[] = { 0, 0, 0, 0 };
    	sscanf(host.c_str(), "%d.%d.%d.%d", &addr_i[0], &addr_i[1], &addr_i[2], &addr_i[3]);
    	unsigned char *addr_ch = (unsigned char *)&addr.sin_addr;
    	addr_ch[0] = (unsigned char)addr_i[0];
    	addr_ch[1] = (unsigned char)addr_i[1];
    	addr_ch[2] = (unsigned char)addr_i[2];
    	addr_ch[3] = (unsigned char)addr_i[3];
    }

    data.resize(sizeof(addr));
    memcpy(&data.front(), &addr, sizeof(addr));

	return true;
}

std::string Address::toString() const {
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    if (!data.empty())
    	memcpy(&addr, &data.front(), std::min(sizeof(addr), data.size()));
    return Log::strprintf("%s:%d", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
}
