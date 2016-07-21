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

#include "test.h"


bool Test::launch() {
	log->info(name, "begin");
	success = true;
	run();
	if (success)
		log->info(name, "success");
	else
		log->error(name, "failed");
	log->info(name, "end");
	return success;
}

void Test::buildRandomBuffer(Buffer &buffer, int size) {
	buffer.resize(size);
	srand((int)time(NULL));
	for(Buffer::iterator i = buffer.begin(); i != buffer.end(); ++i)
		*i = (char)(rand()%256 - 128);
}

void Test::popToBuffer(SimpleTcpClient &simpleTcpClient, Buffer &buffer) {
	buffer.resize(simpleTcpClient.size());
	if (!buffer.empty())
		simpleTcpClient.pop(&buffer.front(), buffer.size());
}

void Test::pushFromBuffer(SimpleTcpClient &simpleTcpClient, const Buffer &buffer) {
	if (!buffer.empty())
		simpleTcpClient.push(&buffer.front(), buffer.size());
}

bool Test::compareBuffers(const Buffer &a, const Buffer &b) {
	if (a.size() != b.size()) return false;
	return a.empty() || b.empty()
		|| memcmp(&a.front(), &b.front(), a.size()) == 0;
}
