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

#include <climits>
#include <cmath>
#include <cstring>
#include <algorithm>

#include "connection.h"

#include "platform.h"
#include "server.h"


//#define DUMP_TCP_SENT_PACKETS
//#define DUMP_TCP_SENT_PACKETS_DATA
//#define DUMP_TCP_RECV_PACKETS
//#define DUMP_TCP_RECV_PACKETS_DATA

//#define DUMP_UDP_SENT_PACKETS
//#define DUMP_UDP_SENT_PACKETS_DATA
//#define DUMP_UDP_RECV_PACKETS
//#define DUMP_UDP_RECV_PACKETS_DATA

//#define DUMP_UDP_SENT_HANDSHAKINGS
//#define DUMP_UDP_RECV_HANDSHAKINGS

//#define DUMP_UDP_SENT_CONFIRMATIONS
//#define DUMP_UDP_RECV_CONFIRMATIONS

//#define DUMP_UDP_SENT_BYEBYE
//#define DUMP_UDP_RECV_BYEBYE

//#define DUMP_UDP_RESEND
//#define DUMP_UDP_INTERVAL

//#define CHECK_CONFIRMATIONS

//#define SIMULATE_DAMAGE 10

//#define LOG_STATE


Connection::Connection(
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
	long long udpMaxSentMeasureUs
):
	server(&server),
	name(name),
	shortName(shortName),
	tcpSocket(&tcpSocket),
	udpListener(&udpListener),
	udpAddress(udpAddress),
	tcpConnected(true),
	udpConnected(true),
	udpSendPacketSize(udpSendPacketSize),
	udpMaxSentBufferSize(udpMaxSentBufferSize),
	udpMaxReceiveBufferSize(udpMaxReceiveBufferSize),
	udpMaxSentMeasureSize(udpMaxSentMeasureSize),
	udpResendCount(udpResendCount),
	confirmationResendCount(confirmationResendCount),
	udpMaxSendLossPercent(udpMaxSendLossPercent),
	udpResendUs(udpResendUs),
	buildConfirmationsUs(buildConfirmationsUs),
	buildUdpPacketsUs(buildUdpPacketsUs),
	udpMaxSentMeasureUs(udpMaxSentMeasureUs),
	tcpNextSendIndex(),
	udpNextSendIndex(),
	udpReceivedMasterIndex(),
	udpConfirmedMasterIndex(),
	udpReceivedFinalIndex(INT_MAX),
	udpSentBufferSize(),
	udpReceiveBufferSize(),
	udpPacketsToSendCount(),
	remainConfirmationResendCount(),
	remainByeByeResendCount(),
	udpLastSentUs(),
	udpSendIntervalUs(udpInitialSendIntervalUs),
	udpSendIntervalUsFloat((double)udpInitialSendIntervalUs),
	udpSendMeasureIndex(),
	eventTcpRead(*this, server.eventManager, tcpSocket.sourceRead),
	eventTcpWrite(*this, server.eventManager, tcpSocket.sourceWrite),
	eventTcpClose(*this, server.eventManager, tcpSocket.sourceClose),
	eventUdpWrite(*this, server.eventManager, udpListener.getSocket().sourceWrite, udpListener.getTcpAddress().data.empty() ? 0 : 1),
	eventUdpClose(*this, server.eventManager, udpListener.getSocket().sourceClose),
	eventBuildUdpPackets(*this, server.eventManager),
	eventBuildConfirmations(*this, server.eventManager),
	eventBuildByeBye(*this, server.eventManager),
	eventUdpResend(*this, server.eventManager),
	eventUdpCloseWait(*this, server.eventManager),
	eventClose(*this, server.eventManager)
{
	udpSendMeasures[udpSendMeasureIndex].beginUs = Platform::nowUs();
	server.udpSummaryConnectionsSendIntervalUs += udpSendIntervalUs;

	Packet &packet = udpSentPackets[udpNextSendIndex];
	packet.remainResendCount = udpResendCount;
	packet.encode(Packet::Hello, udpNextSendIndex);
	++udpNextSendIndex;

	++udpPacketsToSendCount;
	onUdpSentBufferChanged(packet.getSize());
	eventUdpWrite.setTimeRelativeNow();
	eventTcpClose.setTimeRelativeNow();
	eventUdpClose.setTimeRelativeNow();
}

