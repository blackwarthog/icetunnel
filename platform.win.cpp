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

#include <windows.h>

#include "platform.h"
#include "main.h"
#include "server.h"


class Platform::Internal {
public:
	class Service {
	public:
		static Server *server;
		static SERVICE_STATUS status;
		static SERVICE_STATUS_HANDLE statusHandle;

		static void controlHandler(DWORD request) {
			switch(request) {
			case SERVICE_CONTROL_STOP:
			case SERVICE_CONTROL_SHUTDOWN:
				status.dwWin32ExitCode = 0;
				status.dwCurrentState = SERVICE_STOPPED;
				break;
			default:
				break;
			}
			SetServiceStatus(statusHandle, &status);
		}

		static void main(int argc, char** argv) {
			memset(&status, 0, sizeof(status));
			status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
			status.dwCurrentState = SERVICE_RUNNING;
			status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;

			statusHandle = RegisterServiceCtrlHandler("", (LPHANDLER_FUNCTION)controlHandler);
			if (!statusHandle) return;

			SetServiceStatus(statusHandle, &status);

			//memset(server->log.streams, 0, sizeof(server->log.streams));

			status.dwCurrentState = SERVICE_RUNNING;
			SetServiceStatus(statusHandle, &status);
			server->begin();
			while(status.dwCurrentState == SERVICE_RUNNING && server->step()) { }
			server->end();
		}
	};
};


Server *Platform::Internal::Service::server;
SERVICE_STATUS Platform::Internal::Service::status;
SERVICE_STATUS_HANDLE Platform::Internal::Service::statusHandle;


long long Platform::nowUs() {
	FILETIME ft;
	memset(&ft, 0, sizeof(ft));
	GetSystemTimeAsFileTime(&ft);
	long long t = ((long long)ft.dwHighDateTime << 32) | (long long)ft.dwLowDateTime;
	t /= 10;
    t -= 11644473600000000ll;
    return t;
}

int Platform::lastError() {
    return GetLastError();
}

std::string Platform::lastErrorString(int err) {
    char *buffer = NULL;
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        err,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (char*)&buffer,
        0, NULL );
    std::string s(buffer ? buffer : "");
    LocalFree(buffer);
    return s;
}

int Platform::main(int argc, char **argv) {
	Server server("main");
	if (init(server, argc, argv)) {
		Internal::Service::server = &server;

		SERVICE_TABLE_ENTRY serviceTable[1];
		serviceTable[0].lpServiceName = (char*)"";
		serviceTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTION)Internal::Service::main;
		if ( StartServiceCtrlDispatcher(serviceTable) == 0
		  && GetLastError() == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)
			server.run();
	}
	return 0;
}
