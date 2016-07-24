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

#include <cmath>

#include "server.h"
#include "platform.h"

#include "test/testlauncher.h"
#include "test/benchmarktcpserver.h"


//#define DUMP_UDP_RECV_BAD_PACKETS

//#define LOG_STATISTICS

//#define LOG_CONECTIONS


TcpListener::TcpListener(
	Server &server,
	const std::string &name,
	const Address &tcpAddress,
	const Address &udpAddress,
	int backlog,
	int receiveAddressSize
):
	server(&server),
	name(name),
	socket(server.socketGroup, name + "(socket)", Socket::TCP, receiveAddressSize),
	udpAddress(udpAddress),
	eventRead(*this, server.eventManager, socket.sourceRead),
	eventClose(*this, server.eventManager, socket.sourceClose)
{
	server.log.info(name, "open");
	socket.bind(tcpAddress);
	socket.listen(backlog);
	eventRead.setTimeRelativeNow();
	eventClose.setTimeRelativeNow();
}

void TcpListener::handle(Event &event, long long) {
	if (&event == &eventRead) {
		event.setTimeRelativeNow();
		Socket *client = socket.accept();
		if (client) {
			#ifdef LOG_CONECTIONS
			server->log.info(name, "received tcp-connection from %s", client->getAddressRemote().toString().c_str());
			#endif
			UdpListener *udpListener = server->createUdpListener(Address(), Address());
			if (!udpListener || !server->createConnection(*client, *udpListener, udpAddress)) {
				server->log.info(name, "tcp-connection from %s cancelled", client->getAddressRemote().toString().c_str());
				delete client;
			}
		}
	} else
	if (&event == &eventClose) {
		server->log.info(name, "close");
		server->onTcpListenerClosed(*this, socket.wasError());
	}
}

UdpListener::UdpListener(
	Server &server,
	const std::string &name,
	const Address &udpAddress,
	const Address &tcpAddress,
	int receivePacketSize,
	int receiveAddressSize
):
	server(&server),
	name(name),
	socket(server.socketGroup, name + "(socket)", Socket::UDP, receiveAddressSize),
	tcpAddress(tcpAddress),
	eventRead(*this, server.eventManager, socket.sourceRead, tcpAddress.data.empty() ? 0 : 1),
	eventClose(*this, server.eventManager, socket.sourceClose),
	lastTcpSocketIndex(),
	receivePacketSize(receivePacketSize)
{
	#ifdef LOG_CONECTIONS
	server.log.info(name, "open");
	#else
	if (!tcpAddress.data.empty()) server.log.info(name, "open");
	#endif

	if (!udpAddress.data.empty())
		socket.bind(udpAddress);
	eventRead.setTimeRelativeNow();
	eventClose.setTimeRelativeNow();
}

void UdpListener::handle(Event &event, long long) {
	if (&event == &eventRead) {
		receivePacket.setRawSize(receivePacketSize);
		int size = socket.readfrom(receivePacket.getRawData(), receiveAddress, receivePacket.getRawSize());
		eventRead.setTimeRelativeNow();

		if (size >= 0 && !receiveAddress.data.empty()) {
			server->statUdpReceived += size;
			receivePacket.setRawSize(size);
			if (!receivePacket.checkCrc32()) {
				#ifdef DUMP_UDP_RECV_BAD_PACKETS
				std::cout << "[" << name << " received bad udp-packet, size " << receivePacket.getRawSize() << "]" << std::endl;
				#endif
			} else {
				Connection *connection = connectionByAddress(receiveAddress);
				if (!tcpAddress.data.empty() && !connection) {
					bool isDataPacketType = false;
					switch(receivePacket.getType()) {
					case Packet::Hello:
					case Packet::Bye:
					case Packet::Data:
					case Packet::Disconnect:
						isDataPacketType = true;
						break;
					default:
						break;
					}

					if (isDataPacketType) {
						std::string clientName = Log::strprintf("%s(tcpSocket%d)", name.c_str(), ++lastTcpSocketIndex);
						Socket *client = new Socket(server->socketGroup, clientName, Socket::TCP, socket.getReceiveAddressSize());
						client->connect(tcpAddress);
						connection = server->createConnection(*client, *this, receiveAddress);
					}
				}
				if (connection)
					connection->udpRead(receivePacket);
			}
		}
	} else
	if (&event == &eventClose) {
		if (connections.empty()) {
			#ifdef LOG_CONECTIONS
			server->log.info(name, "close");
			#else
			if (!tcpAddress.data.empty()) server->log.info(name, "close");
			#endif
			server->onUdpListenerClosed(*this, socket.wasError());
		}
	}
}

