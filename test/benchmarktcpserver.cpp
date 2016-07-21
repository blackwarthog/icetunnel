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

#include "benchmarktcpserver.h"


BenchmarkTcpServer::BenchmarkTcpServer(
	Socket::Group &socketGroup,
	Event::Manager &eventManager,
	const std::string &name,
	const Address &address,
	int receiveAddressSize,
	int backlog
):
	SimpleTcpServer(socketGroup, eventManager, name, address, receiveAddressSize, backlog)
{ }

BenchmarkTcpServer::~BenchmarkTcpServer() {
	while(!clients.empty())
		delete *clients.begin();
}

void BenchmarkTcpServer::handle(Event &event, long long plannedTimeUs) {
	if (&event == &eventRead) {
		Socket *clientSocket = socket.accept();
		if (clientSocket) {
			if (eventRead.getManager())
				new BenchmarkTcpClient(*clientSocket, *eventRead.getManager(), clients, stats);
			else
				delete clientSocket;
		}
		eventRead.setTimeRelativeNow();
	}
}
