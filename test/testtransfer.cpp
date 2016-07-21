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

#include "testtransfer.h"


void TestTransfer::run() {
	Address addressClient("127.0.0.1:2234");
	Address addressTunnel("127.0.0.1:2235");
	Address addressServer("127.0.0.1:2236");

	Server server(name + "(server)");
	server.createTcpListener(addressClient, addressTunnel);
	server.createUdpListener(addressTunnel, addressServer);
	server.begin();

	{
		SimpleTcpServer simpleTcpServer(
			server.socketGroup,
			server.eventManager,
			name + "(simpleTcpServer)",
			addressServer );

		Socket *socket = new Socket(server.socketGroup, name + "(clientSocket)", Socket::TCP);
		socket->connect(addressClient);
		SimpleTcpClient simpleTcpClient(*socket, server.eventManager);

		int bytesCount = 1024*1024 + 13;

		Buffer clientWriteBuffer;
		buildRandomBuffer(clientWriteBuffer, bytesCount);
		pushFromBuffer(simpleTcpClient, clientWriteBuffer);

		Buffer serverWriteBuffer;
		buildRandomBuffer(serverWriteBuffer, bytesCount);
		SimpleTcpClient *serverClient = NULL;

		long long durationUs = 10000000;
		long long endUs = Platform::nowUs() + durationUs;
		while(Platform::nowUs() < endUs) {
			server.step();
			if (!serverClient && !simpleTcpServer.clients.empty()) {
				serverClient = *simpleTcpServer.clients.begin();
				pushFromBuffer(*serverClient, serverWriteBuffer);
			}
			if ( serverClient
			  && serverClient->size() == (int)clientWriteBuffer.size()
			  && simpleTcpClient.size() == (int)serverWriteBuffer.size() )
				endUs = std::min(endUs, Platform::nowUs() + 1000000);
		}

		if (serverClient) {
			Buffer readBuffer;
			popToBuffer(*serverClient, readBuffer);
			if (!compareBuffers(clientWriteBuffer, readBuffer)) {
				log->error(name, "client to server transfer failed, processed bytes %d/%d", readBuffer.size(), clientWriteBuffer.size());
				success = false;
			}
			popToBuffer(simpleTcpClient, readBuffer);
			if (!compareBuffers(serverWriteBuffer, readBuffer)) {
				log->error(name, "server to client transfer failed, processed bytes %d/%d", readBuffer.size(), serverWriteBuffer.size());
				success = false;
			}
		} else {
			log->error(name, "server don't see the client");
			success = false;
		}
	}

	server.stepWhile(server.buildConfirmationsUs*server.confirmationResendCount + 4*server.udpResendUs);
	server.end();
}
