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

#include "arch/IArchNetwork.h"
#include "arch/IArchMultithread.h"

#if HAVE_SYS_TYPES_H
#    include <sys/types.h>
#endif
#if HAVE_SYS_SOCKET_H
#    include <sys/socket.h>
#endif
#if !HAVE_SOCKLEN_T
typedef int socklen_t;
#endif
#if HAVE_BLUETOOTH
#    include <bluetooth/sdp.h>
#    include <bluetooth/sdp_lib.h>
#endif

// old systems may use char* for [gs]etsockopt()'s optval argument.
// this should be void on modern systems but char is forwards
// compatible so we always use it.
typedef char optval_t;

#define ARCH_NETWORK ArchNetworkBSD
#define TYPED_ADDR(type_, addr_) (reinterpret_cast<type_*>(&addr_->m_addr))

class ArchSocketImpl {
public:
    int                    m_fd;
    int                    m_refCount;
    IArchNetwork::EAddressFamily                 m_family;
};

class ArchNetAddressImpl {
public:
    ArchNetAddressImpl() : m_len(sizeof(m_addr)) { }

public:
    struct sockaddr_storage        m_addr;
    socklen_t                      m_len;
};

//! Berkeley (BSD) sockets implementation of IArchNetwork
class ArchNetworkBSD : public IArchNetwork {
public:
    ArchNetworkBSD();
    virtual ~ArchNetworkBSD();

    virtual void init();

    // IArchNetwork overrides
    virtual ArchSocket     newSocket(EAddressFamily, ESocketType);
    virtual ArchSocket     copySocket(ArchSocket s);    virtual void        closeSocket(ArchSocket s);
    virtual void        closeSocketForRead(ArchSocket s);
    virtual void        closeSocketForWrite(ArchSocket s);
    virtual void        bindSocket(ArchSocket s, ArchNetAddress addr);
    virtual void        listenOnSocket(ArchSocket s);
    virtual ArchSocket    acceptSocket(ArchSocket s, ArchNetAddress* addr);
    virtual bool        connectSocket(ArchSocket s, ArchNetAddress name);
    virtual int            pollSocket(PollEntry[], int num, double timeout);
    virtual void        unblockPollSocket(ArchThread thread);
    virtual size_t        readSocket(ArchSocket s, void* buf, size_t len);
    virtual size_t        writeSocket(ArchSocket s,
                            const void* buf, size_t len);
    virtual void        throwErrorOnSocket(ArchSocket);
    virtual bool        setNoDelayOnSocket(ArchSocket, bool noDelay);
    virtual bool        setReuseAddrOnSocket(ArchSocket, bool reuse);
    virtual std::string        getHostName();
    virtual ArchNetAddress    newAnyAddr(EAddressFamily);
    virtual ArchNetAddress    copyAddr(ArchNetAddress);
    virtual ArchNetAddress    nameToAddr(const std::string&);
    virtual void            closeAddr(ArchNetAddress);
    virtual std::string        addrToName(ArchNetAddress);
    virtual std::string        addrToString(ArchNetAddress);
    virtual EAddressFamily    getAddrFamily(ArchNetAddress);
    virtual void            setAddrPort(ArchNetAddress, int port);
    virtual int                getAddrPort(ArchNetAddress);
    virtual bool            isAnyAddr(ArchNetAddress);
    virtual bool            isEqualAddr(ArchNetAddress, ArchNetAddress);

private:
    const int*            getUnblockPipe();
    const int*            getUnblockPipeForThread(ArchThread);
    void                setBlockingOnSocket(int fd, bool blocking);
    void                throwError(int);
    void                throwNameError(int);

#if HAVE_BLUETOOTH
    sdp_session_t        *register_service(uint8_t rfcomm_channel);
    ArchNetAddress      find_channel(ArchNetAddress addr);
    ArchNetAddress      find_service(ArchNetAddress addr);
#endif

private:
    ArchMutex            m_mutex;

#if HAVE_BLUETOOTH
    sdp_session_t *      sdp_session;
#endif
};
