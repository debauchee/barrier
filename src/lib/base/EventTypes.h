/*
 * barrier -- mouse and keyboard sharing utility
 * Copyright (C) 2013-2016 Symless Ltd.
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

#include "base/Event.h"

class IEventQueue;

class EventTypes {
public:
    EventTypes();
    void                setEvents(IEventQueue* events);

protected:
    IEventQueue*        getEvents() const;

private:
    IEventQueue*        m_events;
};

#define REGISTER_EVENT(type_, name_)                                    \
Event::Type                                                            \
type_##Events::name_()                                                    \
{                                                                        \
    return getEvents()->registerTypeOnce(m_##name_, __FUNCTION__);            \
}

class ClientEvents : public EventTypes {
public:
    ClientEvents() :
        m_connected(Event::kUnknown),
        m_connectionFailed(Event::kUnknown),
        m_disconnected(Event::kUnknown) { }

    //! @name accessors
    //@{

    //! Get connected event type
    /*!
    Returns the connected event type.  This is sent when the client has
    successfully connected to the server.
    */
    Event::Type        connected();

    //! Get connection failed event type
    /*!
    Returns the connection failed event type.  This is sent when the
    server fails for some reason.  The event data is a FailInfo*.
    */
    Event::Type        connectionFailed();

    //! Get disconnected event type
    /*!
    Returns the disconnected event type.  This is sent when the client
    has disconnected from the server (and only after having successfully
    connected).
    */
    Event::Type        disconnected();

    //@}

private:
    Event::Type        m_connected;
    Event::Type        m_connectionFailed;
    Event::Type        m_disconnected;
};

class IStreamEvents : public EventTypes {
public:
    IStreamEvents() :
        m_inputReady(Event::kUnknown),
        m_outputFlushed(Event::kUnknown),
        m_outputError(Event::kUnknown),
        m_inputShutdown(Event::kUnknown),
        m_outputShutdown(Event::kUnknown) { }

    //! @name accessors
    //@{

    //! Get input ready event type
    /*!
    Returns the input ready event type.  A stream sends this event
    when \c read() will return with data.
    */
    Event::Type        inputReady();

    //! Get output flushed event type
    /*!
    Returns the output flushed event type.  A stream sends this event
    when the output buffer has been flushed.  If there have been no
    writes since the event was posted, calling \c shutdownOutput() or
    \c close() will not discard any data and \c flush() will return
    immediately.
    */
    Event::Type        outputFlushed();

    //! Get output error event type
    /*!
    Returns the output error event type.  A stream sends this event
    when a write has failed.
    */
    Event::Type        outputError();

    //! Get input shutdown event type
    /*!
    Returns the input shutdown event type.  This is sent when the
    input side of the stream has shutdown.  When the input has
    shutdown, no more data will ever be available to read.
    */
    Event::Type        inputShutdown();

    //! Get output shutdown event type
    /*!
    Returns the output shutdown event type.  This is sent when the
    output side of the stream has shutdown.  When the output has
    shutdown, no more data can ever be written to the stream.  Any
    attempt to do so will generate a output error event.
    */
    Event::Type        outputShutdown();

    //@}
        
private:
    Event::Type        m_inputReady;
    Event::Type        m_outputFlushed;
    Event::Type        m_outputError;
    Event::Type        m_inputShutdown;
    Event::Type        m_outputShutdown;
};

class IpcClientEvents : public EventTypes {
public:
    IpcClientEvents() :
        m_connected(Event::kUnknown),
        m_messageReceived(Event::kUnknown) { }

    //! @name accessors
    //@{

    //! Raised when the socket is connected.
    Event::Type        connected();

    //! Raised when a message is received.
    Event::Type        messageReceived();

    //@}
    
private:
    Event::Type        m_connected;
    Event::Type        m_messageReceived;
};

