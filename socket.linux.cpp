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

#include <cstdlib>
#include <cstring>
#include <cerrno>

#include <algorithm>
#include <vector>

#include <unistd.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "socket.h"


struct Socket::Group::Internal {
	int fd;
	std::vector<epoll_event> events;
	int count;
	Internal(): fd(), count() { }
};

Socket::Group::Group(const std::string &name, Log &log):
	internal(new Internal()),
	name(name),
	log(&log)
{
	internal->fd = ::epoll_create(1000);
	internal->events.resize(1000);
	if (internal->fd < 0)
		this->log->error(this->name, "epoll_create failed");
}

Socket::Group::~Group() {
	::close(internal->fd);
	delete internal;
}

void Socket::Group::poll(long long durationUs) {
	if (durationUs < 0) durationUs = 0;
	long long durationMs = (durationUs + rand()%1000)/1000;

	if ((int)internal->events.size() < 3*internal->count)
		internal->events.resize(6*internal->count);
    int nfds = epoll_wait(internal->fd, &internal->events.front(), internal->events.size(), durationMs);
    if (nfds == -1) {
		this->log->errorno(name, "epoll_wait failed");
		usleep(1000);
		return;
    }

    for(int i = 0; i < nfds; ++i) {
    	Socket &socket = *(Socket*)internal->events[i].data.ptr;
    	if (socket.connected && (internal->events[i].events & (EPOLLRDHUP | EPOLLHUP)))
    		socket.closeWrite();
    	if (internal->events[i].events & EPOLLERR)
    		socket.closeWrite(true);
    	if (!(internal->events[i].events & EPOLLIN) && socket.sourceCloseWrite.getReady())
    		socket.closeRead();
   		socket.sourceRead.setReady(!socket.sourceCloseRead.getReady() && (internal->events[i].events & EPOLLIN));
   		socket.sourceWrite.setReady(!socket.sourceCloseWrite.getReady() && (internal->events[i].events & EPOLLOUT));
    }
}


struct Socket::Internal {
	int fd;
	Internal(): fd() { }
};


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
	++group.internal->count;

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
		internal->fd = -1;
		return;
	}

	unsigned long int ulIntOn = 1;
	int intOn = 1;
	epoll_event event;
	memset(&event, 0, sizeof(event));
	event.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP;
	event.data.ptr = this;

	if (internal->fd < 0) {
		group.log->errorno(this->name, "cannot create socket");
		close(true);
	} else
	if (::epoll_ctl(group.internal->fd, EPOLL_CTL_ADD, internal->fd, &event)) {
		group.log->errorno(name, "cannot register socket in epoll (epoll_ctl)");
		::shutdown(internal->fd, SHUT_RDWR);
		::close(internal->fd);
		internal->fd = -1;
		close(true);
	} else
	if (::ioctl(internal->fd, FIONBIO, &ulIntOn)) {
		close(true);
	} else {
		::setsockopt(internal->fd, SOL_SOCKET, SO_REUSEADDR, &intOn, sizeof(intOn));
	}
}

Socket::~Socket() {
	close();
	delete internal;
	--group->internal->count;
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
		if (errno != EINPROGRESS)
			group->log->errorno(name, "connect(" + address.toString() + ")");
}

void Socket::listen(int backlog) {
	if (::listen(internal->fd, backlog))
		group->log->errorno(name, "listen");
}

Socket* Socket::accept() {
	receiveAddress.data.resize(receiveAddressSize);
	unsigned int addressSize = receiveAddress.data.size();
	int fd = ::accept(internal->fd, (::sockaddr*)&receiveAddress.data.front(), &addressSize);
	if (fd < 0) {
		receiveAddress.data.clear();
		if (errno == EAGAIN) sourceRead.setReady(false); else
			if (errno != EINTR) group->log->errorno(name, "accept");
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

	int result = ::recv(internal->fd, data, size, MSG_NOSIGNAL);
	if (result < 0) {
		if (errno == EAGAIN) sourceRead.setReady(false); else
			if (errno != EINTR) group->log->errorno(name, "recv");
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

	int result = ::send(internal->fd, data, size, MSG_NOSIGNAL);
	if (result < 0) {
		if (errno == EAGAIN) sourceWrite.setReady(false); else
			if (errno != EINTR) group->log->errorno(name, "send");
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
	unsigned int addressSize = receiveAddress.data.size();
	int result = ::recvfrom(internal->fd, data, size, MSG_NOSIGNAL | MSG_TRUNC, (::sockaddr*)&receiveAddress.data.front(), &addressSize);
	if (result < 0) {
		if (errno == EAGAIN) sourceRead.setReady(false); else
			if (errno != EINTR) group->log->errorno(name, "recvfrom");
	} else {
		address.data.resize(addressSize);
		memcpy(&address.data.front(), &receiveAddress.data.front(), addressSize);
	}

	return std::max(0, result);
}

int Socket::writeto(const void *data, const Address &address, int size, const std::string &writerName) {
	if (sourceCloseWrite.getReady()) {
		group->log->error(name + "(" + writerName + ")", "writeto: socket closed for write");
		return 0;
	}
	if (!sourceWrite.getReady())
		group->log->warning(name + "(" + writerName + ")", "writeto: socket was not ready for write");

	int result = ::sendto(internal->fd, data, size, MSG_NOSIGNAL, (const ::sockaddr*)&address.data.front(), address.data.size());
	if (result < 0) {
		if (errno == EAGAIN) sourceWrite.setReady(false);  else
			if (errno != EINTR) group->log->errorno(name + "(" + writerName + ")", "sendto");
	}

	return std::max(0, result);
}

void Socket::close(bool error) {
	if (error) this->error = true;
	if (internal->fd >= 0) {
		if (::epoll_ctl(group->internal->fd, EPOLL_CTL_DEL, internal->fd, (epoll_event*)1)) // pass any ptr to prevent bug at old kernels
			group->log->errorno(name, "cannot unregister socket from epoll (epoll_ctl)");
		::shutdown(internal->fd, SHUT_RDWR);
		::close(internal->fd);
		internal->fd = -1;
	}
	sourceRead.setReady(false);
	sourceWrite.setReady(false);
	sourceCloseRead.setReady(true);
	sourceCloseWrite.setReady(true);
	sourceClose.setReady(true);
}

void Socket::initialize() { }
void Socket::deinitialize() { }