Connection* UdpListener::connectionByAddress(const Address &udpAddress) {
	std::map<Address, Connection*>::const_iterator i = connections.find(udpAddress);
	return i == connections.end() ? NULL : i->second;
}


Server::Server(const std::string &name):
	name(name),
	lastObjectIndex(),
	test(),
	tcpBacklog(1024),
	tcpReceiveAddressSize(1024),
	udpReceivePacketSize(1024*1024),
	udpReceiveAddressSize(1024),
	udpSendPacketSize(1024), // (1460),
	udpMaxSentBufferSize(8*1024*1024),
	udpMaxReceiveBufferSize(8*1024*1024),
	udpMaxSentMeasureSize(64*1024),
	udpResendCount(20),
	confirmationResendCount(5),
	udpMaxSendLossPercent(30.0),
	udpInitialSendIntervalUs(1000),
	udpResendUs(1000000),
	buildConfirmationsUs(100000),
	buildUdpPacketsUs(100000),
	udpMaxSentMeasureUs(1000000),
	udpSummaryConnectionsSendIntervalUs(),
	statTcpSent(),
	statTcpReceived(),
	statUdpSent(),
	statUdpReceived(),
	statUdpReceivedExtra(),
	statCpuWorkUs(),
	statCpuSleepUs(),
	statLastMeasureUs(Platform::nowUs()),
	socketGroup(name + "(socketGroup)", log)
{
	log.streams[Log::Info]    = &std::cout;
	log.streams[Log::Warning] = &std::cerr;
	log.streams[Log::Error]   = &std::cerr;
}

Server::~Server() {
	while(!connections.empty())
		onConnectionClosed(**connections.begin());
	while(!tcpListeners.empty())
		onTcpListenerClosed(**tcpListeners.begin());
	while(!udpListeners.empty())
		onUdpListenerClosed(**udpListeners.begin());
	while(!testListeners.empty())
		{ delete *testListeners.begin(); testListeners.erase(testListeners.begin()); }
}

void Server::onTcpListenerClosed(TcpListener &tcpListener, bool error) {
	log.info(tcpListener.getName(), "close");

	Address tcpAddress = tcpListener.getTcpAddress();
	Address udpAddress = tcpListener.getUdpAddress();

	tcpListeners.erase(&tcpListener);
	delete &tcpListener;

	if (error && !udpAddress.data.empty())
		createTcpListener(tcpAddress, udpAddress);
}

void Server::onUdpListenerClosed(UdpListener &udpListener, bool error) {
	#ifdef LOG_CONECTIONS
	log.info(udpListener.getName(), "close");
	#else
	if (!udpListener.getTcpAddress().data.empty()) log.info(udpListener.getName(), "close");
	#endif

	Address udpAddress = udpListener.getUdpAddress();
	Address tcpAddress = udpListener.getTcpAddress();

	udpListeners.erase(&udpListener);
	delete &udpListener;

	if (error && !tcpAddress.data.empty())
		createUdpListener(udpAddress, tcpAddress);
}

void Server::onConnectionClosed(Connection &connection) {
	#ifdef LOG_CONECTIONS
	log.info(connection.getName(), "close");
	#endif

	UdpListener &udpListener = connection.getUdpListener();
	std::map<Address, Connection*>::iterator i = udpListener.connections.find(connection.getUdpAddress());
	if (i != udpListener.connections.end() && i->second == &connection)
		udpListener.connections.erase(i);
	connections.erase(&connection);
	delete &connection;

	if (udpListener.getTcpAddress().data.empty())
		onUdpListenerClosed(udpListener);
}