Connection::~Connection() {
	server->udpSummaryConnectionsSendIntervalUs -= udpSendIntervalUs;
	delete tcpSocket;
}

void Connection::handle(Event &event, long long plannedTimeUs) {
	if (&event == &eventTcpRead) {
		tcpRead();
	} else
	if (&event == &eventTcpWrite) {
		tcpWrite();
	} else
	if (&event == &eventTcpClose) {
		tcpClose(tcpSocket->wasError());
	} else
	if (&event == &eventUdpWrite) {
		udpWrite(plannedTimeUs);
	} else
	if (&event == &eventUdpClose) {
		udpClose(udpListener->getSocket().wasError());
	} else
	if (&event == &eventBuildUdpPackets) {
		buildUdpPackets(true);
	} else
	if (&event == &eventBuildConfirmations) {
		buildConfirmations();
	} else
	if (&event == &eventBuildByeBye) {
		buildByeBye();
	} else
	if (&event == &eventUdpResend) {
		udpResend();
	} else
	if (&event == &eventUdpCloseWait) {
		if (isUdpFinished()) udpClose();
	} else
	if (&event == &eventClose) {
		server->onConnectionClosed(*this);
	}
}

void Connection::tcpRead() {
	if (!tcpConnected) return;

	int prevSize = tcpReceivedData.size();
	int maxSize = udpSendPacketSize;
	tcpReceivedData.resize(tcpReceivedData.size() + maxSize);

	int size = tcpSocket->read(&tcpReceivedData[prevSize], maxSize);
	if (size > 0) {
		server->statTcpReceived += size;
		if (udpConnected) {
			tcpReceivedData.resize(prevSize + size);

			#ifdef DUMP_TCP_RECV_PACKETS
			std::cout << "[" << shortName << " received tcp-packet, size " << size << "]" << std::endl;
			#ifdef DUMP_TCP_RECV_PACKETS_DATA
			std::cout.write(&tcpReceivedData[prevSize], size);
			std::cout << std::endl << "[end]" << std::endl;
			#endif
			#endif

			buildUdpPackets(false);
		} else tcpReceivedData.resize(prevSize);
	} else tcpReceivedData.resize(prevSize);

	onUdpSentBufferChanged(0);
}

void Connection::tcpWrite(bool fake) {
	if (tcpConnected && fake) return;
	if (!tcpConnected && !fake) return;

	while(tcpNextSendIndex < udpReceivedMasterIndex) {
		std::map<int, Packet>::iterator i = udpReceivedPackets.find(tcpNextSendIndex);
		if (i == udpReceivedPackets.end()) break;

		Packet::Type type = i->second.getType();
		const void *data = i->second.getData();
		int size = i->second.getSize();

		if (fake || type != Packet::Data || size <= 0) {
			i->second.sent = true;
			if (i->second.isComplete()) {
				udpReceiveBufferSize -= size;
				udpReceivedPackets.erase(i);
			}
			++tcpNextSendIndex;

			if (!fake && type == Packet::Bye) {
				tcpClose();
				return;
			} else
			if (!fake && type == Packet::Disconnect) {
				// TODO: provide info about error to remote tcp-host
				tcpClose();
				return;
			}

			continue;
		}

		size = tcpSocket->write(data, size);
		if (size > 0) {
			server->statTcpSent += size;

			#ifdef DUMP_TCP_SENT_PACKETS
			std::cout << "[" << shortName << " sent tcp-packet (from udp #" << i->first << "), size " << size << "/" << i->second.getSize() << "]" << std::endl;
			#ifdef DUMP_TCP_SENT_PACKETS_DATA
			std::cout.write((const char *)data, size);
			std::cout << std::endl << "[end]" << std::endl;
			#endif
			#endif

			int dataIndex = (const char *)data - &i->second.data.front();
			i->second.data.erase(i->second.data.begin() + dataIndex, i->second.data.begin() + dataIndex + size);
			udpReceiveBufferSize -= size;
			if (i->second.getSize() <= 0) {
				i->second.sent = true;
				if (i->second.isComplete()) udpReceivedPackets.erase(i);
				++tcpNextSendIndex;
			}
		}

		if (tcpNextSendIndex < udpReceivedMasterIndex)
			eventTcpWrite.setTimeRelativeNow();
		break;
	}
}

