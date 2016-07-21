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

#ifndef _EVENT_H_
#define _EVENT_H_


#include <map>
#include <vector>


class Event {
public:
	enum LinkType {
		LinkReady  = 0,
		LinkRaise  = 1,
		LinkSource = 2
	};
	enum {
		LinkCount  = 3
	};

	class Handler {
	public:
		virtual ~Handler() { }
		virtual void handle(Event &event, long long plannedTimeUs);
	};

	class Source {
	private:
		friend class Event;
		bool ready;
		Event *list;
	public:
		Source();
		~Source();
		void setReady(bool value);
		bool getReady() const { return ready; }
	};

	class Manager {
	private:
		friend class Event;
		Event *listReady;
		std::vector< std::pair<Event*, Event*> > shuffle;
	public:
		Manager();
		~Manager();
		long long doEvents(long long timeUs);
	};

private:
	Handler *handler;
	Manager *manager;

	Event **previousNext[LinkCount];
	Event *next[LinkCount];

	int priority;
	long long timeUs;
	bool ready;

	void link(LinkType type, Event *&previousNext);
	void unlink(LinkType type);
	void unlink();

	void updateReady();

public:
	Event();
	Event(Handler &handler, Manager &manager, int priority = 0);
	Event(Handler &handler, Manager &manager, Source &source, int priority = 0);
	~Event();

	void init(Handler &handler, Manager &manager, int priority = 0);
	void init(Handler &handler, Manager &manager, Source &source, int priority = 0);

	void raise();

	void setReady(bool value);
	bool getReady() const { return ready; }

	void setTime(long long timeUs, bool force = false);
	void setTimeRelativeNow(long long timeUs = 0, bool force = false);
	void setTimeRelativePrev(long long timeUs, bool force = false);
	long long getTime() const { return timeUs; }

	void disable() { setTime(-1, true); }
	bool isEnabled() const { return getTime() >= 0; }

	Handler* getHandler() const { return handler; }
	Manager* getManager() const { return manager; }
	int getPriority() const { return priority; }
};


#endif
