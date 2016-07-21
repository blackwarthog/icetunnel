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

#ifndef _SOCKET_H_
#define _SOCKET_H_

#include "log.h"
#include "event.h"
#include "address.h"


class Socket {
public:
	enum Type {
		UDP,
		TCP
	};

	class Group {
	private:
		friend class Socket;
		struct Internal;
		Internal *internal;
		std::string name;
		Log *log;
	public:
		explicit Group(const std::string &name, Log &log);
		~Group();
		void poll(long long durationUs);
		Log& getLog() const { return *log; }
	};

private:
	struct Internal;

	Internal *internal;
	Group *group;
	std::string name;
	Type type;
	Address addressLocal;
	Address addressRemote;
	int lastClientIndex;
	bool connected;
	bool error;

	Address receiveAddress;
	int receiveAddressSize;

public:
	Event::Source sourceRead;
	Event::Source sourceWrite;
	Event::Source sourceCloseRead;
	Event::Source sourceCloseWrite;
	Event::Source sourceClose;

public:
	Socket(Group &group, const std::string &name, Type type, int receiveAddressSize = 1024, void *internalId = NULL);
	~Socket();

	void bind(const Address &address);
	void connect(const Address &address);
	void listen(int backlog);
	Socket* accept();

	int read(void *data, int size);
	int write(const void *data, int size);
	int readfrom(void *data, Address &address, int size);
	int writeto(const void *data, const Address &address, int size, const std::string &writerName = std::string());
	void close(bool error = false);

	void closeRead(bool error = false) {
		if (error) this->error = true;
		if (!sourceCloseRead.getReady()) {
			sourceRead.setReady(false);
			sourceCloseRead.setReady(true);
			if (sourceCloseWrite.getReady()) close();
		}
	}

	void closeWrite(bool error= false) {
		if (error) this->error = true;
		if (!sourceCloseWrite.getReady()) {
			sourceWrite.setReady(false);
			sourceCloseWrite.setReady(true);
			if (sourceCloseRead.getReady()) close();
		}
	}

	void setName(const std::string &name) {
		if (this->name != name) {
			group->log->info(name, "new name %s", name.c_str());
			this->name = name;
		}
	}
	const std::string& getName() const { return name; }
	Group& getGroup() const { return *group; }
	Type getType() const { return type; }
	int getReceiveAddressSize() const { return receiveAddressSize; }
	const Address& getAddressLocal() const { return addressLocal; }
	const Address& getAddressRemote() const { return addressRemote; }
	bool wasError() const { return error; }

	static void initialize();
	static void deinitialize();
};


#endif
