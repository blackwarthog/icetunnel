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

#ifndef _ADDRESS_H_
#define _ADDRESS_H_

#include <string>
#include <vector>

struct Address {
	std::vector<char> data;

	Address() { }
	explicit Address(const std::string &address) { fromString(address); }

	bool operator< (const Address &other) const;
	bool operator== (const Address &other) const;
	std::string toString() const;
	bool fromString(const std::string &address);
};

#endif
