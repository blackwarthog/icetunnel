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

#include <winsock2.h>
#include <windows.h>

#include "socket.h"


struct Socket::Internal {
	SOCKET fd;
	Internal(): fd() { }
};


struct Socket::Group::Internal {
	std::vector<WSAPOLLFD> events;
	std::vector<Socket*> sockets;

	void registerSocket(Socket &socket) {
		events.push_back(WSAPOLLFD());
		sockets.push_back(&socket);
		memset(&events.back(), 0, sizeof(events.back()));
		events.back().fd = socket.internal->fd;
		events.back().events = POLLRDNORM | POLLWRNORM;
	}

	void unregisterSocket(Socket &socket) {
		for(std::vector<Socket*>::iterator i = sockets.begin(); i != sockets.end(); ++i)
			if (*i == &socket) {
				events.erase(events.begin() + (i - sockets.begin()));
				sockets.erase(i);
				break;
			}
	}
};

Socket::Group::Group(const std::string &name, Log &log):
	internal(new Internal()),
	name(name),
	log(&log)
{ }

Socket::Group::~Group() {
	delete internal;
}

void Socket::Group::poll(long long durationUs) {
	if (durationUs < 0) durationUs = 0;
	long long durationMs = (durationUs + rand()%1000)/1000;

	if (WSAPoll(&internal->events.front(), internal->events.size(), durationMs) < 0) {
		log->errorno(name, "WSAPoll failed");
		Sleep(1);
		return;
	}

    for(int i = 0; i < (int)internal->events.size(); ++i) {
    	short revents = internal->events[i].revents;
    	if (revents) {
			Socket &socket = *internal->sockets[i];
			if (socket.connected && (revents & POLLHUP))
				socket.closeWrite();
			if (revents & POLLERR)
				socket.closeWrite(true);
			if (!(revents & POLLRDNORM) && socket.sourceCloseWrite.getReady())
				socket.closeRead();
			socket.sourceRead.setReady(!socket.sourceCloseRead.getReady() && (revents & POLLRDNORM));
			socket.sourceWrite.setReady(!socket.sourceCloseWrite.getReady() && (revents & POLLWRNORM));
    	}
    }
}


Socket::Socket(Group &group, const std::string &name, Type type, int receiveAddressSize, void *internalId):
	internal(new Internal()),
	group(&group),
	name(name),
	type(type),
	lastClientIndex(),
	connected(internalId != NULL),
	error(),
	receiveAddressSize(receiveAddressSize)
{
	if (internalId) {
		internal->fd = *(int*)internalId;
	} else
	switch(type) {
	case TCP:
		internal->fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		break;
	case UDP:
		internal->fd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		break;
	default:
		internal->fd = INVALID_SOCKET;
		return;
	}

	BOOL boolOn = 1;
	unsigned long int ulIntOn = 1;
	if (internal->fd == INVALID_SOCKET) {
		group.log->errorno(this->name, "cannot create socket");
		close(true);
	} else
	if (::ioctlsocket(internal->fd, FIONBIO, &ulIntOn)) {
		close(true);
	} else {
		::setsockopt(internal->fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&boolOn, sizeof(boolOn));
		group.internal->registerSocket(*this);
	}
}

Socket::~Socket() {
	close();
	delete internal;
}

void Socket::bind(const Address &address) {
	addressLocal = address;
	if (::bind(internal->fd, (const sockaddr*)&address.data.front(), address.data.size()))
		group->log->errorno(name, "bind");
}

void Socket::connect(const Address &address) {
	connected = true;
	addressRemote = address;
	if (::connect(internal->fd, (const sockaddr*)&address.data.front(), address.data.size()))
		if (WSAGetLastError() != WSAEWOULDBLOCK)
			group->log->errorno(name, "connect(" + address.toString() + ")");
}

void Socket::listen(int backlog) {
	if (::listen(internal->fd, backlog))
		group->log->errorno(name, "listen");
}

