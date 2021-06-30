/*
 * barrier -- mouse and keyboard sharing utility
 * Copyright (C) 2012-2016 Symless Ltd.
 * Copyright (C) 2011 Nick Bolton
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

#include "base/IEventQueue.h"

#include "test/global/gmock.h"

class MockEventQueue : public IEventQueue
{
public:
    MOCK_METHOD0(loop, void());
    MOCK_METHOD2(newOneShotTimer, EventQueueTimer*(double, void*));
    MOCK_METHOD2(newTimer, EventQueueTimer*(double, void*));
    MOCK_METHOD2(getEvent, bool(Event&, double));
    MOCK_METHOD1(adoptBuffer, void(IEventQueueBuffer*));
    MOCK_METHOD2(registerTypeOnce, Event::Type(Event::Type&, const char*));
    MOCK_METHOD1(removeHandlers, void(void*));
    MOCK_METHOD1(registerType, Event::Type(const char*));
    MOCK_CONST_METHOD0(isEmpty, bool());
    MOCK_METHOD3(adoptHandler, void(Event::Type, void*, IEventJob*));
    MOCK_METHOD1(getTypeName, const char*(Event::Type));
    MOCK_METHOD1(addEvent, void(const Event&));
    MOCK_METHOD2(removeHandler, void(Event::Type, void*));
    MOCK_METHOD1(dispatchEvent, bool(const Event&));
    MOCK_CONST_METHOD2(getHandler, IEventJob*(Event::Type, void*));
    MOCK_METHOD1(deleteTimer, void(EventQueueTimer*));
    MOCK_CONST_METHOD1(getRegisteredType, Event::Type(const std::string&));
    MOCK_METHOD0(getSystemTarget, void*());
    MOCK_METHOD0(forClient, ClientEvents&());
    MOCK_METHOD0(forIStream, IStreamEvents&());
    MOCK_METHOD0(forIpcClient, IpcClientEvents&());
    MOCK_METHOD0(forIpcClientProxy, IpcClientProxyEvents&());
    MOCK_METHOD0(forIpcServer, IpcServerEvents&());
    MOCK_METHOD0(forIpcServerProxy, IpcServerProxyEvents&());
    MOCK_METHOD0(forIDataSocket, IDataSocketEvents&());
    MOCK_METHOD0(forIListenSocket, IListenSocketEvents&());
    MOCK_METHOD0(forISocket, ISocketEvents&());
    MOCK_METHOD0(forOSXScreen, OSXScreenEvents&());
    MOCK_METHOD0(forClientListener, ClientListenerEvents&());
    MOCK_METHOD0(forClientProxy, ClientProxyEvents&());
    MOCK_METHOD0(forClientProxyUnknown, ClientProxyUnknownEvents&());
    MOCK_METHOD0(forServer, ServerEvents&());
    MOCK_METHOD0(forServerApp, ServerAppEvents&());
    MOCK_METHOD0(forIKeyState, IKeyStateEvents&());
    MOCK_METHOD0(forIPrimaryScreen, IPrimaryScreenEvents&());
    MOCK_METHOD0(forIScreen, IScreenEvents&());
    MOCK_METHOD0(forClipboard, ClipboardEvents&());
    MOCK_METHOD0(forFile, FileEvents&());
    MOCK_CONST_METHOD0(waitForReady, void());
    MOCK_CONST_METHOD0(restoreTerminal, void());
};