class IpcClientProxyEvents : public EventTypes {
public:
    IpcClientProxyEvents() :
        m_messageReceived(Event::kUnknown),
        m_disconnected(Event::kUnknown) { }

    //! @name accessors
    //@{

    //! Raised when the server receives a message from a client.
    Event::Type        messageReceived();

    //! Raised when the client disconnects from the server.
    Event::Type        disconnected();
        
    //@}

private:
    Event::Type        m_messageReceived;
    Event::Type        m_disconnected;
};

class IpcServerEvents : public EventTypes {
public:
    IpcServerEvents() :
        m_clientConnected(Event::kUnknown),
        m_messageReceived(Event::kUnknown) { }

    //! @name accessors
    //@{

    //! Raised when we have created the client proxy.
    Event::Type        clientConnected();
    
    //! Raised when a message is received through a client proxy.
    Event::Type        messageReceived();

    //@}

private:
    Event::Type        m_clientConnected;
    Event::Type        m_messageReceived;
};

class IpcServerProxyEvents : public EventTypes {
public:
    IpcServerProxyEvents() :
        m_messageReceived(Event::kUnknown) { }

    //! @name accessors
    //@{

    //! Raised when the client receives a message from the server.
    Event::Type        messageReceived();

    //@}

private:
    Event::Type        m_messageReceived;
};

class IDataSocketEvents : public EventTypes {
public:
    IDataSocketEvents() :
        m_connected(Event::kUnknown),
        m_secureConnected(Event::kUnknown),
        m_connectionFailed(Event::kUnknown) { }

    //! @name accessors
    //@{

    //! Get connected event type
    /*!
    Returns the socket connected event type.  A socket sends this
    event when a remote connection has been established.
    */
    Event::Type        connected();
    
    //! Get secure connected event type
    /*!
     Returns the secure socket connected event type.  A secure socket sends
     this event when a remote connection has been established.
     */
    Event::Type        secureConnected();

    //! Get connection failed event type
    /*!
    Returns the socket connection failed event type.  A socket sends
    this event when an attempt to connect to a remote port has failed.
    The data is a pointer to a ConnectionFailedInfo.
    */
    Event::Type        connectionFailed();

    //@}

private:
    Event::Type        m_connected;
    Event::Type        m_secureConnected;
    Event::Type        m_connectionFailed;
};

class IListenSocketEvents : public EventTypes {
public:
    IListenSocketEvents() :
        m_connecting(Event::kUnknown) { }

    //! @name accessors
    //@{

    //! Get connecting event type
    /*!
    Returns the socket connecting event type.  A socket sends this
    event when a remote connection is waiting to be accepted.
    */
    Event::Type        connecting();

    //@}

private:
    Event::Type        m_connecting;
};

class ISocketEvents : public EventTypes {
public:
    ISocketEvents() :
        m_disconnected(Event::kUnknown),
        m_stopRetry(Event::kUnknown) { }

    //! @name accessors
    //@{

    //! Get disconnected event type
    /*!
    Returns the socket disconnected event type.  A socket sends this
    event when the remote side of the socket has disconnected or
    shutdown both input and output.
    */
    Event::Type        disconnected();

    //! Get stop retry event type
    /*!
     Returns the stop retry event type.  This is sent when the client
     doesn't want to reconnect after it disconnects from the server.
     */
    Event::Type        stopRetry();

    //@}

private:
    Event::Type        m_disconnected;
    Event::Type        m_stopRetry;
};

class OSXScreenEvents : public EventTypes {
public:
    OSXScreenEvents() :
        m_confirmSleep(Event::kUnknown) { }

    //! @name accessors
    //@{

    Event::Type        confirmSleep();

    //@}

private:
    Event::Type        m_confirmSleep;
};

class ClientListenerEvents : public EventTypes {
public:
    ClientListenerEvents() :
        m_accepted(Event::kUnknown),
        m_connected(Event::kUnknown) { }

    //! @name accessors
    //@{
    
