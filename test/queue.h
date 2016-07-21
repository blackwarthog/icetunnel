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

#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <cstring>

#include <vector>


class Queue {
private:
	std::vector<char> buffer;
	int beginIndex;
	int endIndex;

public:
	explicit Queue(int reserve = 0):
		buffer(reserve > 1024 ? reserve : 1024), beginIndex(), endIndex() { }

	void reserve(int size)
		{ if (endIndex + size > (int)buffer.size()) buffer.resize(endIndex + size); }
	int push(int size)
		{ if (size > 0) { reserve(size); endIndex += size; return size; } return 0; }
	int push(const void *data, int size)
		{ if (size > 0) { reserve(size); memcpy(end(), data, size); return push(size); } return 0; }

	int pop(void *data, int size) {
		if (size <= 0) return 0;
		if (size > this->size()) size = this->size();
		if (size > 0) {
			memcpy(data, begin(), size);
			return pop(size);
		}
		return 0;
	}

	int pop(int size) {
		if (size <= 0) return 0;
		if (size > this->size()) size = this->size();
		beginIndex += size;
		if (empty()) beginIndex = endIndex = 0;
		return size;
	}

	void* begin() { return &buffer[beginIndex]; }
	void* end() { return &buffer[endIndex]; }

	const void* begin() const { return &buffer[beginIndex]; }
	const void* end() const { return &buffer[endIndex]; }

	int size() const { return endIndex - beginIndex; }
	bool empty() const { return size() <= 0; }
	void clear() { pop(size()); }
};

#endif