void Connection::udpWrite(long long plannedTimeUs) {
	if (!udpConnected) return;

	if (!udpConfirmationPackets.empty()) {
		Packet &packet = udpConfirmationPackets.front();

		#ifdef SIMULATE_DAMAGE
		if (rand()%100 < (SIMULATE_DAMAGE)) packet.setCrc32(0);
		#endif

		int size = udpListener->getSocket().writeto(packet.getRawData(), udpAddress, packet.getRawSize(), name);
		if (size > 0 || packet.getRawSize() == 0) {
			server->statUdpSent += size;
			if (size != packet.getRawSize())
				server->log.warning(name, "confirmation udp-packet sent truncated %d/%d", size, packet.getRawSize());

			#ifdef DUMP_UDP_SENT_CONFIRMATIONS
			if (packet.getType() == Packet::Confirmation) {
				int a = 0, b = 0, currentIndex = packet.getIndex();
				const void *data = (const int *)packet.getData();
				int size = packet.getSize();
				std::cout << "[" << shortName << ", sent confirmations, master " << packet.getIndex() << ", indices ";
				while(Packet::unpackIntPair(a, b, data, size)) {
					currentIndex += a + b;
					std::cout << " " << (currentIndex - b) << "-" << (currentIndex-1);
				}
				std::cout << "]" << std::endl;
			}
			#endif

			#ifdef DUMP_UDP_SENT_BYEBYE
			if (packet.getType() == Packet::ByeBye)
				std::cout << "[" << shortName << ", sent bye-bye #" << packet.getIndex() << "]" << std::endl;
			#endif

			udpLastSentUs = plannedTimeUs;
			onUdpSentBufferChanged(-packet.getSize());
			udpConfirmationPackets.erase(udpConfirmationPackets.begin());
			if (isUdpFinished()) {
				udpClose();
				return;
			}
		}

		if (!udpConfirmationPackets.empty() || udpPacketsToSendCount > 0)
			eventUdpWrite.setTime(udpLastSentUs + udpSendIntervalUs);
		return;
	}

	for(std::map<int, Packet>::iterator i = udpSentPackets.begin(); i != udpSentPackets.end(); ++i) {
		if (!i->second.sent) {
			Packet &packet = i->second;

			#ifdef SIMULATE_DAMAGE
			unsigned int damagedCrc32 = packet.getCrc32();
			if (rand()%100 < (SIMULATE_DAMAGE)) packet.setCrc32(0);
			#endif

			int size = udpListener->getSocket().writeto(packet.getRawData(), udpAddress, packet.getRawSize(), name);

			#ifdef SIMULATE_DAMAGE
			packet.setCrc32(damagedCrc32);
			#endif

			if (size > 0 || packet.getRawSize() == 0) {
				server->statUdpSent += size;

				if (size != packet.getRawSize())
					server->log.warning(name, "udp-packet sent truncated %d/%d", size, packet.getRawSize());

				long long timeUs = Platform::nowUs();
				packet.sent = true;
				packet.sentTimeUs = timeUs;
				packet.measureIndex = udpSendMeasureIndex;
				udpLastSentUs = plannedTimeUs;

				onUdpSent(udpSendMeasureIndex, udpSendIntervalUs, packet.getSize());

				--udpPacketsToSendCount;

				#ifdef DUMP_UDP_SENT_HANDSHAKINGS
				switch(packet.getType()) {
				case Packet::Hello:
					std::cout << "[" << shortName << " sent hello]" << std::endl;
					break;
				case Packet::Bye:
					std::cout << "[" << shortName << " sent bye #" << packet.getIndex() << "]" << std::endl;
					break;
				case Packet::Disconnect:
					std::cout << "[" << shortName << " sent disconnect #" << packet.getIndex() << "]" << std::endl;
					break;
				default:
					break;
				}
				#endif

				#ifdef DUMP_UDP_SENT_PACKETS
				if (packet.getType() == Packet::Data) {
					std::cout << "[" << shortName << " sent udp-packet #" << packet.getIndex() << ", size " << packet.getSize() << "]" << std::endl;
					#ifdef DUMP_UDP_SENT_PACKETS_DATA
					std::cout.write((const char*)packet.getData(), packet.getSize());
					std::cout << std::endl << "[end]" << std::endl;
					#endif
				}
				#endif

				if (packet.isComplete()) {
					server->log.warning(name, "packet was confirmed before sent #%d", packet.getIndex());
					onUdpSentBufferChanged(-packet.getSize());
					udpSentPackets.erase(i);
					if (isUdpFinished()) {
						udpClose();
						return;
					}
				} else {
					eventUdpResend.setTimeRelativeNow(udpResendUs);
				}
			}

			if (udpPacketsToSendCount > 0)
				eventUdpWrite.setTime(udpLastSentUs + udpSendIntervalUs);
			return;
		}
	}

	//server->log.warning(name, "eventUdpWrite raised, but nothing to write");
}

