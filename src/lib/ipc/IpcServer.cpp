/*
 * barrier -- mouse and keyboard sharing utility
 * Copyright (C) 2012-2016 Symless Ltd.
 * Copyright (C) 2012 Nick Bolton
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

#include "ipc/IpcServer.h"

#include "ipc/Ipc.h"
#include "ipc/IpcClientProxy.h"
#include "ipc/IpcMessage.h"
#include "net/IDataSocket.h"
#include "io/IStream.h"
#include "base/IEventQueue.h"
#include "base/TMethodEventJob.h"
#include "base/Event.h"
#include "base/Log.h"

//
// IpcServer
//

IpcServer::IpcServer(IEventQueue* events, SocketMultiplexer* socketMultiplexer) :
    m_mock(false),
    m_events(events),
    m_socketMultiplexer(socketMultiplexer),
    m_socket(nullptr),
    m_address(NetworkAddress(IPC_HOST, IPC_PORT))
{
    init();
}

IpcServer::IpcServer(IEventQueue* events, SocketMultiplexer* socketMultiplexer, int port) :
    m_mock(false),
    m_events(events),
    m_socketMultiplexer(socketMultiplexer),
    m_address(NetworkAddress(IPC_HOST, port))
{
    init();
}

void
IpcServer::init()
{
    m_socket = new TCPListenSocket(m_events, m_socketMultiplexer, IArchNetwork::kBLUETOOTH);

    m_clientsMutex = ARCH->newMutex();
    m_address.resolve();

    m_events->adoptHandler(
        m_events->forIListenSocket().connecting(), m_socket,
        new TMethodEventJob<IpcServer>(
        this, &IpcServer::handleClientConnecting));
}

IpcServer::~IpcServer()
{
    if (m_mock) {
        return;
    }

    if (m_socket != nullptr) {
        delete m_socket;
    }

    ARCH->lockMutex(m_clientsMutex);
    ClientList::iterator it;
    for (it = m_clients.begin(); it != m_clients.end(); it++) {
        deleteClient(*it);
    }
    m_clients.clear();
    ARCH->unlockMutex(m_clientsMutex);
    ARCH->closeMutex(m_clientsMutex);
    
    m_events->removeHandler(m_events->forIListenSocket().connecting(), m_socket);
}

void
IpcServer::listen()
{
    m_socket->bind(m_address);
}

void
IpcServer::handleClientConnecting(const Event&, void*)
{
    barrier::IStream* stream = m_socket->accept();
    if (stream == NULL) {
        return;
    }

    LOG((CLOG_DEBUG "accepted ipc client connection"));

    ARCH->lockMutex(m_clientsMutex);
    IpcClientProxy* proxy = new IpcClientProxy(*stream, m_events);
    m_clients.push_back(proxy);
    ARCH->unlockMutex(m_clientsMutex);

    m_events->adoptHandler(
        m_events->forIpcClientProxy().disconnected(), proxy,
        new TMethodEventJob<IpcServer>(
        this, &IpcServer::handleClientDisconnected));

    m_events->adoptHandler(
        m_events->forIpcClientProxy().messageReceived(), proxy,
        new TMethodEventJob<IpcServer>(
        this, &IpcServer::handleMessageReceived));

    m_events->addEvent(Event(
        m_events->forIpcServer().clientConnected(), this, proxy, Event::kDontFreeData));
}

void
IpcServer::handleClientDisconnected(const Event& e, void*)
{
    IpcClientProxy* proxy = static_cast<IpcClientProxy*>(e.getTarget());

    ArchMutexLock lock(m_clientsMutex);
    m_clients.remove(proxy);
    deleteClient(proxy);

    LOG((CLOG_DEBUG "ipc client proxy removed, connected=%d", m_clients.size()));
}

void
IpcServer::handleMessageReceived(const Event& e, void*)
{
    Event event(m_events->forIpcServer().messageReceived(), this);
    event.setDataObject(e.getDataObject());
    m_events->addEvent(event);
}

void
IpcServer::deleteClient(IpcClientProxy* proxy)
{
    m_events->removeHandler(m_events->forIpcClientProxy().messageReceived(), proxy);
    m_events->removeHandler(m_events->forIpcClientProxy().disconnected(), proxy);
    delete proxy;
}

bool
IpcServer::hasClients(EIpcClientType clientType) const
{
    ArchMutexLock lock(m_clientsMutex);

    if (m_clients.empty()) {
        return false;
    }

    ClientList::const_iterator it;
    for (it = m_clients.begin(); it != m_clients.end(); it++) {
        // at least one client is alive and type matches, there are clients.
        IpcClientProxy* p = *it;
        if (!p->m_disconnecting && p->m_clientType == clientType) {
            return true;
        }
    }

    // all clients must be disconnecting, no active clients.
    return false;
}

void
IpcServer::send(const IpcMessage& message, EIpcClientType filterType)
{
    ArchMutexLock lock(m_clientsMutex);

    ClientList::iterator it;
    for (it = m_clients.begin(); it != m_clients.end(); it++) {
        IpcClientProxy* proxy = *it;
        if (proxy->m_clientType == filterType) {
            proxy->send(message);
        }
    }
}
