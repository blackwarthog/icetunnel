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

#include "simpletcpserver.h"


SimpleTcpServer::SimpleTcpServer(
	Socket::Group &socketGroup,
	Event::Manager &eventManager,
	const std::string &name,
	const Address &address,
	int receiveAddressSize,
	int backlog
):
	socket(socketGroup, name, Socket::TCP, receiveAddressSize),
	eventRead(*this, eventManager, socket.sourceRead)
{
	eventRead.setTimeRelativeNow();
	socket.bind(address);
	socket.listen(backlog);
}

SimpleTcpServer::~SimpleTcpServer() {
	while(!clients.empty()) {
		delete *clients.begin();
		clients.erase(clients.begin());
	}
}

void SimpleTcpServer::handle(Event &event, long long plannedTimeUs) {
	if (&event == &eventRead) {
		Socket *clientSocket = socket.accept();
		if (clientSocket) {
			if (eventRead.getManager()) {
				SimpleTcpClient *client = new SimpleTcpClient(*clientSocket, *eventRead.getManager());
				clients.insert(client);
			} else {
				delete clientSocket;
			}
		}
		eventRead.setTimeRelativeNow();
	}
}
