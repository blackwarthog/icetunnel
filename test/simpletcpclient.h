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

#ifndef _SIMPLETCPCLIENT_H_
#define _SIMPLETCPCLIENT_H_

#include "../event.h"
#include "../socket.h"

#include "queue.h"


class SimpleTcpClient: public Event::Handler {
protected:
	Socket *socket;
	Event eventRead;
	Event eventWrite;

	bool closeOnEof;
	Queue readBuffer;
	Queue writeBuffer;

public:
	SimpleTcpClient(Socket &socket, Event::Manager &eventManager);
	~SimpleTcpClient();

	void handle(Event &event, long long plannedTimeUs);

	void push(const void *data, int size) {
		if (writeBuffer.empty()) eventWrite.setTimeRelativeNow();
		writeBuffer.push(data, size);
	}

	int pop(void *data, int size)
		{ return readBuffer.pop(data, size); }

	void push(char c) { push(&c, 1); }
	char pop() { char c = 0; pop(&c, 1); return c; }

	void finish() { closeOnEof = true; }
	bool empty() const { return readBuffer.empty(); }
	int size() const { return readBuffer.size(); }
	bool done() const { return writeBuffer.empty(); }

	Socket& getSocket() const { return *socket; }
	bool isClosed() const { return socket->sourceClose.getReady(); }
	bool wasError() const { return socket->wasError(); }
};

#endif
