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

#ifndef _TESTLAUNCHER_H_
#define _TESTLAUNCHER_H_

#include "../address.h"
#include "../log.h"


class TestLauncher {
public:
	static bool launchAll(Log &log, const Address &tcpRemoteAddress = Address(), const Address &udpRemoteAddress = Address());
};

#endif
