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
#include <cstdlib>

#include <string>
#include <sstream>
#include <vector>

#include "server.h"
#include "socket.h"
#include "platform.h"


namespace params {
	struct Param {
		std::string name;
		int args_count;
		std::string args;
		std::string description;
		bool (*func)(Server&, char**);

		Param(
			const char *name,
			int args_count,
			const char *args,
			const char *description,
			bool (*func)(Server&, char**)
		):
			name(name),
			args_count(args_count),
			args(args),
			description(description),
			func(func)
		{
			for(int i = 0; i < (int)this->name.size(); ++i)
				if (this->name[i] == '_') this->name[i] = '-';
		}
	};

	bool daemon(Server &server, char **) {
		memset(server.log.streams, 0, sizeof(server.log.streams));
		return true;
	}

	bool test(Server &server, char **) {
		server.test = true;
		return true;
	}

	bool test_tcp_remote_address(Server &server, char **args) {
		return server.testTcpRemoteAddress.fromString(args[1]);
	}

	bool test_udp_remote_address(Server &server, char **args) {
		return server.testUdpRemoteAddress.fromString(args[1]);
	}

	bool tcp_backlog(Server &server, char **args) {
		server.tcpBacklog = atoi(args[1]);
		return true;
	}

	bool tcp_receive_address_size(Server &server, char **args) {
		server.tcpReceiveAddressSize = atoi(args[1]);
		return true;
	}

	bool udp_receive_packet_size(Server &server, char **args) {
		server.udpReceivePacketSize = atoi(args[1]);
		return true;
	}

	bool udp_receive_address_size(Server &server, char **args) {
		server.udpReceiveAddressSize = atoi(args[1]);
		return true;
	}

	bool udp_send_packet_size(Server &server, char **args) {
		server.udpSendPacketSize = atoi(args[1]);
		return true;
	}

	bool udp_max_sent_buffer_size(Server &server, char **args) {
		server.udpMaxSentBufferSize = atoi(args[1]);
		return true;
	}

	bool udp_max_receive_buffer_size(Server &server, char **args) {
		server.udpMaxReceiveBufferSize = atoi(args[1]);
		return true;
	}

	bool udp_max_sent_measure_size(Server &server, char **args) {
		server.udpMaxSentMeasureSize = atoi(args[1]);
		return true;
	}

	bool udp_resend_count(Server &server, char **args) {
		server.udpResendCount = atoi(args[1]);
		return true;
	}

	bool confirmation_resend_count(Server &server, char **args) {
		server.confirmationResendCount = atoi(args[1]);
		return true;
	}

	bool udp_max_send_loss_percent(Server &server, char **args) {
		server.udpMaxSendLossPercent = atof(args[1]);
		return true;
	}

	bool udp_initial_send_interval_us(Server &server, char **args) {
		server.udpInitialSendIntervalUs = atoll(args[1]);
		return true;
	}

	bool udp_resend_us(Server &server, char **args) {
		server.udpResendUs = atoll(args[1]);
		return true;
	}

	bool build_confirmations_us(Server &server, char **args) {
		server.buildConfirmationsUs = atoll(args[1]);
		return true;
	}

	bool build_udp_packets_us(Server &server, char **args) {
		server.buildUdpPacketsUs = atoll(args[1]);
		return true;
	}

	bool udp_max_sent_measure_us(Server &server, char **args) {
		server.udpMaxSentMeasureUs = atoll(args[1]);
		return true;
	}

	bool udp_listener(Server &server, char **args) {
		Address udpAddress;
		Address tcpAddress;
		if (!udpAddress.fromString(args[1])) return false;
		if (!tcpAddress.fromString(args[2])) return false;
		return NULL != server.createUdpListener(udpAddress, tcpAddress);
	}

	bool tcp_listener(Server &server, char **args) {
		Address tcpAddress;
		Address udpAddress;
		if (!tcpAddress.fromString(args[1])) return false;
		if (!udpAddress.fromString(args[2])) return false;
		return NULL != server.createTcpListener(tcpAddress, udpAddress);
	}

	bool test_listener(Server &server, char **args) {
		Address tcpAddress;
		if (!tcpAddress.fromString(args[1])) return false;
		return NULL != server.createTestListener(tcpAddress);
	}

	bool help(Server &, char **) {
		return true;
	}



#define PARAM0(name, description) Param(#name, 0, "", description, name)
#define PARAM1(name, arg, description) Param(#name, 1, arg, description, name)
#define PARAM2(name, arg1, arg2, description) Param(#name, 2, arg1 " " arg2, description, name)

