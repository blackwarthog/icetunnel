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

#ifndef _BENCHMARKTCPSERVER_H_
#define _BENCHMARKTCPSERVER_H_

#include "simpletcpserver.h"
#include "benchmarktcpclient.h"


class BenchmarkTcpServer: public SimpleTcpServer {
private:
	BenchmarkTcpClient::Container clients;
	BenchmarkTcpClient::StatList stats;

public:
	BenchmarkTcpServer(
		Socket::Group &socketGroup,
		Event::Manager &eventManager,
		const std::string &name,
		const Address &address,
		int receiveAddressSize = 1024,
		int backlog = 1024 );
	~BenchmarkTcpServer();
	void handle(Event &event, long long plannedTimeUs);
};

#endif
