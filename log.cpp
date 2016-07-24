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

#include <cstdlib>
#include <cstring>
#include <ctime>

#include "log.h"
#include "platform.h"


Log::Log() {
	memset(streams, 0, sizeof(streams));
}

std::string Log::vstrprintf(const char *format, va_list args) {
	std::string str;

	va_list argsCopy;
	va_copy(argsCopy, args);
	int size = vsnprintf(NULL, 0, format, args);
	if (size > 0) {
		str.resize(size);
		vsnprintf(&str[0], size+1, format, argsCopy);
	}
	va_end(argsCopy);

	return str;
}

std::string Log::strprintf(const char *format, ...) {
	va_list args;
	va_start(args, format);
	std::string str = vstrprintf(format, args);
	va_end(args);
	return str;
}

void Log::logLine(Type type, const std::string &name, const std::string &line) {
	if (!streams[type]) return;
	std::ostream &stream = *streams[type];

	static const char type_name[TypeCount][1024] = {
		"info",
		"warning",
		"error"
	};

	time_t rawtime;
	struct tm *ptm;
	time(&rawtime);
	ptm = gmtime(&rawtime);
	char time_buf[1024] = {};
	strftime(time_buf, sizeof(time_buf), "%Y.%m.%d %H:%M:%S UTC", ptm);

	stream << time_buf
		   << ": "
		   << type_name[type]
		   << " - "
		   << name
		   << " - "
		   << line
		   << std::endl;
}

void Log::log(Type type, const std::string &name, const char *format, ...) {
	va_list args;
	va_start(args, format);
	logLine(type, name, vstrprintf(format, args));
	va_end(args);
}

void Log::info(const std::string &name, const char *format, ...) {
	va_list args;
	va_start(args, format);
	logLine(Info, name, vstrprintf(format, args));
	va_end(args);
}

void Log::warning(const std::string &name, const char *format, ...) {
	va_list args;
	va_start(args, format);
	logLine(Warning, name, vstrprintf(format, args));
	va_end(args);
}

void Log::error(const std::string &name, const char *format, ...) {
	va_list args;
	va_start(args, format);
	logLine(Error, name, vstrprintf(format, args));
	va_end(args);
}

void Log::errorno(const std::string &name, const std::string &message, int error) {
	log(Error, name, "%s - %s (%d)", message.c_str(), Platform::lastErrorString(error).c_str(), error);
}

void Log::errorno(const std::string &name, const std::string &message) {
	Log::errorno(name, message, Platform::lastError());
}

void Log::errorno(const std::string &name, int error) {
	log(Error, name, "%s (%d)", strerror(error), error);
}

void Log::errorno(const std::string &name) {
	Log::errorno(name, Platform::lastError());
}
