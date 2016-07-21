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

#ifndef _BENCHMARKTCPCLIENT_H_
#define _BENCHMARKTCPCLIENT_H_

#include <set>
#include <map>

#include "simpletcpclient.h"


class BenchmarkTcpClient: public SimpleTcpClient {
public:
	typedef std::set<BenchmarkTcpClient*> Container;
	typedef std::pair<int, long long> StatEntry;
	typedef std::vector<StatEntry> StatList;

private:
	Event eventClose;
	Container *container;
	StatList *stats;

	bool server;
	int requestsCount;
	int requestSize;
	int answerSize;

	int requestsCountRemain;
	int requestSizeRemain;
	int answerSizeRemain;
	unsigned int currentSeed;
	unsigned int currentCrc32;

	long long beginUs;
	long long endUs;
	int receviedSize;

public:
	BenchmarkTcpClient(
		Socket &socket,
		Event::Manager &eventManager,
		Container &container,
		StatList &stats,
		int requestsCount = 0,
		int requestSize = 0,
		int answerSize = 0 );
	~BenchmarkTcpClient();

	void handle(Event &event, long long plannedTimeUs);

private:
	bool hasInt() const { return readBuffer.size() >= (int)sizeof(int); }

	int popInt() {
		if (!hasInt()) return 0;
		int value = 0;
		pop(&value, sizeof(value));
		return value;
	}

	void pushInt(int value)
		{ push(&value, sizeof(value)); }

	void buildWriteChunk();
	void formatError();

	double getSpeed() const;
};

#endif
