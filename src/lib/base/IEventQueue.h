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

#include "common/IInterface.h"
#include "base/Event.h"
#include <string>

class IEventJob;
class IEventQueueBuffer;

// Opaque type for timer info.  This is defined by subclasses of
// IEventQueueBuffer.
class EventQueueTimer;

// Event type registration classes.
class ClientEvents;
class IStreamEvents;
class IpcClientEvents;
class IpcClientProxyEvents;
class IpcServerEvents;
class IpcServerProxyEvents;
class IDataSocketEvents;
class IListenSocketEvents;
class ISocketEvents;
class OSXScreenEvents;
class ClientListenerEvents;
class ClientProxyEvents;
class ClientProxyUnknownEvents;
class ServerEvents;
class ServerAppEvents;
class IKeyStateEvents;
class IPrimaryScreenEvents;
class IScreenEvents;
class ClipboardEvents;
class FileEvents;

//! Event queue interface
/*!
An event queue provides a queue of Events.  Clients can block waiting
on any event becoming available at the head of the queue and can place
new events at the end of the queue.  Clients can also add and remove
timers which generate events periodically.
*/
class IEventQueue : public IInterface {
public:
    class TimerEvent {
    public:
        EventQueueTimer*    m_timer;    //!< The timer
        UInt32                m_count;    //!< Number of repeats
    };

    //! @name manipulators
    //@{

    //! Loop the event queue until quit
    /*!
    Dequeues and dispatches events until the kQuit event is found.
    */
    virtual void        loop() = 0;

    //! Set the buffer
    /*!
    Replace the current event queue buffer.  Any queued events are
    discarded.  The queue takes ownership of the buffer.
    */
    virtual void        adoptBuffer(IEventQueueBuffer*) = 0;

    //! Remove event from queue
    /*!
    Returns the next event on the queue into \p event.  If no event is
    available then blocks for up to \p timeout seconds, or forever if
    \p timeout is negative.  Returns true iff an event was available.
    */
    virtual bool        getEvent(Event& event, double timeout = -1.0) = 0;

    //! Dispatch an event
    /*!
    Looks up the dispatcher for the event's target and invokes it.
    Returns true iff a dispatcher exists for the target.
    */
    virtual bool        dispatchEvent(const Event& event) = 0;

    //! Add event to queue
    /*!
    Adds \p event to the end of the queue.
    */
    virtual void        addEvent(const Event& event) = 0;

    //! Create a recurring timer
    /*!
    Creates and returns a timer.  An event is returned after \p duration
    seconds and the timer is reset to countdown again.  When a timer event
    is returned the data points to a \c TimerEvent.  The client must pass
    the returned timer to \c deleteTimer() (whether or not the timer has
    expired) to release the timer.  The returned timer event uses the
    given \p target.  If \p target is NULL it uses the returned timer as
    the target.

    Events for a single timer don't accumulate in the queue, even if the
    client reading events can't keep up.  Instead, the \c m_count member
    of the \c TimerEvent indicates how many events for the timer would
    have been put on the queue since the last event for the timer was
    removed (or since the timer was added).
    */
    virtual EventQueueTimer*
                        newTimer(double duration, void* target) = 0;

    //! Create a one-shot timer
    /*!
    Creates and returns a one-shot timer.  An event is returned when
    the timer expires and the timer is removed from further handling.
    When a timer event is returned the data points to a \c TimerEvent.
    The c_count member of the \c TimerEvent is always 1.  The client
    must pass the returned timer to \c deleteTimer() (whether or not the
    timer has expired) to release the timer.  The returned timer event
    uses the given \p target.  If \p target is NULL it uses the returned
    timer as the target.
    */
    virtual EventQueueTimer*
                        newOneShotTimer(double duration,
                            void* target) = 0;

    //! Destroy a timer
    /*!
    Destroys a previously created timer.  The timer is removed from the
    queue and will not generate event, even if the timer has expired.
    */
    virtual void        deleteTimer(EventQueueTimer*) = 0;

    //! Register an event handler for an event type
    /*!
    Registers an event handler for \p type and \p target.  The \p handler
    is adopted.  Any existing handler for the type,target pair is deleted.
    \c dispatchEvent() will invoke \p handler for any event for \p target
    of type \p type.  If no such handler exists it will use the handler
    for \p target and type \p kUnknown if it exists.
    */
    virtual void        adoptHandler(Event::Type type,
                            void* target, IEventJob* handler) = 0;

    //! Unregister an event handler for an event type
    /*!
    Unregisters an event handler for the \p type, \p target pair and
    deletes it.
    */
    virtual void        removeHandler(Event::Type type, void* target) = 0;

    //! Unregister all event handlers for an event target
    /*!
    Unregisters all event handlers for the \p target and deletes them.
    */
    virtual void        removeHandlers(void* target) = 0;

    //! Creates a new event type
    /*!
    If \p type contains \c kUnknown then it is set to a unique event
    type id otherwise it is left alone.  The final value of \p type
    is returned.
    */
    virtual Event::Type
                        registerTypeOnce(Event::Type& type,
                            const char* name) = 0;

    //! Wait for event queue to become ready
    /*!
    Blocks on the current thread until the event queue is ready for events to
    be added.
    */
    virtual void        waitForReady() const = 0;
    //! Restore the terminal settings
    /*!
    Called to restore the terminal settings after daemonizing.
    */
	virtual void        restoreTerminal() const = 0;

    //@}
    //! @name accessors
    //@{

    //! Get an event handler
    /*!
    Finds and returns the event handler for the \p type, \p target pair
    if it exists, otherwise it returns NULL.
    */
    virtual IEventJob*    getHandler(Event::Type type, void* target) const = 0;

    //! Get name for event
    /*!
    Returns the name for the event \p type.  This is primarily for
    debugging.
    */
    virtual const char*    getTypeName(Event::Type type) = 0;

    //! Get an event type by name
    /*!
    Returns the registered type for an event for a given name.
    */
    virtual Event::Type getRegisteredType(const std::string& name) const = 0;

    //! Get the system event type target
    /*!
    Returns the target to use for dispatching \c Event::kSystem events.
    */
    virtual void*        getSystemTarget() = 0;

    //@}

    //
    // Event type providers.
    //

    virtual ClientEvents&                forClient() = 0;
    virtual IStreamEvents&                forIStream() = 0;
    virtual IpcClientEvents&            forIpcClient() = 0;
    virtual IpcClientProxyEvents&        forIpcClientProxy() = 0;
    virtual IpcServerEvents&            forIpcServer() = 0;
    virtual IpcServerProxyEvents&        forIpcServerProxy() = 0;
    virtual IDataSocketEvents&            forIDataSocket() = 0;
    virtual IListenSocketEvents&        forIListenSocket() = 0;
    virtual ISocketEvents&                forISocket() = 0;
    virtual OSXScreenEvents&            forOSXScreen() = 0;
    virtual ClientListenerEvents&        forClientListener() = 0;
    virtual ClientProxyEvents&            forClientProxy() = 0;
    virtual ClientProxyUnknownEvents&    forClientProxyUnknown() = 0;
    virtual ServerEvents&                forServer() = 0;
    virtual ServerAppEvents&            forServerApp() = 0;
    virtual IKeyStateEvents&            forIKeyState() = 0;
    virtual IPrimaryScreenEvents&        forIPrimaryScreen() = 0;
    virtual IScreenEvents&                forIScreen() = 0;
    virtual ClipboardEvents&            forClipboard() = 0;
    virtual FileEvents&                    forFile() = 0;
};
