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

#ifndef _LOG_H_
#define _LOG_H_

#include <cstdarg>

#include <string>
#include <iostream>

class Log {
public:
	enum Type {
		Info      = 0,
		Warning   = 1,
		Error     = 2
	};
	enum {
		TypeCount = 3
	};

	std::ostream *streams[TypeCount];

	Log();

	static std::string vstrprintf(const char *format, va_list arg);
	static std::string strprintf(const char *format, ...);

	void logLine(Type, const std::string &name, const std::string &line);
	void log(Type type, const std::string &name, const char *format, ...);
	void info(const std::string &name, const char *format, ...);
	void warning(const std::string &name, const char *format, ...);
	void error(const std::string &name, const char *format, ...);
	void errorno(const std::string &name, const std::string &message, int error);
	void errorno(const std::string &name, const std::string &message);
	void errorno(const std::string &name, int error);
	void errorno(const std::string &name);
};


#endif
