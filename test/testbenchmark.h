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

#ifndef _TESTBENCHMARK_H_
#define _TESTBENCHMARK_H_

#include "test.h"


class TestBenchmark: public Test {
private:
	bool single;
	bool tunnel;
	Address remoteAddress;
public:
	explicit TestBenchmark(Log &log, bool single = false, bool tunnel = false, const Address &remoteAddress = Address()):
		Test( std::string("benchmark")
			+ (single ? "(single)" : "")
			+ (tunnel ? "(tunnel)" : "")
			+ (remoteAddress.data.empty() ? "" : "(" + remoteAddress.toString() + ")"),
			log ),
		single(single),
		tunnel(tunnel),
		remoteAddress(remoteAddress) { }
protected:
	void run();
};

#endif
