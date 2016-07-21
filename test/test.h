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

#ifndef _TEST_H_
#define _TEST_H_

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <climits>

#include <vector>
#include <set>

#include "../log.h"
#include "../platform.h"
#include "../server.h"

#include "random.h"
#include "simpletcpserver.h"
#include "benchmarktcpserver.h"


class Test {
public:
	typedef std::vector<char> Buffer;

protected:
	std::string name;
	Log *log;
	bool success;

	virtual void run() { }

public:
	explicit Test(const std::string &name, Log &log):
		name(name), log(&log), success(true) { }
	virtual ~Test() { }
	bool launch();

	static void buildRandomBuffer(Buffer &buffer, int size);
	static void popToBuffer(SimpleTcpClient &simpleTcpClient, Buffer &buffer);
	static void pushFromBuffer(SimpleTcpClient &simpleTcpClient, const Buffer &buffer);
	static bool compareBuffers(const Buffer &a, const Buffer &b);
};

#endif
