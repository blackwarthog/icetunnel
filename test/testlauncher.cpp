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

#include "testlauncher.h"
#include "test.h"
#include "testbenchmark.h"
#include "testsimpletcp.h"
#include "testtransfer.h"

bool TestLauncher::launchAll(Log &log, const Address &tcpRemoteAddress, const Address &udpRemoteAddress) {
	bool success = true;
	std::string name = "test::launchAll";
	log.info(name, "begin");


	success &= TestSimpleTcp(log).launch();
	success &= TestTransfer(log).launch();
	success &= TestBenchmark(log,  true, false).launch();
	success &= TestBenchmark(log, false, false).launch();
	success &= TestBenchmark(log,  true,  true).launch();
	success &= TestBenchmark(log, false,  true).launch();
	if (!tcpRemoteAddress.data.empty()) {
		success &= TestBenchmark(log,  true, false, tcpRemoteAddress).launch();
		success &= TestBenchmark(log, false, false, tcpRemoteAddress).launch();
	}
	if (!udpRemoteAddress.data.empty()) {
		success &= TestBenchmark(log,  true, true, udpRemoteAddress).launch();
		success &= TestBenchmark(log, false, true, udpRemoteAddress).launch();
	}


	if (success)
		log.info(name, "success");
	else
		log.error(name, "failed");
	log.info(name, "end");
	return success;
}
