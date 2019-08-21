/*
 * barrier -- mouse and keyboard sharing utility
 * Copyright (C) 2012-2016 Symless Ltd.
 * Copyright (C) 2004 Chris Schoeneman
 * 
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 * 
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "base/EventQueue.h"

#include "mt/Mutex.h"
#include "mt/Lock.h"
#include "arch/Arch.h"
#include "base/SimpleEventQueueBuffer.h"
#include "base/Stopwatch.h"
#include "base/IEventJob.h"
#include "base/EventTypes.h"
#include "base/Log.h"
#include "base/XBase.h"
#include "../gui/src/ShutdownCh.h"

EVENT_TYPE_ACCESSOR(Client)
EVENT_TYPE_ACCESSOR(IStream)
EVENT_TYPE_ACCESSOR(IpcClient)
EVENT_TYPE_ACCESSOR(IpcClientProxy)
EVENT_TYPE_ACCESSOR(IpcServer)
EVENT_TYPE_ACCESSOR(IpcServerProxy)
EVENT_TYPE_ACCESSOR(IDataSocket)
EVENT_TYPE_ACCESSOR(IListenSocket)
EVENT_TYPE_ACCESSOR(ISocket)
EVENT_TYPE_ACCESSOR(OSXScreen)
EVENT_TYPE_ACCESSOR(ClientListener)
EVENT_TYPE_ACCESSOR(ClientProxy)
EVENT_TYPE_ACCESSOR(ClientProxyUnknown)
EVENT_TYPE_ACCESSOR(Server)
EVENT_TYPE_ACCESSOR(ServerApp)
EVENT_TYPE_ACCESSOR(IKeyState)
EVENT_TYPE_ACCESSOR(IPrimaryScreen)
EVENT_TYPE_ACCESSOR(IScreen)
EVENT_TYPE_ACCESSOR(Clipboard)
EVENT_TYPE_ACCESSOR(File)

// interrupt handler.  this just adds a quit event to the queue.
static
void
interrupt(Arch::ESignal, void* data)
{
    EventQueue* events = static_cast<EventQueue*>(data);
    events->addEvent(Event(Event::kQuit));
}


//
// EventQueue
//

EventQueue::EventQueue() :
    m_systemTarget(0),
    m_nextType(Event::kLast),
    m_typesForClient(NULL),
    m_typesForIStream(NULL),
    m_typesForIpcClient(NULL),
    m_typesForIpcClientProxy(NULL),
    m_typesForIpcServer(NULL),
    m_typesForIpcServerProxy(NULL),
    m_typesForIDataSocket(NULL),
    m_typesForIListenSocket(NULL),
    m_typesForISocket(NULL),
    m_typesForOSXScreen(NULL),
    m_typesForClientListener(NULL),
    m_typesForClientProxy(NULL),
    m_typesForClientProxyUnknown(NULL),
    m_typesForServer(NULL),
    m_typesForServerApp(NULL),
    m_typesForIKeyState(NULL),
    m_typesForIPrimaryScreen(NULL),
    m_typesForIScreen(NULL),
    m_typesForClipboard(NULL),
    m_typesForFile(NULL),
    m_readyMutex(new Mutex),
    m_readyCondVar(new CondVar<bool>(m_readyMutex, false))
{
    m_mutex = ARCH->newMutex();
    ARCH->setSignalHandler(Arch::kINTERRUPT, &interrupt, this);
    ARCH->setSignalHandler(Arch::kTERMINATE, &interrupt, this);
    m_buffer = new SimpleEventQueueBuffer;
}

EventQueue::~EventQueue()
{
    delete m_buffer;
    delete m_readyCondVar;
    delete m_readyMutex;
    
    ARCH->setSignalHandler(Arch::kINTERRUPT, NULL, NULL);
    ARCH->setSignalHandler(Arch::kTERMINATE, NULL, NULL);
    ARCH->closeMutex(m_mutex);
}

void
EventQueue::loop()
{
    m_buffer->init();
    {
        Lock lock(m_readyMutex);
        *m_readyCondVar = true;
        m_readyCondVar->signal();
    }
    LOG((CLOG_DEBUG "event queue is ready"));
    while (!m_pending.empty()) {
        LOG((CLOG_DEBUG "add pending events to buffer"));
        Event& event = m_pending.front();
        addEventToBuffer(event);
        m_pending.pop();
    }
    
    Event event;
    getEvent(event);
    while (event.getType() != Event::kQuit) {
        dispatchEvent(event);
        Event::deleteData(event);
        getEvent(event);
    }
}

Event::Type
EventQueue::registerTypeOnce(Event::Type& type, const char* name)
{
    ArchMutexLock lock(m_mutex);
    if (type == Event::kUnknown) {
        m_typeMap.insert(std::make_pair(m_nextType, name));
        m_nameMap.insert(std::make_pair(name, m_nextType));
        LOG((CLOG_DEBUG1 "registered event type %s as %d", name, m_nextType));
        type = m_nextType++;
    }
    return type;
}

const char*
EventQueue::getTypeName(Event::Type type)
{
    switch (type) {
    case Event::kUnknown:
        return "nil";

    case Event::kQuit:
        return "quit";

    case Event::kSystem:
        return "system";

    case Event::kTimer:
        return "timer";

    default:
        TypeMap::const_iterator i = m_typeMap.find(type);
        if (i == m_typeMap.end()) {
            return "<unknown>";
        }
        else {
            return i->second;
        }
    }
}

void
EventQueue::adoptBuffer(IEventQueueBuffer* buffer)
{
    ArchMutexLock lock(m_mutex);

    LOG((CLOG_DEBUG "adopting new buffer"));

    if (m_events.size() != 0) {
        // this can come as a nasty surprise to programmers expecting
        // their events to be raised, only to have them deleted.
        LOG((CLOG_DEBUG "discarding %d event(s)", m_events.size()));
    }

    // discard old buffer and old events
    delete m_buffer;
    for (EventTable::iterator i = m_events.begin(); i != m_events.end(); ++i) {
        Event::deleteData(i->second);
    }
    m_events.clear();
    m_oldEventIDs.clear();

    // use new buffer
    m_buffer = buffer;
    if (m_buffer == NULL) {
        m_buffer = new SimpleEventQueueBuffer;
    }
}

bool
EventQueue::getEvent(Event& event, double timeout)
{
    Stopwatch timer(true);
retry:
    // if no events are waiting then handle timers and then wait
    while (m_buffer->isEmpty()) {
        // handle timers first
        if (hasTimerExpired(event)) {
            return true;
        }

        // get time remaining in timeout
        double timeLeft = timeout - timer.getTime();
        if (timeout >= 0.0 && timeLeft <= 0.0) {
            return false;
        }

        // get time until next timer expires.  if there is a timer
        // and it'll expire before the client's timeout then use
        // that duration for our timeout instead.
        double timerTimeout = getNextTimerTimeout();
        if (timeout < 0.0 || (timerTimeout >= 0.0 && timerTimeout < timeLeft)) {
            timeLeft = timerTimeout;
        }

        // wait for an event
        m_buffer->waitForEvent(timeLeft);
    }

    // get the event
    UInt32 dataID;
    IEventQueueBuffer::Type type = m_buffer->getEvent(event, dataID);
    switch (type) {
    case IEventQueueBuffer::kNone:
        if (timeout < 0.0 || timeout <= timer.getTime()) {
            // don't want to fail if client isn't expecting that
            // so if getEvent() fails with an infinite timeout
            // then just try getting another event.
            goto retry;
        }
        return false;

    case IEventQueueBuffer::kSystem:
        return true;

    case IEventQueueBuffer::kUser:
        {
            ArchMutexLock lock(m_mutex);
            event = removeEvent(dataID);
            return true;
        }

    default:
        assert(0 && "invalid event type");
        return false;
    }
}

bool
EventQueue::dispatchEvent(const Event& event)
{
    void* target   = event.getTarget();
    IEventJob* job = getHandler(event.getType(), target);
    if (job == NULL) {
        job = getHandler(Event::kUnknown, target);
    }
    if (job != NULL) {
        job->run(event);
        return true;
    }
    return false;
}

void
EventQueue::addEvent(const Event& event)
{
    // discard bogus event types
    switch (event.getType()) {
    case Event::kUnknown:
    case Event::kSystem:
    case Event::kTimer:
        return;

    default:
        break;
    }
    
    if ((event.getFlags() & Event::kDeliverImmediately) != 0) {
        dispatchEvent(event);
        Event::deleteData(event);
    }
    else if (!(*m_readyCondVar)) {
        m_pending.push(event);
    }
    else {
        addEventToBuffer(event);
    }
}

void
EventQueue::addEventToBuffer(const Event& event)
{
    ArchMutexLock lock(m_mutex);
    
    // store the event's data locally
    UInt32 eventID = saveEvent(event);
    
    // add it
    if (!m_buffer->addEvent(eventID)) {
        // failed to send event
        removeEvent(eventID);
        Event::deleteData(event);
    }
}

EventQueueTimer*
EventQueue::newTimer(double duration, void* target)
{
    assert(duration > 0.0);

    EventQueueTimer* timer = m_buffer->newTimer(duration, false);
    if (target == NULL) {
        target = timer;
    }
    ArchMutexLock lock(m_mutex);
    m_timers.insert(timer);
    // initial duration is requested duration plus whatever's on
    // the clock currently because the latter will be subtracted
    // the next time we check for timers.
    m_timerQueue.push(Timer(timer, duration,
                            duration + m_time.getTime(), target, false));
    return timer;
}

EventQueueTimer*
EventQueue::newOneShotTimer(double duration, void* target)
{
    assert(duration > 0.0);

    EventQueueTimer* timer = m_buffer->newTimer(duration, true);
    if (target == NULL) {
        target = timer;
    }
    ArchMutexLock lock(m_mutex);
    m_timers.insert(timer);
    // initial duration is requested duration plus whatever's on
    // the clock currently because the latter will be subtracted
    // the next time we check for timers.
    m_timerQueue.push(Timer(timer, duration,
                            duration + m_time.getTime(), target, true));
    return timer;
}

void
EventQueue::deleteTimer(EventQueueTimer* timer)
{
    ArchMutexLock lock(m_mutex);
    for (TimerQueue::iterator index = m_timerQueue.begin();
                            index != m_timerQueue.end(); ++index) {
        if (index->getTimer() == timer) {
            m_timerQueue.erase(index);
            break;
        }
    }
    Timers::iterator index = m_timers.find(timer);
    if (index != m_timers.end()) {
        m_timers.erase(index);
    }
    m_buffer->deleteTimer(timer);
}

void
EventQueue::adoptHandler(Event::Type type, void* target, IEventJob* handler)
{
    ArchMutexLock lock(m_mutex);
    IEventJob*& job = m_handlers[target][type];
    delete job;
    job = handler;
}

void
EventQueue::removeHandler(Event::Type type, void* target)
{
    IEventJob* handler = NULL;
    {
        ArchMutexLock lock(m_mutex);
        HandlerTable::iterator index = m_handlers.find(target);
        if (index != m_handlers.end()) {
            TypeHandlerTable& typeHandlers = index->second;
            TypeHandlerTable::iterator index2 = typeHandlers.find(type);
            if (index2 != typeHandlers.end()) {
                handler = index2->second;
                typeHandlers.erase(index2);
            }
        }
    }
    delete handler;
}

void
EventQueue::removeHandlers(void* target)
{
    std::vector<IEventJob*> handlers;
    {
        ArchMutexLock lock(m_mutex);
        HandlerTable::iterator index = m_handlers.find(target);
        if (index != m_handlers.end()) {
            // copy to handlers array and clear table for target
            TypeHandlerTable& typeHandlers = index->second;
            for (TypeHandlerTable::iterator index2 = typeHandlers.begin();
                            index2 != typeHandlers.end(); ++index2) {
                handlers.push_back(index2->second);
            }
            typeHandlers.clear();
        }
    }

    // delete handlers
    for (std::vector<IEventJob*>::iterator index = handlers.begin();
                            index != handlers.end(); ++index) {
        delete *index;
    }
}

bool
EventQueue::isEmpty() const
{
    return (m_buffer->isEmpty() && getNextTimerTimeout() != 0.0);
}

IEventJob*
EventQueue::getHandler(Event::Type type, void* target) const
{
    ArchMutexLock lock(m_mutex);
    HandlerTable::const_iterator index = m_handlers.find(target);
    if (index != m_handlers.end()) {
        const TypeHandlerTable& typeHandlers = index->second;
        TypeHandlerTable::const_iterator index2 = typeHandlers.find(type);
        if (index2 != typeHandlers.end()) {
            return index2->second;
        }
    }
    return NULL;
}

UInt32
EventQueue::saveEvent(const Event& event)
{
    // choose id
    UInt32 id;
    if (!m_oldEventIDs.empty()) {
        // reuse an id
        id = m_oldEventIDs.back();
        m_oldEventIDs.pop_back();
    }
    else {
        // make a new id
        id = static_cast<UInt32>(m_events.size());
    }

    // save data
    m_events[id] = event;
    return id;
}

Event
EventQueue::removeEvent(UInt32 eventID)
{
    // look up id
    EventTable::iterator index = m_events.find(eventID);
    if (index == m_events.end()) {
        return Event();
    }

    // get data
    Event event = index->second;
    m_events.erase(index);

    // save old id for reuse
    m_oldEventIDs.push_back(eventID);

    return event;
}

bool
EventQueue::hasTimerExpired(Event& event)
{
    // return true if there's a timer in the timer priority queue that
    // has expired.  if returning true then fill in event appropriately
    // and reset and reinsert the timer.
    if (m_timerQueue.empty()) {
        return false;
    }

    // get time elapsed since last check
    const double time = m_time.getTime();
    m_time.reset();

    // countdown elapsed time
    for (TimerQueue::iterator index = m_timerQueue.begin();
                            index != m_timerQueue.end(); ++index) {
        (*index) -= time;
    }

    // done if no timers are expired
    if (m_timerQueue.top() > 0.0) {
        return false;
    }

    // remove timer from queue
    Timer timer = m_timerQueue.top();
    m_timerQueue.pop();

    // prepare event and reset the timer's clock
    timer.fillEvent(m_timerEvent);
    event = Event(Event::kTimer, timer.getTarget(), &m_timerEvent);
    timer.reset();

    // reinsert timer into queue if it's not a one-shot
    if (!timer.isOneShot()) {
        m_timerQueue.push(timer);
    }

    return true;
}

double
EventQueue::getNextTimerTimeout() const
{
    // return -1 if no timers, 0 if the top timer has expired, otherwise
    // the time until the top timer in the timer priority queue will
    // expire.
    if (m_timerQueue.empty()) {
        return -1.0;
    }
    if (m_timerQueue.top() <= 0.0) {
        return 0.0;
    }
    return m_timerQueue.top();
}

Event::Type
EventQueue::getRegisteredType(const String& name) const
{
    NameMap::const_iterator found = m_nameMap.find(name);
    if (found != m_nameMap.end())
        return found->second;

    return Event::kUnknown;
}

void*
EventQueue::getSystemTarget()
{
    // any unique arbitrary pointer will do
    return &m_systemTarget;
}

void
EventQueue::waitForReady() const
{
    double timeout = ARCH->time() + 10;
    Lock lock(m_readyMutex);
    
    while (!m_readyCondVar->wait()) {
        if (ARCH->time() > timeout) {
            throw std::runtime_error("event queue is not ready within 5 sec");
        }
    }
}

//
// EventQueue::Timer
//

EventQueue::Timer::Timer(EventQueueTimer* timer, double timeout,
                double initialTime, void* target, bool oneShot) :
    m_timer(timer),
    m_timeout(timeout),
    m_target(target),
    m_oneShot(oneShot),
    m_time(initialTime)
{
    assert(m_timeout > 0.0);
}

EventQueue::Timer::~Timer()
{
    // do nothing
}

void
EventQueue::Timer::reset()
{
    m_time = m_timeout;
}

EventQueue::Timer&
EventQueue::Timer::operator-=(double dt)
{
    m_time -= dt;
    return *this;
}

EventQueue::Timer::operator double() const
{
    return m_time;
}

bool
EventQueue::Timer::isOneShot() const
{
    return m_oneShot;
}

EventQueueTimer*
EventQueue::Timer::getTimer() const
{
    return m_timer;
}

void*
EventQueue::Timer::getTarget() const
{
    return m_target;
}

void
EventQueue::Timer::fillEvent(TimerEvent& event) const
{
    event.m_timer = m_timer;
    event.m_count = 0;
    if (m_time <= 0.0) {
        event.m_count = static_cast<UInt32>((m_timeout - m_time) / m_timeout);
    }
}

bool
EventQueue::Timer::operator<(const Timer& t) const
{
    return m_time < t.m_time;
}