Socket* Socket::accept() {
	receiveAddress.data.resize(receiveAddressSize);
	int addressSize = receiveAddress.data.size();
	int fd = ::accept(internal->fd, (::sockaddr*)&receiveAddress.data.front(), &addressSize);
	if (fd < 0) {
		WSAGetLastError();
		receiveAddress.data.clear();
		if (WSAGetLastError() == WSAEWOULDBLOCK) sourceRead.setReady(false); else
			if (WSAGetLastError() != WSAEINTR) group->log->errorno(name, "accept");
		return NULL;
	}
	receiveAddress.data.resize(addressSize);

	std::string clientName = Log::strprintf("%s(client%d)", name.c_str(), ++lastClientIndex);
	Socket *client = new Socket(*group, clientName, type, receiveAddressSize, &fd);
	if (client->sourceClose.getReady()) {
		delete client;
		return NULL;
	}

	client->addressLocal = addressLocal;
	client->addressRemote = receiveAddress;
	memcpy(&client->addressRemote.data.front(), &receiveAddress.data.front(), addressSize);
	return client;
}

int Socket::read(void *data, int size) {
	if (sourceCloseRead.getReady()) {
		group->log->error(name, "read: socket closed for read");
		return 0;
	}
	if (!sourceRead.getReady())
		group->log->warning(name, "read: socket was not ready for read");

	int result = ::recv(internal->fd, (char*)data, size, 0);
	if (result < 0) {
		if (WSAGetLastError() == WSAEWOULDBLOCK) sourceRead.setReady(false); else
			if (WSAGetLastError() != WSAEINTR) group->log->errorno(name, "recv");
	} else
	if (result == 0)
		closeRead();

	return result >= 0 ? result : 0;
}

int Socket::write(const void *data, int size) {
	if (sourceCloseWrite.getReady()) {
		group->log->error(name, "write: socket closed for write");
		return 0;
	}
	if (!sourceWrite.getReady())
		group->log->warning(name, "write: socket was not ready for write");

	int result = ::send(internal->fd, (const char*)data, size, 0);
	if (result < 0) {
		if (WSAGetLastError() == WSAEWOULDBLOCK) sourceWrite.setReady(false); else
			if (WSAGetLastError() != WSAEINTR) group->log->errorno(name, "send");
	}

	return std::max(0, result);
}

int Socket::readfrom(void *data, Address &address, int size) {
	if (sourceCloseRead.getReady()) {
		group->log->error(name, "readfrom: socket closed for read");
		return 0;
	}
	if (!sourceRead.getReady())
		group->log->warning(name, "readfrom: socket was not ready for read");

	address.data.clear();
	receiveAddress.data.resize(receiveAddressSize);
	int addressSize = receiveAddress.data.size();
	int result = ::recvfrom(internal->fd, (char*)data, size, 0, (::sockaddr*)&receiveAddress.data.front(), &addressSize);
	if (result < 0) {
		if (WSAGetLastError() == WSAEWOULDBLOCK) sourceRead.setReady(false); else
			if (WSAGetLastError() != WSAEINTR) group->log->errorno(name, "recvfrom");
	} else {
		address.data.resize(addressSize);
		memcpy(&address.data.front(), &receiveAddress.data.front(), addressSize);
	}

	return std::max(0, result);
}

int Socket::writeto(const void *data, const Address &address, int size, const std::string &writerName) {
	if (sourceCloseWrite.getReady()) {
		group->log->error(name + "(" + writerName + ")(" + address.toString() + ")", "writeto: socket closed for write");
		return 0;
	}
	if (!sourceWrite.getReady())
		group->log->warning(name + "(" + writerName + ")(" + address.toString() + ")", "writeto: socket was not ready for write");

	int result = ::sendto(internal->fd, (const char*)data, size, 0, (const ::sockaddr*)&address.data.front(), address.data.size());
	if (result < 0) {
		if (WSAGetLastError() == WSAEWOULDBLOCK) sourceWrite.setReady(false);  else
			if (WSAGetLastError() != WSAEINTR) group->log->errorno(name + "(" + writerName + ")(" + address.toString() + ")", "sendto");
	}

	return std::max(0, result);
}

void Socket::close(bool error) {
	if (error) this->error = true;
	if (internal->fd != INVALID_SOCKET) {
		group->internal->unregisterSocket(*this);
		::shutdown(internal->fd, SD_BOTH);
		::closesocket(internal->fd);
		internal->fd = INVALID_SOCKET;
	}
	sourceRead.setReady(false);
	sourceWrite.setReady(false);
	sourceCloseRead.setReady(true);
	sourceCloseWrite.setReady(true);
	sourceClose.setReady(true);
}

void Socket::initialize() {
	WSADATA wsa_data;
	WSAStartup(MAKEWORD(2, 2), &wsa_data);
}

void Socket::deinitialize() {
	WSACleanup();
}