void Connection::buildUdpPackets(bool flush) {
	if (!udpConnected) return;

	int fullSize = 0;
	for(int i = 0; i < (int)tcpReceivedData.size(); i += udpSendPacketSize) {
		int size = std::min((int)tcpReceivedData.size() - i, udpSendPacketSize);
		if (!flush && size < udpSendPacketSize) break;
		if (size <= 0) break;

		++udpPacketsToSendCount;

		Packet &packet = udpSentPackets[udpNextSendIndex];
		packet.remainResendCount = udpResendCount;
		packet.encode(Packet::Data, udpNextSendIndex, &tcpReceivedData[i], size);
		udpNextSendIndex++;

		setEventUdpWrite();

		onUdpSentBufferChanged(packet.getSize());
		fullSize += size;
	}
	tcpReceivedData.erase(tcpReceivedData.begin(), tcpReceivedData.begin() + fullSize);

	if (fullSize > 0 || tcpReceivedData.empty())
		eventBuildUdpPackets.disable();
	if (!tcpReceivedData.empty())
		eventBuildUdpPackets.setTimeRelativeNow(buildUdpPacketsUs);
}

void Connection::buildConfirmations() {
	if (!udpConnected) return;

	for(std::list<Packet>::iterator i = udpConfirmationPackets.begin(); i != udpConfirmationPackets.end();)
		if (i->getType() == Packet::Confirmation) {
			onUdpSentBufferChanged(-i->getSize());
			i = udpConfirmationPackets.erase(i);
		} else ++i;

	// pack confirmations
	confirmationData.resize(udpSendPacketSize);
	int prevIndex = udpReceivedMasterIndex;
	void *data = &confirmationData.front();
	int size = (int)confirmationData.size();
	for(std::map<int, Packet>::iterator i = udpReceivedPackets.lower_bound(udpReceivedMasterIndex); i != udpReceivedPackets.end();) {
		int begin = i->first;
		int end = begin + 1;
		++i;
		while(i != udpReceivedPackets.end() && i->first == end) ++i, ++end;

		if (!Packet::packIntPair(begin - prevIndex, end - begin, data, size)) {
			udpConfirmationPackets.push_back(Packet());
			Packet &packet = udpConfirmationPackets.back();
			packet.encode(Packet::Confirmation, udpReceivedMasterIndex, &confirmationData.front(), (int)confirmationData.size() - size);
			onUdpSentBufferChanged(packet.getSize());

			data = &confirmationData.front();
			size = (int)confirmationData.size();
			if (!Packet::packIntPair(begin - udpReceivedMasterIndex, end - begin, data, size)) {
				server->log.error(name, "cannot encode confirmation");
				break;
			}
		}

		prevIndex = end;
	}
	udpConfirmationPackets.push_back(Packet());
	Packet &packet = udpConfirmationPackets.back();
	packet.encode(Packet::Confirmation, udpReceivedMasterIndex, &confirmationData.front(), (int)confirmationData.size() - size);
	onUdpSentBufferChanged(packet.getSize());

	udpConfirmedMasterIndex = udpReceivedMasterIndex;
	setEventUdpWrite();

	#ifdef CHECK_CONFIRMATIONS
	bool success = true;
	std::set<int> unconfirmedIndices;
	for(std::map<int, Packet>::const_iterator i = udpReceivedPackets.lower_bound(udpReceivedMasterIndex); i != udpReceivedPackets.end(); ++i)
		unconfirmedIndices.insert(i->first);
	for(std::list<Packet>::const_iterator i = udpConfirmationPackets.begin(); i != udpConfirmationPackets.end(); ++i) {
		if (i->getType() == Packet::Confirmation) {
			int a = 0, b = 0, currentIndex = i->getIndex();
			if (currentIndex != udpReceivedMasterIndex) {
				server->log.error(name, "buildConfirmations: wrong master index %d (should be %d)", currentIndex, udpReceivedMasterIndex);
				success = false;
			}
			const void *data = i->getData();
			int size = i->getSize();
			while(Packet::unpackIntPair(a, b, data, size)) {
				currentIndex += a + b;
				for(int j = currentIndex - b; j < currentIndex; ++j) {
					if (udpReceivedPackets.count(j) == 0)
						{ server->log.error(name, "buildConfirmations: wrong index %d", j); success = false; }
					else
					if (unconfirmedIndices.count(j) == 0)
						{ server->log.error(name, "buildConfirmations: reconfirmed %d", j); success = false; }
					unconfirmedIndices.erase(j);
				}
			}
			if (size)
				{ server->log.error(name, "buildConfirmations: wrong format"); success = false; }
		}
	}
	for(std::set<int>::const_iterator i = unconfirmedIndices.begin(); i != unconfirmedIndices.end(); ++i)
		{ server->log.error(name, "buildConfirmations: unconfirmed index %d", *i); success = false; }
	if (!success) {
		for(std::list<Packet>::const_iterator i = udpConfirmationPackets.begin(); i != udpConfirmationPackets.end(); ++i) {
			if (i->getType() == Packet::Confirmation) {
				int a = 0, b = 0, currentIndex = i->getIndex();
				const void *data = (const int *)i->getData();
				int size = i->getSize();
				std::cout << "[" << shortName << ", prepared confirmations, master " << i->getIndex() << ", indices ";
				while(Packet::unpackIntPair(a, b, data, size)) {
					currentIndex += a + b;
					std::cout << " " << (currentIndex - b) << "-" << (currentIndex-1);
				}
				std::cout << "]" << std::endl;
			}
		}
	}
	#endif

	// TODO: optimize - quick erase items with index < tcpNextSendIndex
	for(std::map<int, Packet>::iterator i = udpReceivedPackets.begin(); i != udpReceivedPackets.end() && i->first < tcpNextSendIndex;) {
		i->second.confirmed = i->first < udpReceivedMasterIndex;
		if (i->second.isComplete()) {
			udpReceiveBufferSize -= i->second.getSize();
			udpReceivedPackets.erase(i++);
		} else ++i;
	}

	--remainConfirmationResendCount;
	if (remainConfirmationResendCount > 0)
		eventBuildConfirmations.setTimeRelativeNow(buildConfirmationsUs);
}