TcpListener* Server::createTcpListener(const Address &tcpAddress, const Address &udpAddress) {
	std::string name = Log::strprintf("#%d tcp-listener %s -> %s", ++lastObjectIndex, tcpAddress.toString().c_str(), udpAddress.toString().c_str());
	TcpListener *tcpListener = new TcpListener(
		*this,
		name,
		tcpAddress,
		udpAddress,
		tcpBacklog,
		tcpReceiveAddressSize );
	tcpListeners.insert(tcpListener);
	return tcpListener;
}

UdpListener* Server::createUdpListener(const Address &udpAddress, const Address &tcpAddress) {
	std::string name = Log::strprintf("#%d udp-listener %s -> %s", ++lastObjectIndex, udpAddress.toString().c_str(), tcpAddress.toString().c_str());
	UdpListener *udpListener = new UdpListener(
		*this,
		name,
		udpAddress,
		tcpAddress,
		udpReceivePacketSize,
		udpReceiveAddressSize );
	udpListeners.insert(udpListener);
	return udpListener;
}

BenchmarkTcpServer* Server::createTestListener(const Address &tcpAddress) {
	std::string name = Log::strprintf("#%d test-listener %s", ++lastObjectIndex, tcpAddress.toString().c_str());
	BenchmarkTcpServer *testListener = new BenchmarkTcpServer(
		socketGroup,
		eventManager,
		name,
		tcpAddress,
		tcpReceiveAddressSize,
		tcpBacklog );
	testListeners.insert(testListener);
	return testListener;
}

long long Server::getUdpInitialIntervalUs() const {
	long long udpSendIntervalUs = udpInitialSendIntervalUs;
	if (!connections.empty()) {
		double summarySpeed = 0.0;
		int count = 0;
		for(std::set<Connection*>::const_iterator i = connections.begin(); i != connections.end(); ++i)
			if (!(*i)->isNoMoreDataWillBeSent()) {
				summarySpeed += 1.0/(double)std::max(10ll, (*i)->getUdpSendIntervalUs());
				++count;
			}
		if (count > 0) {
			summarySpeed = std::max(summarySpeed, 0.25/(double)udpInitialSendIntervalUs);
			udpSendIntervalUs = (long long)::ceil((double)(count+1)/summarySpeed);
			if (udpSendIntervalUs > udpResendUs/3)
				udpSendIntervalUs = udpResendUs/3;
			if (udpSendIntervalUs < udpInitialSendIntervalUs/2)
				udpSendIntervalUs = udpInitialSendIntervalUs/2;
		}
	}
	return udpSendIntervalUs;
}

double Server::getUdpInitialSpeed() const {
	return 1000000.0*(double)udpSendPacketSize/(double)getUdpInitialIntervalUs();
}

Connection* Server::createConnection(Socket &tcpSocket, UdpListener &udpListener, const Address &udpAddress) {
	std::string shortName = Log::strprintf("#%d", ++lastObjectIndex);
	std::string name = Log::strprintf("%s connection %s <-> %s", shortName.c_str(), tcpSocket.getAddressRemote().toString().c_str(), udpAddress.toString().c_str());

	#ifdef LOG_CONECTIONS
	log.info(name, "opening");
	#endif

	if (Connection *c = udpListener.connectionByAddress(udpAddress)) {
		log.warning("already exists connection with same udp address (%s)", c->getName().c_str());
		return NULL;
	}

	Connection *connection = new Connection(
		*this,
		name,
		shortName,
		tcpSocket,
		udpListener,
		udpAddress,
		udpSendPacketSize,
		udpMaxSentBufferSize,
		udpMaxReceiveBufferSize,
		udpMaxSentMeasureSize,
		udpResendCount,
		confirmationResendCount,
		udpMaxSendLossPercent,
		getUdpInitialIntervalUs(),
		udpResendUs,
		buildConfirmationsUs,
		buildUdpPacketsUs,
		udpMaxSentMeasureUs );
	connections.insert(connection);
	udpListener.connections[udpAddress] = connection;
	return connection;
}

void Server::run(long long stepUs) {
	begin();
	while(step(stepUs));
	end();
}

void Server::begin() {
	if (test) {
		log.info(name, "'test' flag is set");
		TestLauncher::launchAll(log, testTcpRemoteAddress, testUdpRemoteAddress);
		test = false;
	}
	log.info(name, "start");
}