    //! Get accepted event type
    /*!
     Returns the accepted event type.  This is sent whenever a server
     accepts a client.
     */
    Event::Type        accepted();
    
    //! Get connected event type
    /*!
    Returns the connected event type.  This is sent whenever a
    a client connects.
    */
    Event::Type        connected();

    //@}

private:
    Event::Type        m_accepted;
    Event::Type        m_connected;
};

class ClientProxyEvents : public EventTypes {
public:
    ClientProxyEvents() :
        m_ready(Event::kUnknown),
        m_disconnected(Event::kUnknown) { }

    //! @name accessors
    //@{

    //! Get ready event type
    /*!
    Returns the ready event type.  This is sent when the client has
    completed the initial handshake.  Until it is sent, the client is
    not fully connected.
    */
    Event::Type        ready();

    //! Get disconnect event type
    /*!
    Returns the disconnect event type.  This is sent when the client
    disconnects or is disconnected.  The target is getEventTarget().
    */
    Event::Type        disconnected();

    //@}

private:
    Event::Type        m_ready;
    Event::Type        m_disconnected;
};

class ClientProxyUnknownEvents : public EventTypes {
public:
    ClientProxyUnknownEvents() :
        m_success(Event::kUnknown),
        m_failure(Event::kUnknown) { }

    //! @name accessors
    //@{

    //! Get success event type
    /*!
    Returns the success event type.  This is sent when the client has
    correctly responded to the hello message.  The target is this.
    */
    Event::Type        success();

    //! Get failure event type
    /*!
    Returns the failure event type.  This is sent when a client fails
    to correctly respond to the hello message.  The target is this.
    */
    Event::Type        failure();

    //@}
        
private:
    Event::Type        m_success;
    Event::Type        m_failure;
};

class ServerEvents : public EventTypes {
public:
    ServerEvents() :
        m_error(Event::kUnknown),
        m_connected(Event::kUnknown),
        m_disconnected(Event::kUnknown),
        m_switchToScreen(Event::kUnknown),
        m_toggleScreen(Event::kUnknown),
        m_switchInDirection(Event::kUnknown),
        m_keyboardBroadcast(Event::kUnknown),
        m_lockCursorToScreen(Event::kUnknown),
        m_screenSwitched(Event::kUnknown) { }

    //! @name accessors
    //@{

    //! Get error event type
    /*!
    Returns the error event type.  This is sent when the server fails
    for some reason.
    */
    Event::Type        error();

    //! Get connected event type
    /*!
    Returns the connected event type.  This is sent when a client screen
    has connected.  The event data is a \c ScreenConnectedInfo* that
    indicates the connected screen.
    */
    Event::Type        connected();

    //! Get disconnected event type
    /*!
    Returns the disconnected event type.  This is sent when all the
    clients have disconnected.
    */
    Event::Type        disconnected();

    //! Get switch to screen event type
    /*!
    Returns the switch to screen event type.  The server responds to this
    by switching screens.  The event data is a \c SwitchToScreenInfo*
    that indicates the target screen.
    */
    Event::Type        switchToScreen();

    //! Get toggle screen event type
    /*!
    Returns the toggle screen event type.  The server responds to this
    by toggling screens.  These is no event data.
    */
    Event::Type        toggleScreen();

    //! Get switch in direction event type
    /*!
    Returns the switch in direction event type.  The server responds to this
    by switching screens.  The event data is a \c SwitchInDirectionInfo*
    that indicates the target direction.
    */
    Event::Type        switchInDirection();

    //! Get keyboard broadcast event type
    /*!
    Returns the keyboard broadcast event type.  The server responds
    to this by turning on keyboard broadcasting or turning it off.  The
    event data is a \c KeyboardBroadcastInfo*.
    */
    Event::Type        keyboardBroadcast();