void Connection::buildByeBye() {
	if (!udpConnected) return;

	udpConfirmationPackets.push_back(Packet());
	Packet &packet = udpConfirmationPackets.back();
	packet.encode(Packet::ByeBye, udpNextSendIndex);

	onUdpSentBufferChanged(packet.getSize());
	setEventUdpWrite();

	--remainByeByeResendCount;
	if (remainByeByeResendCount > 0)
		eventBuildByeBye.setTimeRelativeNow(buildConfirmationsUs);
}

void Connection::udpResend() {
	if (!udpConnected) return;

	long long timeUs = Platform::nowUs();
	for(std::map<int, Packet>::iterator i = udpSentPackets.begin(); i != udpSentPackets.end(); ++i) {
		if (i->second.sent) {
			if (i->second.sentTimeUs + udpResendUs <= timeUs) {
				if (i->second.remainResendCount > 0) {
					++udpPacketsToSendCount;

					i->second.sent = false;
					i->second.remainResendCount--;

					#ifdef DUMP_UDP_RESEND
					std::cout << "[" << shortName << " resend udp-packet #" << i->second.getIndex() << ", size " << i->second.getSize() << "]" << std::endl;
					#endif

					onUdpDelivered(false, i->second.measureIndex, i->second.getSize());
					setEventUdpWrite();
				} else {
					server->log.error(name, "remote udp host don't answers");
					udpClose(true);
					return;
				}
			} else {
				eventUdpResend.setTime(i->second.sentTimeUs + udpResendUs);
			}
		}
	}
}

