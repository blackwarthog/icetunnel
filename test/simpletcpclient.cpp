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

#include "simpletcpclient.h"


SimpleTcpClient::SimpleTcpClient(Socket &socket, Event::Manager &eventManager):
	socket(&socket),
	eventRead(*this, eventManager, socket.sourceRead),
	eventWrite(*this, eventManager, socket.sourceWrite),
	closeOnEof()
{
	eventRead.setTimeRelativeNow();
}

SimpleTcpClient::~SimpleTcpClient()
	{ delete socket; }

void SimpleTcpClient::handle(Event &event, long long) {
	if (&event == &eventRead) {
		while(eventRead.getReady()) {
			readBuffer.reserve(10000);
			if (readBuffer.push(socket->read(readBuffer.end(), 10000)) < 10000) break;
		}
		eventRead.setTimeRelativeNow();
	} else
	if (&event == &eventWrite) {
		if (!writeBuffer.empty())
			writeBuffer.pop(socket->write(writeBuffer.begin(), writeBuffer.size()));
		if (!writeBuffer.empty())
			eventWrite.setTimeRelativeNow();
		if (closeOnEof && writeBuffer.empty())
			socket->close();
	}
}
