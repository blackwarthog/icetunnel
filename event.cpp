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

#include <cassert>
#include <cstdlib>
#include <cstring>

#include <iostream>

#include "event.h"
#include "platform.h"


void Event::Handler::handle(Event&, long long) { }


Event::Source::Source():
	ready(), list()
{ }

Event::Source::~Source() {
	setReady(false);
	while(list) list->unlink(LinkSource);
}

void Event::Source::setReady(bool value) {
	if (ready == value) return;
	ready = value;
	for(Event *event = list; event; event = event->next[LinkSource])
		event->setReady(ready);
}


Event::Manager::Manager():
	listReady() { }

Event::Manager::~Manager() {
	while(listReady) listReady->unlink();
}

long long Event::Manager::doEvents(long long timeUs) {
	// get ready events
	int count = 0;
	int priority = 0;
	Event *events = NULL;
	for(Event *event = listReady; event; event = event->next[LinkReady]) {
		if (event->getTime() <= timeUs) {
			if (!events) priority = event->getPriority(); else
				if (priority < event->getPriority()) priority = event->getPriority();
			if (event->getPriority() == priority) {
				event->link(LinkRaise, events);
				++count;
			}
		}
	}

	// skip non-priority
	for(Event *event = events; event;)
		if (event->getPriority() == priority) event = event->next[LinkRaise]; else {
			Event *e = event;
			event = event->next[LinkRaise];
			e->unlink(LinkRaise);
			--count;
		}

	// shuffle event before raise
	shuffle.clear();
	shuffle.resize(count);
	for(Event *event = events; event;) {
		Event *next = event->next[LinkRaise];
		std::pair<Event*, Event*> &item = shuffle[rand()%shuffle.size()];
		if (item.first == NULL) {
			event->link(LinkRaise, item.first);
			item.second = event;
		} else
		if (rand()%2) {
			event->link(LinkRaise, item.first);
		} else {
			event->link(LinkRaise, item.second->next[LinkRaise]);
			item.second = event;
		}
		event = next;
	}

	assert(events == NULL);
	while(events) events->unlink(LinkRaise);

	Event **lastNext = &events;
	for(std::vector< std::pair<Event*, Event*> >::iterator i = shuffle.begin(); i != shuffle.end(); ++i) {
		if (i->first) {
			*lastNext = i->first;
			i->first->previousNext[LinkRaise] = lastNext;
			lastNext = &i->second->next[LinkRaise];
		}
	}

	// raise
	while(events) {
		assert(events->getReady() && events->isEnabled());
		events->raise();
	}

	// find time of next event
	if (listReady) {
		long long nextTime = listReady->getTime();
		for(Event *event = listReady; event; event = event->next[LinkReady])
			if (event->getTime() < nextTime)
				nextTime = event->getTime();
		return nextTime;
	}
	return -1;
}


Event::Event():
	handler(),
	manager(),
	priority(),
	timeUs(-1),
	ready()
{
	memset(previousNext, 0, sizeof(previousNext));
	memset(next, 0, sizeof(next));
}

Event::Event(Handler &handler, Manager &manager, int priority):
	handler(),
	manager(),
	timeUs(-1),
	ready()
{
	memset(previousNext, 0, sizeof(previousNext));
	memset(next, 0, sizeof(next));
	init(handler, manager, priority);
}

Event::Event(Handler &handler, Manager &manager, Source &source, int priority):
	handler(),
	manager(),
	priority(),
	timeUs(-1),
	ready()
{
	memset(previousNext, 0, sizeof(previousNext));
	memset(next, 0, sizeof(next));
	init(handler, manager, source, priority);
}

Event::~Event() {
	unlink();
}

void Event::link(LinkType type, Event *&previousNext) {
	if (this->previousNext[type]) unlink(type);
	this->previousNext[type] = &previousNext;
	next[type] = previousNext;
	if (next[type]) next[type]->previousNext[type] = &next[type];
	previousNext = this;
}

void Event::unlink(LinkType type) {
	if (previousNext[type]) {
		*previousNext[type] = next[type];
		if (next[type]) next[type]->previousNext[type] = previousNext[type];
		previousNext[type] = NULL;
		next[type] = NULL;
	}
}

void Event::unlink() {
	for(int i = 0; i < LinkCount; ++i)
		unlink((LinkType)i);
	timeUs = -1;
	ready = false;
	handler = NULL;
	manager = NULL;
}

void Event::init(Handler &handler, Manager &manager, int priority) {
	unlink();
	this->handler = &handler;
	this->manager = &manager;
	this->priority = priority;
	setReady(true);
}

void Event::init(Handler &handler, Manager &manager, Source &source, int priority) {
	unlink();
	this->handler = &handler;
	this->manager = &manager;
	this->priority = priority;
	link(LinkSource, source.list);
	setReady(source.getReady());
}

void Event::raise() {
	long long plannedTimeUs = timeUs;
	disable();
	if (handler) handler->handle(*this, plannedTimeUs);
}

void Event::updateReady() {
	if (getReady() && isEnabled()) {
		if (!previousNext[LinkReady] && manager) link(LinkReady, manager->listReady);
	} else {
		unlink(LinkReady);
		unlink(LinkRaise);
	}
}

void Event::setReady(bool value) {
	if (ready != value) {
		ready = value;
		updateReady();
	}
}

void Event::setTime(long long timeUs, bool force) {
	if (this->timeUs == timeUs) return;
	if ( force
	  || (this->timeUs > timeUs && timeUs >= 0)
	  || (this->timeUs < 0) )
	{
		this->timeUs = timeUs;
		updateReady();
	}
}

void Event::setTimeRelativeNow(long long timeUs, bool force)
	{ setTime(Platform::nowUs() + timeUs, force); }

void Event::setTimeRelativePrev(long long timeUs, bool force) {
	if (isEnabled())
		setTime(this->timeUs + timeUs, force);
	else
		setTimeRelativeNow(0, force);
}
