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

#include "testbenchmark.h"


void TestBenchmark::run() {
	int maxClients = 100;
	long long initializeUs = 300000000ll;
	long long durationUs   = 600000000ll;
	long long reportUs     =  10000000ll;
	double size = 1.0;

	Address addressClient("127.0.0.1:2237");
	Address addressTunnel("127.0.0.1:2238");
	Address addressServer("127.0.0.1:2239");

	if (tunnel && remoteAddress.data.empty()) {
		initializeUs = 150000000ll;
		durationUs   = 300000000ll;
	} else
	if (tunnel && !remoteAddress.data.empty()) {
		addressTunnel = remoteAddress;
		addressServer.data.clear();
	} else
	if (!tunnel && remoteAddress.data.empty()) {
		initializeUs = 30000000ll;
		durationUs   = 60000000ll;
		size = 16.0;
		addressClient = addressServer;
		addressTunnel.data.clear();
	} else
	if (!tunnel && !remoteAddress.data.empty()) {
		addressClient = remoteAddress;
		addressTunnel.data.clear();
		addressServer.data.clear();
	}

	if (single) {
		size *= 0.25*maxClients;
		maxClients = 1;
	}

	Server server(name + "(server)");
	if (single && remoteAddress.data.empty())
		server.udpMaxSentBufferSize = 64*1024*1024;
	if (!single)
		server.udpInitialSendIntervalUs *= 10;//*maxClients;
	if (!addressClient.data.empty() && !addressTunnel.data.empty())
		server.createTcpListener(addressClient, addressTunnel);
	if (!addressTunnel.data.empty() && !addressServer.data.empty())
		server.createUdpListener(addressTunnel, addressServer);
	server.begin();

	BenchmarkTcpServer *benchmarkTcpServer = NULL;
	if (!addressServer.data.empty())
		benchmarkTcpServer = new BenchmarkTcpServer(
			server.socketGroup,
			server.eventManager,
			name + "(simpleTcpServer)",
			addressServer );

	srand((unsigned int)(Platform::nowUs()%RAND_MAX));
	BenchmarkTcpClient::Container clients;
	BenchmarkTcpClient::StatList stats;
	long long endInitializationUs = Platform::nowUs() + initializeUs;
	long long endUs = endInitializationUs + durationUs;
	long long nextReportUs = Platform::nowUs() + reportUs;
	bool initialized = false;

	log->info(name, "block size %f-%f Mb", size, 2.0*size);
	log->info(name, "initialize in %f seconds", 0.000001*(double)initializeUs);
	while(Platform::nowUs() < endUs) {
		if (Platform::nowUs() >= nextReportUs) {
			nextReportUs = Platform::nowUs() + reportUs;
			long long remainUs = (initialized ? endUs : endInitializationUs) - Platform::nowUs();
			log->info(name, "%f seconds remain", ::round(0.000001*(double)remainUs));
		}

		server.step();

		int currentMaxClients = initialized ? maxClients
				              : 2*(maxClients - (int)(maxClients*(endInitializationUs - Platform::nowUs())/initializeUs));
		if (currentMaxClients < 1) currentMaxClients = 1;
		if (currentMaxClients > maxClients) currentMaxClients = maxClients;

		while((int)clients.size() < currentMaxClients) {
			double currentSize = size*1024.0*1024.0*(1.0 + 1.0*(double)rand()/(double)RAND_MAX);
			int requestSize = 1 + ::round(0.01*currentSize);
			int answerSize = 1 + ::round(currentSize);
			Socket *socket = new Socket(server.socketGroup, name + "(clientSocket)", Socket::TCP);
			socket->connect(addressClient);
			new BenchmarkTcpClient(
				*socket,
				server.eventManager,
				clients,
				stats,
				1,
				requestSize,
				answerSize );
		}

		if (!initialized && Platform::nowUs() >= endInitializationUs) {
			endUs = Platform::nowUs() + durationUs;
			nextReportUs = Platform::nowUs() + reportUs;
			stats.clear();
			initialized = true;
			log->info(name, "do benchmark in %f seconds", 0.000001*(double)durationUs);
		}
	}

	log->info(name, "total connections %d", stats.size());
	double logSummary = 0.0;
	double minSpeed = 0.0;
	double maxSpeed = 0.0;
	int count = 0;
	int errors = 0;
	long long sizeSummary = 0;
	for(BenchmarkTcpClient::StatList::const_iterator i = stats.begin(); i != stats.end(); ++i) {
		if (i->first > 0.0) {
			sizeSummary += i->first;
			double speed = 1000000.0*(double)i->first/(double)i->second;
			if (!count) minSpeed = maxSpeed = speed; else
			if (minSpeed > speed) minSpeed = speed; else
			if (maxSpeed < speed) maxSpeed = speed;
			logSummary += ::log10(speed);
			++count;
		} else {
			++errors;
		}
	}

	if (errors) {
		success = false;
		log->error(name, "connection errors %d", errors);
	}

	if (count == 0) success = false;
	double logAverage = count > 0 ? logSummary/(double)count : 0.0;
	double logSummarySqrDeviation = 0.0;
	for(BenchmarkTcpClient::StatList::const_iterator i = stats.begin(); i != stats.end(); ++i) {
		if (i->first > 0.0) {
			double speed = 1000000.0*(double)i->first/(double)i->second;
			double d = logAverage - ::log10(speed);
			logSummarySqrDeviation += d*d;
		}
	}

	double logAverageSqrDeviation = count > 0 ? logSummarySqrDeviation/(double)count : 0.0;
	log->info(name, "simultaneous connections count %d", maxClients);
	log->info(name, "full %f kB/s, avg %f kB/s, min %f kB/s, max %f kB/s, deviation %f dB (%f KB/s)",
		1000000.0*(double)sizeSummary/(double)durationUs/1024.0,
		1000000.0*(double)sizeSummary/(double)durationUs/1024.0/(double)maxClients,
		minSpeed/1024.0,
		maxSpeed/1024.0,
		::sqrt(logAverageSqrDeviation)*10.0,
		(::pow(10.0, logAverage) - ::pow(10.0, logAverage - ::sqrt(logAverageSqrDeviation)))/1024.0 );

	while(!clients.empty()) delete *clients.begin();
	if (benchmarkTcpServer) delete benchmarkTcpServer;

	server.stepWhile(10000000);
	server.end();
}