	Param params[] = {
		PARAM0(daemon, "disable logging"),
		PARAM0(test, "run tests"),
		PARAM1(test_tcp_remote_address, "<address>", "address of remote test-listener"),
		PARAM1(test_udp_remote_address, "<address>", "address of remote udp-tunnel to test-listener"),
		PARAM1(tcp_backlog, "<value>", "backlog parameter for listen() function"),
		PARAM1(tcp_receive_address_size, "<value>", "maximum size of tcp-address data"),
		PARAM1(udp_receive_packet_size, "<value>", "size of buffer to receive single udp-packet"),
		PARAM1(udp_receive_address_size, "<value>", "maximum size of udp-address data"),
		PARAM1(udp_send_packet_size, "<value>", "size of sent udp-packets"),
		PARAM1(udp_max_sent_buffer_size, "<value>", "size of send buffer per connection"),
		PARAM1(udp_max_receive_buffer_size, "<value>", "size of receive buffer connection"),
		PARAM1(udp_max_sent_measure_size, "<value>", "amount of transfered data to do single speed measure"),
		PARAM1(udp_resend_count, "<value>", "count of tries to send udp-packet before disconnect"),
		PARAM1(confirmation_resend_count, "<value>", "count of confirmations for each received udp-packet"),
		PARAM1(udp_max_send_loss_percent, "<value>", "percent of loss packets, creater value produce more extra traffic, but when value less then real loss percend of the line, speed falls down to zero"),
		PARAM1(udp_initial_send_interval_us, "<value>", "initial interval in microseconds of send udp-packets, determines initial speed of connection, uses when no one measure complete yet"),
		PARAM1(udp_resend_us, "<value>", "time in microseconds of awaiting confirmation before resending udp-packet"),
		PARAM1(build_confirmations_us, "<value>", "interval in microseconds of send confirmations"),
		PARAM1(build_udp_packets_us, "<value>", "time in microseconds of awaiting data from tcp before send non-full udp-packet"),
		PARAM1(udp_max_sent_measure_us, "<value>", "time in microseconds to do single speed measure"),
		PARAM2(udp_listener, "<from>", "<to>", "server-side of tunnel forward all incoming udp-connections to specified tcp-address"),
		PARAM2(tcp_listener, "<from>", "<to>", "client-side of tunnel forward all incoming tcp-connections to specified address of udp-listener"),
		PARAM1(test_listener, "<address>", "simple server uses to do some tests, see: --test-tcp-remote-address, --test-tcp-remote-address"),
		PARAM0(help, "show help"),
	};
}

bool init(Server &server, int argc, char **argv) {
	std::ostringstream out;

	bool help = argc <= 1;
	std::vector<bool> processed(argc, false);

	int params_count = (int)(sizeof(params::params)/sizeof(params::params[0]));
	for(int p = 0; p < params_count; ++p) {
		for(int i = 1; i < argc; ++i) {
			std::string arg = argv[i];
			for(int pp = 0; pp < params_count; ++pp) {
				params::Param &param = params::params[pp];
				if ("--" + param.name == arg) {
					if (p == pp) {
						if (!param.func(server, argv + i)) {
							out << "cannot set param --" << param.name;
							for(int j = 0; j < param.args_count; ++j)
								out << " " << argv[i+1+j];
							out << std::endl;
						}
						if (param.name == "help")
							help = true;
						for(int j = 0; j <= param.args_count; ++j)
							processed[i+j] = true;
					}
					i += param.args_count;
					break;
				}
			}
		}
	}

	for(int i = 1; i < argc; ++i)
		if (!processed[i])
			help = true;

	if (help) {
		out << std::endl;
		out << "IceTunnel allow to pass TCP-traffic via UDP and increase speed on channels with big percent of packet loss" << std::endl;
		out << std::endl;

		out << "Usage: " << std::endl;
		out << "    icetunnel" << std::endl;
		for(int i = 0; i < (int)(sizeof(params::params)/sizeof(params::params[0])); ++i) {
			params::Param &param = params::params[i];
			out << "        --" << param.name;
			if (!param.args.empty()) out << " " << param.args;
			out << std::endl;
		}
		out << std::endl;

		out << "Description:" << std::endl;
		out << std::endl;
		for(int i = 0; i < (int)(sizeof(params::params)/sizeof(params::params[0])); ++i) {
			params::Param &param = params::params[i];
			out << "  --" << param.name;
			if (!param.args.empty()) out << " " << param.args;
			out << std::endl;
			out << "    " << param.description;
			out << std::endl;
			out << std::endl;
		}
		out << std::endl;

		out << "Example server (listen UDP-port 1234 and forward it into SOCKS proxy runned at IP 1.2.3.4):" << std::endl;
		out << "    icetunnel --udp-listener 1234 1.2.3.4:1080" << std::endl;
		out << std::endl;
		out << "Example client (listen TCP-port 1080 and forward int into UDP 4.3.2.1:1234 and next to SOCKS proxy runned at IP 1.2.3.4):" << std::endl;
		out << "    icetunnel --tcp-listener 1080 4.3.2.1:1234" << std::endl;
		out << std::endl;

		for(int i = 1; i < argc; ++i)
			if (!processed[i])
				out << "unknown param: " << argv[i] << std::endl;
		if (help) out << std::endl;
	}

	out.flush();
	if (server.log.streams[Log::Info])
		(*server.log.streams[Log::Info]) << out.str();

	return server.test || !server.tcpListeners.empty() || !server.udpListeners.empty() || !server.testListeners.empty();
}

int main(int argc, char **argv) {
	Socket::initialize();
	int r = Platform::main(argc, argv);
	Socket::deinitialize();
	return r;
}
