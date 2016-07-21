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

#ifndef _PACKET_H_
#define _PACKET_H_

#include <vector>

class Packet {
public:
	enum Type {
		Hello = 0,		// open connection
		Bye,            // close connection
		Data,           // data
		Confirmation,   // confirmation
		Disconnect,     // close, when tcp-connection closed with error
		ByeBye          // special confirmation packet,
		                //   signal that no more "data" packets will be sent
		                //   no more hello, bye, data or disconnect
	};

	bool sent;
	long long sentTimeUs;
	int measureIndex;
	int remainResendCount;
	bool confirmed;

	std::vector<char> data;

	Packet():
		sent(),
		sentTimeUs(),
		measureIndex(),
		remainResendCount(),
		confirmed()
	{ data.reserve(12); }

	bool isComplete() const
		{ return sent && confirmed; }

	template<typename T>
	const T get(int offset) const {
		return offset >= 0 && offset + (int)sizeof(T) <= (int)data.size()
			 ? *(const T*)&data[offset] : T();
	}

	template<typename T>
	void set(int offset, const T &value) {
		if (offset < 0) return;
		if ((int)data.size() < offset + (int)sizeof(T))
			data.resize(offset + (int)sizeof(T));
		*(T*)&data[offset] = value;
	}

	unsigned int getCrc32() const { return get<unsigned int>(0); }
	void setCrc32(unsigned int value) { set<unsigned int>(0, value); }

	Type getType() const
		{ return (Type)get<unsigned char>(4); }
	void setType(Type value)
		{ set<unsigned char>(4, value); }

	int getIndex() const { return get<unsigned int>(5); }
	void setIndex(int value) { set<unsigned int>(5, value); }

	const void* getData() const { return (int)data.size() > 9 ? &data[9] : NULL; }
	int getSize() const { return (int)data.size() > 9 ? (int)data.size() - 9 : 0; }
	void setData(const void *data, int size);

	void* getRawData() { return data.empty() ? NULL : &data.front(); }
	const void* getRawData() const { return data.empty() ? NULL : &data.front(); }
	int getRawSize() const { return (int)data.size(); }
	void setRawData(const void *data, int size);
	void setRawSize(int size) { setRawData(NULL, size); }

	void encode(Type type, int index, const void *data = NULL, int size = 0);

	static unsigned int crc32(const void *data, int size, unsigned int previousCrc32 = 0);

	void applyCrc32()
		{ setCrc32(crc32(&data[4], data.size() - 4)); }
	bool checkCrc32()
		{ return getCrc32() == crc32(&data[4], data.size() - 4); }

	static bool checkCrc32(const void *data, int size)
		{ return size > 4 && *(unsigned int*)data == crc32((const char*)data + 4, size - 4); }

	static bool packIntPair(int a, int b, void *&data, int &size);
	static bool unpackIntPair(int &a, int &b, const void *&data, int &size);
};

#endif
