/*
 * barrier -- mouse and keyboard sharing utility
 * Copyright (C) 2012-2016 Symless Ltd.
 * Copyright (C) 2002 Chris Schoeneman
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

#include "barrier/clipboard_types.h"
#include "barrier/key_types.h"
#include "base/Event.h"
#include "base/Stopwatch.h"

class Client;
class ClientInfo;
class EventQueueTimer;
class IClipboard;
namespace barrier { class IStream; }
class IEventQueue;

//! Proxy for server
/*!
This class acts a proxy for the server, converting calls into messages
to the server and messages from the server to calls on the client.
*/
class ServerProxy {
public:
    /*!
    Process messages from the server on \p stream and forward to
    \p client.
    */
    ServerProxy(Client* client, barrier::IStream* stream, IEventQueue* events);
    ~ServerProxy();

    //! @name manipulators
    //@{

    void                onInfoChanged();
    bool                onGrabClipboard(ClipboardID);
    void                onClipboardChanged(ClipboardID, const IClipboard*);
    void                onLocalInput(SInt32 x, SInt32 y);

    //@}

    // sending file chunk to server
    void                fileChunkSending(UInt8 mark, char* data, size_t dataSize);

    // sending dragging information to server
    void                sendDragInfo(UInt32 fileCount, const char* info, size_t size);

    // Send screensaver information to server
    void                sendScreensaver(bool activate);

#ifdef BARRIER_TEST_ENV
    void                handleDataForTest() { handleData(Event(), NULL); }
#endif

protected:
    enum EResult { kOkay, kUnknown, kDisconnect };
    EResult                parseHandshakeMessage(const UInt8* code);
    EResult                parseMessage(const UInt8* code);

private:
    // if compressing mouse motion then send the last motion now
    void                flushCompressedMouse();

    void                sendInfo(const ClientInfo&);

    void                resetKeepAliveAlarm();
    void                setKeepAliveRate(double);

    // modifier key translation
    KeyID                translateKey(KeyID) const;
    KeyModifierMask            translateModifierMask(KeyModifierMask) const;

    // event handlers
    void                handleData(const Event&, void*);
    void                handleKeepAliveAlarm(const Event&, void*);

    // message handlers
    void                enter();
    void                leave();
    void                setClipboard();
    void                grabClipboard();
    void                keyDown();
    void                keyRepeat();
    void                keyUp();
    void                mouseDown();
    void                mouseUp();
    void                mouseMove();
    void                mouseRelativeMove();
    void                mouseWheel();
    void                rcvScreensaver();
    void                resetOptions();
    void                setOptions();
    void                queryInfo();
    void                infoAcknowledgment();
    void                fileChunkReceived();
    void                dragInfoReceived();
    void                handleClipboardSendingEvent(const Event&, void*);

private:
    typedef EResult (ServerProxy::*MessageParser)(const UInt8*);

    Client*            m_client;
    barrier::IStream*    m_stream;

    UInt32                m_seqNum;

    bool                m_compressMouse;
    bool                m_compressMouseRelative;
    SInt32                m_xMouse, m_yMouse;
    SInt32                m_dxMouse, m_dyMouse;

    bool                m_ignoreMouse;

    KeyModifierID        m_modifierTranslationTable[kKeyModifierIDLast];

    double                m_keepAliveAlarm;
    EventQueueTimer*    m_keepAliveAlarmTimer;

    MessageParser        m_parser;
    IEventQueue*        m_events;
};
