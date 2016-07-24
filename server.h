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

#ifndef _SERVER_H_
#define _SERVER_H_

#include <set>
#include <vector>
#include <string>

#include "log.h"
#include "event.h"
#include "socket.h"
#include "connection.h"

#define ERROR_SOCKET_ERROR                            1005001
#define ERROR_CONNECTION_LOST  						  1005002
#define ERROR_CANNOT_CREATE_CONFIRMATIONS_UDP_PACKET  1005003

class Server;
class BenchmarkTcpServer;

class TcpListener: public Event::Handler {
private:
	Server *server;
	std::string name;
	Socket socket;
	Address udpAddress;
	Event eventRead;
	Event eventClose;

public:
	TcpListener(
		Server &server,
		const std::string &name,
		const Address &tcpAddress,
		const Address &udpAddress,
		int backlog,
		int receiveAddressSize );
	void handle(Event &event, long long plannedTimeUs);

	const std::string getName() const { return name; }
	const Address& getTcpAddress() const { return socket.getAddressLocal(); }
	const Address& getUdpAddress() const { return udpAddress; }
};

class UdpListener: public Event::Handler {
private:
	Server *server;
	std::string name;
	Socket socket;
	Address tcpAddress;
	Event eventRead;
	Event eventClose;

	int lastTcpSocketIndex;
	Address receiveAddress;
	Packet receivePacket;
	int receivePacketSize;

public:
	std::map<Address, Connection*> connections;

	UdpListener(
		Server &server,
		const std::string &name,
		const Address &udpAddress,
		const Address &tcpAddress,
		int receivePacketSize,
		int receiveAddressSize );

	void handle(Event &event, long long plannedTimeUs);

	const std::string getName() const { return name; }
	const Address& getTcpAddress() const { return tcpAddress; }
	const Address& getUdpAddress() const { return socket.getAddressLocal(); }

	Connection* connectionByAddress(const Address &udpAddress);
	Socket& getSocket() { return socket; }
	const Socket& getSocket() const { return socket; }
};

class Server {
public:
	std::string name;
	int lastObjectIndex;

	bool test;
	Address testTcpRemoteAddress;
	Address testUdpRemoteAddress;

	int tcpBacklog;
	int tcpReceiveAddressSize;
	int udpReceivePacketSize;
	int udpReceiveAddressSize;
	int udpSendPacketSize;
	int udpMaxSentBufferSize;
	int udpMaxReceiveBufferSize;
	int udpMaxSentMeasureSize;
	int udpResendCount;
	int confirmationResendCount;
	double udpMaxSendLossPercent;
	long long udpInitialSendIntervalUs;
	long long udpResendUs;
	long long buildConfirmationsUs;
	long long buildUdpPacketsUs;
	long long udpMaxSentMeasureUs;

	std::set<TcpListener*> tcpListeners;
	std::set<UdpListener*> udpListeners;
	std::set<BenchmarkTcpServer*> testListeners;
	std::set<Connection*> connections;

	long long udpSummaryConnectionsSendIntervalUs;

	long long statTcpSent;
	long long statTcpReceived;
	long long statUdpSent;
	long long statUdpReceived;
	long long statUdpReceivedExtra;
	long long statCpuWorkUs;
	long long statCpuSleepUs;
	long long statLastMeasureUs;

	Log log;
	Event::Manager eventManager;
	Socket::Group socketGroup;

	Server(const std::string &name);
	~Server();

	void onTcpListenerClosed(TcpListener &tcpListener, bool error = false);
	void onUdpListenerClosed(UdpListener &udpListener, bool error = false);
	void onConnectionClosed(Connection &connection);

	TcpListener* createTcpListener(const Address &tcpAddress, const Address &udpAddress);
	UdpListener* createUdpListener(const Address &udpAddress, const Address &tcpAddress);
	BenchmarkTcpServer* createTestListener(const Address &tcpAddress);
	Connection* createConnection(Socket &tcpSocket, UdpListener &udpListener, const Address &udpAddress);

	long long getUdpInitialIntervalUs() const;
	double getUdpInitialSpeed() const;

	void run(long long stepUs = 2000000);

	void begin();
	bool stepWhile(long long durationUs, bool force = false);
	bool step(long long stepUs = 2000000);
	void end();
};

#endif
