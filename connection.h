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

#ifndef _CONNECTION_H_
#define _CONNECTION_H_

#include <map>
#include <vector>
#include <string>
#include <list>

#include "event.h"
#include "packet.h"
#include "address.h"
#include "socket.h"

class Server;
class UdpListener;

struct Measure {
	long long beginUs;
	long long endUs;

	int count;
	int size;
	int successSize;
	int failSize;
	long long summaryIntervalUs;

	Measure():
		beginUs(-1),
		endUs(-1),
		count(),
		size(),
		successSize(),
		failSize(),
		summaryIntervalUs()
	{ }
};

class Connection: public Event::Handler {
private:
	Server *server;

	std::string name;
	std::string shortName;

	Socket *tcpSocket;
	UdpListener *udpListener;
	Address udpAddress;

	bool tcpConnected;
	bool udpConnected;

	int udpSendPacketSize;
	int udpMaxSentBufferSize;
	int udpMaxReceiveBufferSize;
	int udpMaxSentMeasureSize;
	int udpResendCount;
	int confirmationResendCount;
	double udpMaxSendLossPercent;
	long long udpResendUs;
	long long buildConfirmationsUs;
	long long buildUdpPacketsUs;
	long long udpMaxSentMeasureUs;

	int tcpNextSendIndex;
	int udpNextSendIndex;
	int udpReceivedMasterIndex;
	int udpConfirmedMasterIndex;
	int udpReceivedFinalIndex;
	int udpSentBufferSize;
	int udpReceiveBufferSize;
	int udpPacketsToSendCount;
	int remainConfirmationResendCount;
	int remainByeByeResendCount;

	std::vector<char> tcpReceivedData;
	std::vector<int> confirmationData;
	std::map<int, Packet> udpSentPackets;
	std::list<Packet> udpConfirmationPackets;
	std::map<int, Packet> udpReceivedPackets;

	long long udpLastSentUs;
	long long udpSendIntervalUs;
	double udpSendIntervalUsFloat;

	int udpSendMeasureIndex;
	std::map<int, Measure> udpSendMeasures;

	Event eventTcpRead;
	Event eventTcpWrite;
	Event eventTcpClose;
	Event eventUdpWrite;
	Event eventUdpClose;
	Event eventBuildUdpPackets;
	Event eventBuildConfirmations;
	Event eventBuildByeBye;
	Event eventUdpResend;
	Event eventUdpCloseWait;
	Event eventClose;

public:
	Connection(
		Server &server,
		const std::string &name,
		const std::string &shortName,
		Socket &tcpSocket,
		UdpListener &udpListener,
		const Address &udpAddress,
		int udpSendPacketSize,
		int udpMaxSentBufferSize,
		int udpMaxReceiveBufferSize,
		int udpMaxSentMeasureSize,
		int udpResendCount,
		int confirmationResendCount,
		double udpMaxSendLossPercent,
		long long udpInitialSendIntervalUs,
		long long udpResendUs,
		long long buildConfirmationsUs,
		long long buildUdpPacketsUs,
		long long udpMaxSentMeasureUs );

	~Connection();

	void handle(Event &event, long long plannedTimeUs);

	const std::string& getName() const { return name; }
	UdpListener& getUdpListener() const { return *udpListener; }
	const Address& getUdpAddress() const { return udpAddress; }

private:
	void tcpRead();
	void tcpWrite(bool fake = false);

public:
	void udpRead(const Packet &packet);

private:
	void udpWrite(long long plannedTimeUs);

	void buildUdpPackets(bool flush);
	void buildConfirmations();
	void buildByeBye();
	void udpResend();

	void tcpClose(bool error = false);
	void udpClose(bool error = false);

	bool isNoMoreDataWillBeSent();
	bool isUdpFinished();
	void onUdpSentBufferChanged(int sizeIncrement);
	void onUdpSent(int measureIndex, long long intervalUs, int size);
	void onUdpDelivered(bool success, int measureIndex, int size);
	void setEventUdpWrite();
};

#endif