bool Server::stepWhile(long long durationUs, bool force) {
	long long endUs = Platform::nowUs() + durationUs;
	bool success = true;
	while(force || success) {
		long long stepUs = endUs - Platform::nowUs();
		if (stepUs < 0) stepUs = 0;
		success &= step(stepUs);
		if (stepUs <= 0) break;
	}
	return success;
}

bool Server::step(long long stepUs) {
	long long beginUs = Platform::nowUs();
	long long nextUs = eventManager.doEvents(beginUs);
	if (nextUs < 0 || nextUs > beginUs + stepUs)
		nextUs = beginUs + stepUs;
	long long pollBeginUs = Platform::nowUs();
	long long durationUs = std::max(0ll, nextUs - pollBeginUs);
	socketGroup.poll(durationUs);

	#ifdef LOG_STATISTICS
	long long pollEndUs = Platform::nowUs();
	long long endUs = pollEndUs;
	long long actualDurationUs = std::min(durationUs, pollEndUs - pollBeginUs);
	statCpuWorkUs += endUs - beginUs - actualDurationUs;
	statCpuSleepUs += actualDurationUs;
	if (statLastMeasureUs + 2000000ll <= pollEndUs) {
		long long dtUs = pollEndUs - statLastMeasureUs;
		double dt = 0.000001*(double)dtUs;

		double cpuLoading = (double)(statCpuWorkUs)/(double)(statCpuWorkUs + statCpuSleepUs);
		double tcpSend = (double)statTcpSent/dt;
		double tcpReceive = (double)statTcpReceived/dt;
		double udpSend = (double)statUdpSent/dt;
		double udpReceive = (double)statUdpReceived/dt;
		double udpReceiveExtra = (double)statUdpReceivedExtra/dt;

		log.info(name,
			"cpu %f%%, tcp send %fKB/s, tcp receive %fKB/s, udp send %fKB/s, udp receive %fKB/s, udp overhead %fKB/s",
			100.0*cpuLoading,
			tcpSend/1024.0,
			tcpReceive/1024.0,
			udpSend/1024.0,
			udpReceive/1024.0,
			udpReceiveExtra/1024.0 );

		int count = 0;
		double minSpeed = 0.0;
		double maxSpeed = 0.0;
		double sumSpeed = 0.0;
		for(std::set<Connection*>::const_iterator i = connections.begin(); i != connections.end(); ++i)
			if (!(*i)->isNoMoreDataWillBeSent()) {
				double speed = 1000000.0*(double)udpSendPacketSize/std::max(10ll, (*i)->getUdpSendIntervalUs());
				if (count == 0 || minSpeed > speed) minSpeed = speed;
				if (count == 0 || maxSpeed < speed) maxSpeed = speed;
				sumSpeed += speed;
				++count;
			}
		double avgSpeed = count ? sumSpeed/(double)count : 0.0;

		double sumSqrDeviation = 0.0;
		for(std::set<Connection*>::const_iterator i = connections.begin(); i != connections.end(); ++i)
			if (!(*i)->isNoMoreDataWillBeSent()) {
				double speed = 1000000.0*(double)udpSendPacketSize/std::max(10ll, (*i)->getUdpSendIntervalUs());
				sumSqrDeviation += (speed - avgSpeed)*(speed - avgSpeed);
			}
		double avgDeviation = count ? ::sqrt(sumSqrDeviation/(double)count) : 0;

		log.info(name,
			"connections %d, initial speed %fKB/s, avg %fKB/s, min %fKB/s, max %fKB/s, deviation %fKB/s",
			count,
			getUdpInitialSpeed()/1024.0,
			avgSpeed/1024.0,
			minSpeed/1024.0,
			maxSpeed/1024.0,
			avgDeviation/1024.0 );

		statLastMeasureUs = pollEndUs;
		statCpuWorkUs = 0;
		statCpuSleepUs = 0;
		statTcpSent = 0;
		statTcpReceived = 0;
		statUdpSent = 0;
		statUdpReceived = 0;
		statUdpReceivedExtra = 0;
	}
	#endif

	return !tcpListeners.empty() || !udpListeners.empty() || !testListeners.empty() || !connections.empty();
}

void Server::end() {
	log.info(name, "stop");
}