    //! Get lock cursor event type
    /*!
    Returns the lock cursor event type.  The server responds to this
    by locking the cursor to the active screen or unlocking it.  The
    event data is a \c LockCursorToScreenInfo*.
    */
    Event::Type        lockCursorToScreen();

    //! Get screen switched event type
    /*!
    Returns the screen switched event type.  This is raised when the
    screen has been switched to a client.
    */
    Event::Type        screenSwitched();

    //@}
        
private:
    Event::Type        m_error;
    Event::Type        m_connected;
    Event::Type        m_disconnected;
    Event::Type        m_switchToScreen;
    Event::Type        m_toggleScreen;
    Event::Type        m_switchInDirection;
    Event::Type        m_keyboardBroadcast;
    Event::Type        m_lockCursorToScreen;
    Event::Type        m_screenSwitched;
};

class ServerAppEvents : public EventTypes {
public:
    ServerAppEvents() :
        m_reloadConfig(Event::kUnknown),
        m_forceReconnect(Event::kUnknown),
        m_resetServer(Event::kUnknown) { }
        
    //! @name accessors
    //@{
        
    Event::Type        reloadConfig();
    Event::Type        forceReconnect();
    Event::Type        resetServer();

    //@}
        
private:
    Event::Type        m_reloadConfig;
    Event::Type        m_forceReconnect;
    Event::Type        m_resetServer;
};

class IKeyStateEvents : public EventTypes {
public:
    IKeyStateEvents() :
        m_keyDown(Event::kUnknown),
        m_keyUp(Event::kUnknown),
        m_keyRepeat(Event::kUnknown) { }

    //! @name accessors
    //@{

    //! Get key down event type.  Event data is KeyInfo*, count == 1.
    Event::Type        keyDown();

    //! Get key up event type.  Event data is KeyInfo*, count == 1.
    Event::Type        keyUp();

    //! Get key repeat event type.  Event data is KeyInfo*.
    Event::Type        keyRepeat();

    //@}
        
private:
    Event::Type        m_keyDown;
    Event::Type        m_keyUp;
    Event::Type        m_keyRepeat;
};

class IPrimaryScreenEvents : public EventTypes {
public:
    IPrimaryScreenEvents() :
        m_buttonDown(Event::kUnknown),
        m_buttonUp(Event::kUnknown),
        m_motionOnPrimary(Event::kUnknown),
        m_motionOnSecondary(Event::kUnknown),
        m_wheel(Event::kUnknown),
        m_screensaverActivated(Event::kUnknown),
        m_screensaverDeactivated(Event::kUnknown),
        m_hotKeyDown(Event::kUnknown),
        m_hotKeyUp(Event::kUnknown),
        m_fakeInputBegin(Event::kUnknown),
        m_fakeInputEnd(Event::kUnknown) { }

    //! @name accessors
    //@{
    
    //!  button down event type.  Event data is ButtonInfo*.
    Event::Type        buttonDown();

    //!  button up event type.  Event data is ButtonInfo*.
    Event::Type        buttonUp();

    //!  mouse motion on the primary screen event type
    /*!
    Event data is MotionInfo* and the values are an absolute position.
    */
    Event::Type        motionOnPrimary();

    //!  mouse motion on a secondary screen event type
    /*!
    Event data is MotionInfo* and the values are motion deltas not
    absolute coordinates.
    */
    Event::Type        motionOnSecondary();

    //!  mouse wheel event type.  Event data is WheelInfo*.
    Event::Type        wheel();

    //!  screensaver activated event type
    Event::Type        screensaverActivated();

    //!  screensaver deactivated event type
    Event::Type        screensaverDeactivated();

    //!  hot key down event type.  Event data is HotKeyInfo*.
    Event::Type        hotKeyDown();

    //!  hot key up event type.  Event data is HotKeyInfo*.
    Event::Type        hotKeyUp();

    //!  start of fake input event type
    Event::Type        fakeInputBegin();

