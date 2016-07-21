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

#include <cerrno>

#include <sys/time.h>

#include "platform.h"
#include "main.h"
#include "server.h"
#include "socket.h"


class Platform::Internal { };


long long Platform::nowUs() {
	timeval tp;
	memset(&tp, 0, sizeof(tp));
	gettimeofday(&tp, NULL);
	return 1000000ll*(long long)tp.tv_sec + (long long)tp.tv_usec;
}

int Platform::lastError() {
	return errno;
}

std::string Platform::lastErrorString(int err) {
	return strerror(err);
}

int Platform::main(int argc, char **argv) {
	Server server("main");
	if (init(server, argc, argv))
		server.run();
	return 0;
}
