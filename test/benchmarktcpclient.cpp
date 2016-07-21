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

#include <algorithm>
#include <vector>

#include "../packet.h"
#include "../platform.h"

#include "benchmarktcpclient.h"
#include "random.h"


BenchmarkTcpClient::BenchmarkTcpClient(
	Socket &socket,
	Event::Manager &eventManager,
	Container &container,
	StatList &stats,
	int requestsCount,
	int requestSize,
	int answerSize
):
	SimpleTcpClient(socket, eventManager),
	eventClose(*this, eventManager, socket.sourceClose),
	container(&container),
	stats(&stats),
	server(requestsCount == 0),
	requestsCount(server ? 0 : requestsCount),
	requestSize(server ? 0 : requestSize),
	answerSize(server ? 0 : answerSize),
	requestsCountRemain(requestsCount),
	requestSizeRemain(),
	answerSizeRemain(),
	currentSeed((unsigned int)Platform::nowUs()%RAND_MAX),
	currentCrc32(),
	beginUs(Platform::nowUs()),
	endUs(-1),
	receviedSize()
{
	container.insert(this);
	eventClose.setTimeRelativeNow();
	if (!server) {
		if (requestsCountRemain <= 0 || requestSize <= 0 || answerSize <= 0)
			formatError(); else buildWriteChunk();
	}
}

BenchmarkTcpClient::~BenchmarkTcpClient() {
	if (endUs <= beginUs || receviedSize <= 0)
		stats->push_back(StatEntry(0, 0));
	else
		stats->push_back(StatEntry(receviedSize, endUs - beginUs));
	container->erase(this);
}

void BenchmarkTcpClient::handle(Event &event, long long) {
	if (&event == &eventRead) {
		int prevSize = readBuffer.size();
		readBuffer.reserve(1024);
		readBuffer.push(socket->read(readBuffer.end(), 1024));
		receviedSize += readBuffer.size() - prevSize;
		eventRead.setTimeRelativeNow();

		if (readBuffer.empty()) return;
		if (!writeBuffer.empty())
			{ formatError(); return; }

		if (server) {
			if (hasInt() && !requestsCount) {
				requestsCount = popInt();
				if (requestsCount <= 0)
					{ formatError(); return; }
				requestsCountRemain = requestsCount;
			}
			if (hasInt() && !requestSize) {
				requestSize = popInt();
				if (requestSize <= 0)
					{ formatError(); return; }
				if (requestsCountRemain <= 0)
					{ formatError(); return; }
				--requestsCountRemain;
			}
			if (hasInt() && !answerSize) {
				answerSize = popInt();
				if (answerSize <= 0)
					{ formatError(); return; }
				requestSizeRemain = requestSize;
			}
			if (requestSizeRemain > 0) {
				currentCrc32 = Packet::crc32(readBuffer.begin(), readBuffer.size(), currentCrc32);
				requestSizeRemain -= readBuffer.size();
				readBuffer.clear();
				if (requestSizeRemain < 0)
					{ formatError(); return; }
				if (requestSizeRemain == 0) {
					currentSeed = currentCrc32;
					currentCrc32 = 0;
					answerSizeRemain = answerSize;
					buildWriteChunk();
				}
			}
		} else {
			while(!empty()) {
				if (answerSizeRemain <= 0 || pop() != (char)((int)random(currentSeed)%256 - 128))
					{ formatError(); return; }
				--answerSizeRemain;
			}
			if (answerSizeRemain == 0) {
				if (requestsCountRemain == 0)
					{ socket->close(); return; }
				requestSizeRemain = requestSize;
				buildWriteChunk();
			}
		}
	} else
	if (&event == &eventWrite) {
		if (writeBuffer.empty()) return;
		if (!readBuffer.empty())
			{ formatError(); return ; }

		if (!writeBuffer.empty())
			writeBuffer.pop(socket->write(writeBuffer.begin(), writeBuffer.size()));
		buildWriteChunk();
		if (!writeBuffer.empty())
			eventWrite.setTimeRelativeNow();
	} else
	if (&event == &eventClose) {
		if ( !socket->wasError()
		  && requestsCountRemain == 0
		  && requestSizeRemain == 0
		  && answerSizeRemain == 0
		  && readBuffer.empty()
		  && writeBuffer.empty())
			endUs = Platform::nowUs();
		delete this;
	}
}

void BenchmarkTcpClient::formatError() {
	socket->getGroup().getLog().error(socket->getName() + "(benchmarkTcpClient)", "format error");
	socket->close(true);
}

void BenchmarkTcpClient::buildWriteChunk() {
	if (writeBuffer.empty()) {
		if (server) {
			if (answerSizeRemain > 0) {
				int size = std::min(answerSizeRemain, 1024);
				writeBuffer.reserve(size);
				for(char *c = (char*)writeBuffer.end(), *end = c + size; c != end; ++c)
					*c = (int)random(currentSeed)%256 - 128;
				writeBuffer.push(size);
				answerSizeRemain -= size;
				eventWrite.setTimeRelativeNow();
				if (answerSizeRemain <= 0) {
					requestSize = -1;
					answerSize = -1;
				}
			}
		} else {
			int size = std::min(requestSize + 3*(int)sizeof(int), 1024);
			writeBuffer.reserve(size);
			if (requestsCountRemain == requestsCount) {
				pushInt(requestsCount);
				requestSizeRemain = requestSize;
			}
			if (requestSizeRemain == requestSize) {
				if (requestsCountRemain <= 0 || requestSize <= 0 || answerSize <= 0)
					{ formatError(); return; }
				pushInt(requestSize);
				pushInt(answerSize);
				--requestsCountRemain;
			}
			if (requestSizeRemain > 0) {
				size -= writeBuffer.size();
				size = std::min(requestSizeRemain, size);
				if (size > 0) {
					for(char *c = (char*)writeBuffer.end(), *end = c + size; c != end; ++c)
						*c = (int)random(currentSeed)%256 - 128;
					currentCrc32 = Packet::crc32(writeBuffer.end(), size, currentCrc32);
					writeBuffer.push(size);
					requestSizeRemain -= size;
				}
				if (requestSizeRemain <= 0) {
					answerSizeRemain = answerSize;
					currentSeed = currentCrc32;
					currentCrc32 = 0;
				}
				eventWrite.setTimeRelativeNow();
			}
		}
	}
}


double BenchmarkTcpClient::getSpeed() const {
	if (endUs <= beginUs || receviedSize <= 0)
		return 0.0;
	return 1000000.0*(double)receviedSize/(double)(endUs - beginUs);
}