    //!  end of fake input event type
    Event::Type        fakeInputEnd();

    //@}

private:
    Event::Type        m_buttonDown;
    Event::Type        m_buttonUp;
    Event::Type        m_motionOnPrimary;
    Event::Type        m_motionOnSecondary;
    Event::Type        m_wheel;
    Event::Type        m_screensaverActivated;
    Event::Type        m_screensaverDeactivated;
    Event::Type        m_hotKeyDown;
    Event::Type        m_hotKeyUp;
    Event::Type        m_fakeInputBegin;
    Event::Type        m_fakeInputEnd;
};

class IScreenEvents : public EventTypes {
public:
    IScreenEvents() :
        m_error(Event::kUnknown),
        m_shapeChanged(Event::kUnknown),
        m_localInput(Event::kUnknown),
        m_suspend(Event::kUnknown),
        m_resume(Event::kUnknown) { }

    //! @name accessors
    //@{

    //! Get error event type
    /*!
    Returns the error event type.  This is sent whenever the screen has
    failed for some reason (e.g. the X Windows server died).
    */
    Event::Type        error();

    //! Get shape changed event type
    /*!
    Returns the shape changed event type.  This is sent whenever the
    screen's shape changes.
    */
    Event::Type        shapeChanged();

    //! Get local input event type
    /*!
    Returns the local input event type. This is sent when the cursor
    is not on the current screen but a local input is detected (e.g.
    via touchscreen inputs or mouse movements).
    */
    Event::Type        localInput();

    //! Get suspend event type
    /*!
    Returns the suspend event type. This is sent whenever the system goes
    to sleep or a user session is deactivated (fast user switching).
    */
    Event::Type        suspend();
    
    //! Get resume event type
    /*!
    Returns the resume event type. This is sent whenever the system wakes
    up or a user session is activated (fast user switching).
    */
    Event::Type        resume();

    //@}
        
private:
    Event::Type        m_error;
    Event::Type        m_shapeChanged;
    Event::Type        m_localInput;
    Event::Type        m_suspend;
    Event::Type        m_resume;
};

class ClipboardEvents : public EventTypes {
public:
    ClipboardEvents() :
        m_clipboardGrabbed(Event::kUnknown),
        m_clipboardChanged(Event::kUnknown),
        m_clipboardSending(Event::kUnknown) { }

    //! @name accessors
    //@{

    //! Get clipboard grabbed event type
    /*!
    Returns the clipboard grabbed event type.  This is sent whenever the
    clipboard is grabbed by some other application so we don't own it
    anymore.  The data is a pointer to a ClipboardInfo.
    */
    Event::Type        clipboardGrabbed();

    //! Get clipboard changed event type
    /*!
    Returns the clipboard changed event type.  This is sent whenever the
    contents of the clipboard has changed.  The data is a pointer to a
    IScreen::ClipboardInfo.
    */
    Event::Type        clipboardChanged();

    //! Clipboard sending event type
    /*!
    Returns the clipboard sending event type. This is used to send 
    clipboard chunks.
    */
    Event::Type        clipboardSending();

    //@}

private:
    Event::Type        m_clipboardGrabbed;
    Event::Type        m_clipboardChanged;
    Event::Type        m_clipboardSending;
};

class FileEvents : public EventTypes {
public:
    FileEvents() :
        m_fileChunkSending(Event::kUnknown),
        m_fileRecieveCompleted(Event::kUnknown),
        m_keepAlive(Event::kUnknown) { }

    //! @name accessors
    //@{

    //! Sending a file chunk
    Event::Type        fileChunkSending();

    //! Completed receiving a file
    Event::Type        fileRecieveCompleted();

    //! Send a keep alive
    Event::Type        keepAlive();

    //@}

private:
    Event::Type        m_fileChunkSending;
    Event::Type        m_fileRecieveCompleted;
    Event::Type        m_keepAlive;
};