void Connection::udpRead(const Packet &packet) {
	if (!udpConnected) return;

	if (packet.getType() == Packet::Confirmation) {
		bool prevNoMoreDataWillBeSent = isNoMoreDataWillBeSent();

		int masterIndex = packet.getIndex();
		const void *data = (const int *)packet.getData();
		int size = packet.getSize();

		#ifdef DUMP_UDP_RECV_CONFIRMATIONS
		{
			int a = 0, b = 0, currentIndex = packet.getIndex();
			const void *data = (const int *)packet.getData();
			int size = packet.getSize();
			std::cout << "[" << shortName << ", received confirmations, master " << packet.getIndex() << ", indices ";
			while(Packet::unpackIntPair(a, b, data, size)) {
				currentIndex += a + b;
				std::cout << " " << (currentIndex - b) << "-" << (currentIndex-1);
			}
			std::cout << "]" << std::endl;
		}
		#endif

		for(std::map<int, Packet>::iterator i = udpSentPackets.begin(); i != udpSentPackets.end();) {
			if (i->first < masterIndex) {
				if (!i->second.confirmed)
					onUdpDelivered(true, i->second.measureIndex, i->second.getSize());
				if (!i->second.sent)
					--udpPacketsToSendCount;

				i->second.sent = true;
				i->second.confirmed = true;
				if (i->second.isComplete()) {
					onUdpSentBufferChanged(-i->second.getSize());
					udpSentPackets.erase(i++);
					continue;
				}
			} else break;
			++i;
		}

		int currentIndex = masterIndex;
		int a, b;
		while(Packet::unpackIntPair(a, b, data, size)) {
			currentIndex += a + b;
			for(int i = currentIndex - b; i < currentIndex; ++i) {
				std::map<int, Packet>::iterator j = udpSentPackets.find(i);
				if (j != udpSentPackets.end()) {
					if (!j->second.confirmed)
						onUdpDelivered(true, j->second.measureIndex, j->second.getSize());
					if (!j->second.sent)
						--udpPacketsToSendCount;

					j->second.sent = true;
					j->second.confirmed = true;
					if (j->second.isComplete()) {
						onUdpSentBufferChanged(-j->second.getSize());
						udpSentPackets.erase(j);
					}
				}
			}
		}
		if (size)
			server->log.warning(name, "received confirmation packet in wrong format");

		server->statUdpReceivedExtra += packet.getRawSize();

		if (udpPacketsToSendCount >= (int)udpSentPackets.size())
			eventUdpResend.disable();
		//if (udpPacketsToSendCount <= 0 && udpConfirmationPackets.empty())
		//	eventUdpWrite.disable();

		if (isNoMoreDataWillBeSent() && !prevNoMoreDataWillBeSent) {
			remainByeByeResendCount = confirmationResendCount;
			eventBuildByeBye.setTimeRelativeNow();
		}
	} else
	if ( packet.getIndex() >= udpReceivedMasterIndex
	  && packet.getIndex() < udpReceivedFinalIndex
	  //&& (packet.getIndex() - tcpNextSendIndex <= udpMaxReceiveBufferSize/udpSendPacketSize)
	  //&& (udpReceiveBufferSize < udpMaxReceiveBufferSize || (tcpNextSendIndex == udpReceivedMasterIndex && packet.getIndex() == udpReceivedMasterIndex))
	  && ( (packet.getType() == Packet::Hello && packet.getIndex() == 0)
	    || packet.getType() == Packet::Bye
	    || packet.getType() == Packet::Disconnect
	    || packet.getType() == Packet::Data ))
	{
		if (!udpReceivedPackets.count(packet.getIndex())) {
			#ifdef DUMP_UDP_RECV_HANDSHAKINGS
			if (packet.getType() == Packet::Hello)
				std::cout << "[" << shortName << " received hello]" << std::endl;
			else
			if (packet.getType() == Packet::Bye)
				std::cout << "[" << shortName << " received bye #" << packet.getIndex() << "]" << std::endl;
			else
			if (packet.getType() == Packet::Disconnect)
				std::cout << "[" << shortName << " received disconnect #" << packet.getIndex() << "]" << std::endl;
			#endif

			#ifdef DUMP_UDP_RECV_PACKETS
			if (packet.getType() == Packet::Data) {
				std::cout << "[" << shortName << " received udp-packet #" << packet.getIndex() << ", size " << packet.getSize() << "]" << std::endl;
				#ifdef DUMP_UDP_RECV_PACKETS_DATA
				std::cout.write((const char*)packet.getData(), packet.getSize());
				std::cout << std::endl << "[end]" << std::endl;
				#endif
			}
			#endif

			Packet &newPacket = udpReceivedPackets[packet.getIndex()];
			newPacket = packet;
			udpReceiveBufferSize +=	newPacket.getSize();

			while(udpReceivedPackets.count(udpReceivedMasterIndex) && udpReceivedMasterIndex < udpReceivedFinalIndex) {
				Packet::Type type = udpReceivedPackets[udpReceivedMasterIndex].getType();
				++udpReceivedMasterIndex;
				if (type == Packet::Bye || type == Packet::Disconnect)
					udpReceivedFinalIndex = udpReceivedMasterIndex;
			}
			tcpWrite(true);

			if (tcpNextSendIndex < udpReceivedMasterIndex)
				eventTcpWrite.setTimeRelativeNow();
			if (udpReceivedMasterIndex >= udpReceivedFinalIndex)
				eventUdpCloseWait.setTimeRelativeNow(udpResendUs*udpResendCount + udpResendUs);
		} else {
			server->statUdpReceivedExtra += packet.getRawSize();
		}
		remainConfirmationResendCount = confirmationResendCount;
		eventBuildConfirmations.setTimeRelativeNow(buildConfirmationsUs);
	} else
	if ( packet.getType() == Packet::ByeBye
	  && packet.getIndex() == udpReceivedMasterIndex
	  && packet.getIndex() == udpReceivedFinalIndex )
	{
		#ifdef DUMP_UDP_RECV_BYEBYE
		std::cout << "[" << shortName << " received bye-bye #" << packet.getIndex() << "]" << std::endl;
		#endif

		remainConfirmationResendCount = 0;
		eventBuildConfirmations.disable();
		for(std::list<Packet>::iterator i = udpConfirmationPackets.begin(); i != udpConfirmationPackets.end();)
			if (i->getType() == Packet::Confirmation) {
				onUdpSentBufferChanged(-i->getSize());
				i = udpConfirmationPackets.erase(i);
			} else ++i;

		//if (udpPacketsToSendCount <= 0 && udpConfirmationPackets.empty())
		//	eventUdpWrite.disable();
		if (eventUdpCloseWait.isEnabled())
			eventUdpCloseWait.setTimeRelativeNow(udpResendUs*confirmationResendCount/3 + udpResendUs);
	} else {
		server->statUdpReceivedExtra += packet.getRawSize();
	}

	if (isUdpFinished()) {
		udpClose();
		return;
	}
}

