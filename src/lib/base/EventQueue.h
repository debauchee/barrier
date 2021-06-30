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

#pragma once

#include "mt/CondVar.h"
#include "arch/IArchMultithread.h"
#include "base/IEventQueue.h"
#include "base/Event.h"
#include "base/PriorityQueue.h"
#include "base/Stopwatch.h"
#include "common/stdmap.h"
#include "common/stdset.h"
#include "base/NonBlockingStream.h"

#include <mutex>
#include <queue>

//! Event queue
/*!
An event queue that implements the platform independent parts and
delegates the platform dependent parts to a subclass.
*/
class EventQueue : public IEventQueue {
public:
    EventQueue();
    virtual ~EventQueue();

    // IEventQueue overrides
    virtual void        loop();
    virtual void        adoptBuffer(IEventQueueBuffer*);
    virtual bool        getEvent(Event& event, double timeout = -1.0);
    virtual bool        dispatchEvent(const Event& event);
    virtual void        addEvent(const Event& event);
    virtual EventQueueTimer*
                        newTimer(double duration, void* target);
    virtual EventQueueTimer*
                        newOneShotTimer(double duration, void* target);
    virtual void        deleteTimer(EventQueueTimer*);
    virtual void        adoptHandler(Event::Type type,
                            void* target, IEventJob* handler);
    virtual void        removeHandler(Event::Type type, void* target);
    virtual void        removeHandlers(void* target);
    virtual Event::Type
                        registerTypeOnce(Event::Type& type, const char* name);
    virtual IEventJob*    getHandler(Event::Type type, void* target) const;
    virtual const char*    getTypeName(Event::Type type);
    virtual Event::Type getRegisteredType(const std::string& name) const;
    void*                getSystemTarget();
    virtual void        waitForReady() const;
	virtual void        restoreTerminal() const { m_parentStream.restoreTerminal(); }

private:
    UInt32                saveEvent(const Event& event);
    Event                removeEvent(UInt32 eventID);
    bool                hasTimerExpired(Event& event);
    double                getNextTimerTimeout() const;
    void                addEventToBuffer(const Event& event);
    bool                parent_requests_shutdown() const;

private:
    class Timer {
    public:
        Timer(EventQueueTimer*, double timeout, double initialTime,
                            void* target, bool oneShot);
        ~Timer();

        void            reset();

        Timer&            operator-=(double);

                        operator double() const;

        bool            isOneShot() const;
        EventQueueTimer*
                        getTimer() const;
        void*            getTarget() const;
        void            fillEvent(TimerEvent&) const;

        bool            operator<(const Timer&) const;

    private:
        EventQueueTimer*    m_timer;
        double                m_timeout;
        void*                m_target;
        bool                m_oneShot;
        double                m_time;
    };

    typedef std::set<EventQueueTimer*> Timers;
    typedef PriorityQueue<Timer> TimerQueue;
    typedef std::map<UInt32, Event> EventTable;
    typedef std::vector<UInt32> EventIDList;
    typedef std::map<Event::Type, const char*> TypeMap;
    typedef std::map<std::string, Event::Type> NameMap;
    typedef std::map<Event::Type, IEventJob*> TypeHandlerTable;
    typedef std::map<void*, TypeHandlerTable> HandlerTable;

    int                    m_systemTarget;
    mutable std::mutex m_mutex;

    // registered events
    Event::Type        m_nextType;
    TypeMap            m_typeMap;
    NameMap            m_nameMap;

    // buffer of events
    IEventQueueBuffer*    m_buffer;

    // saved events
    EventTable            m_events;
    EventIDList        m_oldEventIDs;

    // timers
    Stopwatch            m_time;
    Timers                m_timers;
    TimerQueue            m_timerQueue;
    TimerEvent            m_timerEvent;

    // event handlers
    HandlerTable        m_handlers;

public:
    //
    // Event type providers.
    //
    ClientEvents&                forClient();
    IStreamEvents&                forIStream();
    IpcClientEvents&            forIpcClient();
    IpcClientProxyEvents&        forIpcClientProxy();
    IpcServerEvents&            forIpcServer();
    IpcServerProxyEvents&        forIpcServerProxy();
    IDataSocketEvents&            forIDataSocket();
    IListenSocketEvents&        forIListenSocket();
    ISocketEvents&                forISocket();
    OSXScreenEvents&            forOSXScreen();
    ClientListenerEvents&        forClientListener();
    ClientProxyEvents&            forClientProxy();
    ClientProxyUnknownEvents&    forClientProxyUnknown();
    ServerEvents&                forServer();
    ServerAppEvents&            forServerApp();
    IKeyStateEvents&            forIKeyState();
    IPrimaryScreenEvents&        forIPrimaryScreen();
    IScreenEvents&                forIScreen();
    ClipboardEvents&            forClipboard();
    FileEvents&                    forFile();

private:
    ClientEvents*                m_typesForClient;
    IStreamEvents*                m_typesForIStream;
    IpcClientEvents*            m_typesForIpcClient;
    IpcClientProxyEvents*        m_typesForIpcClientProxy;
    IpcServerEvents*            m_typesForIpcServer;
    IpcServerProxyEvents*        m_typesForIpcServerProxy;
    IDataSocketEvents*            m_typesForIDataSocket;
    IListenSocketEvents*        m_typesForIListenSocket;
    ISocketEvents*                m_typesForISocket;
    OSXScreenEvents*            m_typesForOSXScreen;
    ClientListenerEvents*        m_typesForClientListener;
    ClientProxyEvents*            m_typesForClientProxy;
    ClientProxyUnknownEvents*    m_typesForClientProxyUnknown;
    ServerEvents*                m_typesForServer;
    ServerAppEvents*            m_typesForServerApp;
    IKeyStateEvents*            m_typesForIKeyState;
    IPrimaryScreenEvents*        m_typesForIPrimaryScreen;
    IScreenEvents*                m_typesForIScreen;
    ClipboardEvents*            m_typesForClipboard;
    FileEvents*                    m_typesForFile;
    Mutex*                        m_readyMutex;
    CondVar<bool>*                m_readyCondVar;
    std::queue<Event>            m_pending;
    NonBlockingStream            m_parentStream;
};

#define EVENT_TYPE_ACCESSOR(type_)                                            \
type_##Events&                                                                \
EventQueue::for##type_() {                                                \
    if (m_typesFor##type_ == NULL) {                                        \
        m_typesFor##type_ = new type_##Events();                            \
        m_typesFor##type_->setEvents(dynamic_cast<IEventQueue*>(this));        \
    }                                                                        \
    return *m_typesFor##type_;                                                \
}