bool Connection::isNoMoreDataWillBeSent() {
	return !tcpConnected
		&& tcpReceivedData.empty()
		&& udpSentPackets.empty();
}

bool Connection::isUdpFinished() {
	if (!udpConnected) return true;
	return isNoMoreDataWillBeSent()
		&& udpConfirmationPackets.empty()
		&& udpConfirmedMasterIndex >= udpReceivedMasterIndex
		&& tcpNextSendIndex >= udpReceivedMasterIndex
		&& remainConfirmationResendCount <= 0
		&& remainByeByeResendCount <= 0
		&& !eventUdpCloseWait.isEnabled();
}

void Connection::onUdpSentBufferChanged(int sizeIncrement) {
	udpSentBufferSize += sizeIncrement;
	int count = udpSentPackets.empty() ? 0
			  : udpSentPackets.end()->first - udpSentPackets.begin()->first;
	int maxCount = (int)(10ll*1000000ll/udpSendIntervalUs);
	maxCount = std::min(maxCount, udpMaxSentBufferSize/udpSendPacketSize);
	if (count <= maxCount)
		eventTcpRead.setTimeRelativeNow();
	else
		eventTcpRead.disable();
}

void Connection::onUdpSent(int measureIndex, long long intervalUs, int size) {
	if (measureIndex != udpSendMeasureIndex) return;
	std::map<int, Measure>::iterator i = udpSendMeasures.find(udpSendMeasureIndex);
	if (i == udpSendMeasures.end()) return;

	++i->second.count;
	i->second.size += size;
	i->second.summaryIntervalUs += intervalUs;
	long long timeUs = Platform::nowUs();
	if (i->second.size >= udpMaxSentMeasureSize || timeUs - i->second.beginUs >= udpMaxSentMeasureUs) {
		i->second.endUs = timeUs;
		udpSendMeasures[++udpSendMeasureIndex].beginUs = timeUs;
	}
}

void Connection::onUdpDelivered(bool success, int measureIndex, int size) {
	std::map<int, Measure>::iterator i = udpSendMeasures.find(measureIndex);
	if (i == udpSendMeasures.end()) return;

	(success ? i->second.successSize : i->second.failSize) += size;

	if ( i->second.beginUs >= 0
	  && i->second.endUs >= i->second.beginUs
	  && i->second.count > 0
	  && i->second.successSize + i->second.failSize >= i->second.size
	) {
		double durationUs = (double)(i->second.endUs - i->second.beginUs);
		double intervalUs = (double)i->second.summaryIntervalUs/(double)i->second.count;
		double actualIntervalUs = durationUs/(double)i->second.count;

		double successPart = (double)i->second.successSize/(double)i->second.size;

		double speedAmplifier = (double)successPart/(1.0 - 0.01*udpMaxSendLossPercent);
		if (i->second.failSize <= 0) speedAmplifier = 2.0;
		if (speedAmplifier > 1.0 && actualIntervalUs > (intervalUs + 1.0)*2.0) speedAmplifier = 1.0;
		if (speedAmplifier < 0.5) speedAmplifier = 0.50;
		if (speedAmplifier > 2.0) speedAmplifier = 2.0;

		udpSendIntervalUsFloat = intervalUs/speedAmplifier;

		// add constant speed to autobalance connections
		double maxIntervalUs = (double)(udpResendUs/3);
		double addSpeed = 1.0/maxIntervalUs; // packets/usec

		udpSendIntervalUsFloat = udpSendIntervalUsFloat/(1.0 + addSpeed*udpSendIntervalUsFloat);

		if (udpSendIntervalUsFloat < 0.1) udpSendIntervalUsFloat = 0.1;
		if (udpSendIntervalUsFloat > maxIntervalUs) udpSendIntervalUsFloat = maxIntervalUs;

		server->udpSummaryConnectionsSendIntervalUs -= udpSendIntervalUs;
		udpSendIntervalUs = (long long)round(udpSendIntervalUsFloat);
		server->udpSummaryConnectionsSendIntervalUs += udpSendIntervalUs;

		#ifdef DUMP_UDP_INTERVAL
		std::cout << "[" << shortName << " udp send interval " << udpSendIntervalUs << ", us]" << std::endl;
		#endif

		while(i != udpSendMeasures.begin())
			udpSendMeasures.erase(i--);
		udpSendMeasures.erase(i);
	}
}

void Connection::setEventUdpWrite() {
	if (!eventUdpWrite.isEnabled())
		udpLastSentUs = std::max(udpLastSentUs, Platform::nowUs() - udpSendIntervalUs);
	eventUdpWrite.setTime(udpLastSentUs + udpSendIntervalUs);
}

void Connection::tcpClose(bool error) {
	if (!tcpConnected) return;
	tcpConnected = false;

	#ifdef LOG_STATE
	server->log.info(name, "disconnect tcp");
	#endif

	eventTcpRead.disable();
	eventTcpWrite.disable();
	tcpSocket->close();

	if (!udpConnected) {
		eventClose.setTimeRelativeNow();
		return;
	}

	tcpWrite(true);
	buildUdpPackets(true);

	Packet &packet = udpSentPackets[udpNextSendIndex];
	packet.remainResendCount = udpResendCount;
	packet.encode(error ? Packet::Disconnect : Packet::Bye, udpNextSendIndex);
	udpNextSendIndex++;

	++udpPacketsToSendCount;

	onUdpSentBufferChanged(packet.getSize());

	setEventUdpWrite();
}

void Connection::udpClose(bool /* error */) {
	if (!udpConnected) return;
	udpConnected = false;

	#ifdef LOG_STATE
	server->log.info(name, "disconnect udp");
	#endif

	eventBuildUdpPackets.disable();
	eventBuildConfirmations.disable();
	eventUdpResend.disable();
	eventTcpRead.disable();
	eventUdpWrite.disable();
	tcpSocket->closeRead();

	tcpReceivedData.clear();
	udpConfirmationPackets.clear();
	udpSentPackets.clear();
	onUdpSentBufferChanged(-udpSentBufferSize);

	if (!tcpConnected) {
		eventClose.setTimeRelativeNow();
		return;
	}

	if (tcpNextSendIndex >= udpReceivedMasterIndex)
		tcpClose();
}
