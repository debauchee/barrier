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

#include "arch/unix/ArchNetworkBSD.h"

#include "arch/unix/ArchMultithreadPosix.h"
#include "arch/unix/XArchUnix.h"
#include "arch/Arch.h"
#include "base/Log.h"

#if HAVE_UNISTD_H
#    include <unistd.h>
#endif
#include <netinet/in.h>
#include <netdb.h>
#if !defined(TCP_NODELAY)
#    include <netinet/tcp.h>
#endif
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#if HAVE_POLL
#    include <poll.h>
#else
#    if HAVE_SYS_SELECT_H
#        include <sys/select.h>
#    endif
#    if HAVE_SYS_TIME_H
#        include <sys/time.h>
#    endif
#endif

#if !HAVE_INET_ATON
#    include <stdio.h>
#endif

static const int s_family[] = {
    PF_UNSPEC,
    PF_INET,
    PF_INET6,
    PF_BLUETOOTH
};
static const int s_type[] = {
    SOCK_DGRAM,
    SOCK_STREAM
};

#if !HAVE_INET_ATON
// parse dotted quad addresses.  we don't bother with the weird BSD'ism
// of handling octal and hex and partial forms.
static
in_addr_t
inet_aton(const char* cp, struct in_addr* inp)
{
    unsigned int a, b, c, d;
    if (sscanf(cp, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) {
        return 0;
    }
    if (a >= 256 || b >= 256 || c >= 256 || d >= 256) {
        return 0;
    }
    unsigned char* incp = (unsigned char*)inp;
    incp[0] = (unsigned char)(a & 0xffu);
    incp[1] = (unsigned char)(b & 0xffu);
    incp[2] = (unsigned char)(c & 0xffu);
    incp[3] = (unsigned char)(d & 0xffu);
    return inp->s_addr;
}
#endif

#if HAVE_BLUETOOTH
#    include <bluetooth/bluetooth.h>
#    include <bluetooth/rfcomm.h>
#    include <bluetooth/hci.h>
#    include <bluetooth/hci_lib.h>

static const bdaddr_t bdaddr_any = {0, 0, 0, 0, 0, 0};
static const bdaddr_t bdaddr_local = {0, 0, 0, 0xff, 0xff, 0xff};
uint32_t service_uuid_int[] = { 0x0e92afb7, 0xaeca4c00, 0x950d6f96, 0xc3cd5b16 };

sdp_session_t *ArchNetworkBSD::register_service(uint8_t rfcomm_channel)
{
    const char *service_name = "Barrier";
    const char *service_dsc = "Mouse and keyboard sharing";
    const char *service_prov = "Barrier";

    uuid_t root_uuid, l2cap_uuid, rfcomm_uuid, svc_uuid;
    sdp_list_t *l2cap_list = 0,
               *rfcomm_list = 0,
               *root_list = 0,
               *proto_list = 0,
               *access_proto_list = 0;
    sdp_data_t *channel = 0, *psm = 0;

    sdp_record_t *record = sdp_record_alloc();

    // set the general service ID
    sdp_uuid128_create( &svc_uuid, &service_uuid_int );
    sdp_set_service_id( record, svc_uuid );

    // make the service record publicly browsable
    sdp_uuid16_create(&root_uuid, PUBLIC_BROWSE_GROUP);
    root_list = sdp_list_append(0, &root_uuid);
    sdp_set_browse_groups( record, root_list );

    // set l2cap information
    sdp_uuid16_create(&l2cap_uuid, L2CAP_UUID);
    l2cap_list = sdp_list_append( 0, &l2cap_uuid );
    proto_list = sdp_list_append( 0, l2cap_list );

    // set rfcomm information
    sdp_uuid16_create(&rfcomm_uuid, RFCOMM_UUID);
    channel = sdp_data_alloc(SDP_UINT8, &rfcomm_channel);
    rfcomm_list = sdp_list_append( 0, &rfcomm_uuid );
    sdp_list_append( rfcomm_list, channel );
    sdp_list_append( proto_list, rfcomm_list );

    // attach protocol information to service record
    access_proto_list = sdp_list_append( 0, proto_list );
    sdp_set_access_protos( record, access_proto_list );

    // set the name, provider, and description
    sdp_set_info_attr(record, service_name, service_prov, service_dsc);

    int err = 0;
    sdp_session_t *session = 0;

    // connect to the local SDP server, register the service record, and
    // disconnect
    session = sdp_connect( &bdaddr_any, &bdaddr_local, SDP_RETRY_IF_BUSY );
    err = sdp_record_register(session, record, 0);

    // cleanup
    sdp_data_free( channel );
    sdp_list_free( l2cap_list, 0 );
    sdp_list_free( rfcomm_list, 0 );
    sdp_list_free( root_list, 0 );
    sdp_list_free( access_proto_list, 0 );

    return session;
}

ArchNetAddress ArchNetworkBSD::find_channel(ArchNetAddress addr) {
       struct sockaddr_rc *Baddr = reinterpret_cast<struct sockaddr_rc*>(&addr->m_addr);

       LOG((CLOG_INFO "checking services on %s", addrToString(addr).c_str()));

       uuid_t svc_uuid;
       int err;
       sdp_list_t *response_list = NULL, *search_list, *attrid_list;
       sdp_session_t *session = 0;

       // connect to the SDP server running on the remote machine
       session = sdp_connect( &bdaddr_any, &Baddr->rc_bdaddr, SDP_RETRY_IF_BUSY );
       if(!session) {
               throwError(errno);
       }

       // specify the UUID of the application we're searching for
       sdp_uuid128_create( &svc_uuid, &service_uuid_int );
       search_list = sdp_list_append( NULL, &svc_uuid );

       // specify that we want a list of all the matching applications' attributes
       uint32_t range = 0x0000ffff;
       attrid_list = sdp_list_append( NULL, &range );

       // get a list of service records that have UUID 0xabcd
       err = sdp_service_search_attr_req( session, search_list,
               SDP_ATTR_REQ_RANGE, attrid_list, &response_list);

       sdp_list_t *r = response_list;

       // go through each of the service records
       for (; r; r = r->next ) {
         sdp_record_t *rec = (sdp_record_t*) r->data;
         sdp_list_t *proto_list;

         // get a list of the protocol sequences
         if( sdp_get_access_protos( rec, &proto_list ) == 0 ) {
         sdp_list_t *p = proto_list;

         // go through each protocol sequence
         for( ; p ; p = p->next ) {
               sdp_list_t *pds = (sdp_list_t*)p->data;

               // go through each protocol list of the protocol sequence
               for( ; pds ; pds = pds->next ) {

                   // check the protocol attributes
                   sdp_data_t *d = (sdp_data_t*)pds->data;
                   int proto = 0;
                   for( ; d; d = d->next ) {
                       switch( d->dtd ) {
                           case SDP_UUID16:
                           case SDP_UUID32:
                           case SDP_UUID128:
                               proto = sdp_uuid_to_proto( &d->val.uuid );
                               break;
                           case SDP_UINT8:
                               if( proto == RFCOMM_UUID ) {
                                       LOG((CLOG_INFO "Found synergy service on channel %d", d->val.int8));
                                       Baddr->rc_channel = d->val.int8;
                               }
                               break;
                       }
                   }
               }
               sdp_list_free( (sdp_list_t*)p->data, 0 );
         }
         sdp_list_free( proto_list, 0 );
         }
         sdp_record_free( rec );
       }

       sdp_close(session);
       if(Baddr->rc_channel == 0)
               throw XArchNetwork("Synergy service not available on remote device");

       return addr;
}

ArchNetAddress ArchNetworkBSD::find_service(ArchNetAddress addr) {
       if(isAnyAddr(addr)) {
               LOG((CLOG_INFO "Scanning for bluetooth devices..."));
               inquiry_info *ii = NULL;
               int max_rsp, num_rsp;
               int dev_id, sock, len, flags;
               int i;
               char _addr[19] = { 0 };
               char name[248] = { 0 };

               dev_id = hci_get_route(NULL);
               sock = hci_open_dev( dev_id );
               if (dev_id < 0 || sock < 0) {
                       throwError(errno);
               }

               len  = 8;
               max_rsp = 255;
               flags = IREQ_CACHE_FLUSH;
               ii = (inquiry_info*)malloc(max_rsp * sizeof(inquiry_info));

               ArchNetAddress temp_addr = copyAddr(addr);
               struct sockaddr_rc *temp_baddr = reinterpret_cast<struct sockaddr_rc*>(&temp_addr->m_addr);

               num_rsp = hci_inquiry(dev_id, len, max_rsp, NULL, &ii, flags);
               if( num_rsp < 0 ) perror("hci_inquiry");

               for (i = 0; i < num_rsp; i++) {
                       ba2str(&(ii+i)->bdaddr, _addr);
                       memset(name, 0, sizeof(name));
                       if (hci_read_remote_name(sock, &(ii+i)->bdaddr, sizeof(name),
                       name, 0) < 0)
                       strcpy(name, "[unknown]");
                       LOG((CLOG_INFO "Found device: %s %s", _addr, name));

                       try {
                               temp_baddr->rc_bdaddr = (ii+i)->bdaddr;
                               find_channel(temp_addr);
                       }
                       catch(...) {
                               continue;
                       }
                       //found a device
                       *addr = *temp_addr;
                       break;
               }

               delete temp_addr;
               free( ii );
               hci_close_dev(sock);

               if(isAnyAddr(addr)) {
                       throw XArchNetwork("No devices with synergy service found");
               }

               return addr;

       } else
               return find_channel(addr);
}

#endif

//
// ArchNetworkBSD
//

ArchNetworkBSD::ArchNetworkBSD()
{
}

ArchNetworkBSD::~ArchNetworkBSD()
{
    ARCH->closeMutex(m_mutex);
}

void
ArchNetworkBSD::init()
{
    // create mutex to make some calls thread safe
    m_mutex = ARCH->newMutex();
#if HAVE_BLUETOOTH
    sdp_session = NULL;
#endif
}

ArchSocket
ArchNetworkBSD::newSocket(EAddressFamily family, ESocketType type)
{
    // create socket
    int protocol = 0;
#if HAVE_BLUETOOTH
    if (family == kBLUETOOTH) protocol = BTPROTO_RFCOMM;
#endif
    int fd = socket(s_family[family], s_type[type], protocol);
    if (fd == -1) {
        throwError(errno);
    }
    try {
        setBlockingOnSocket(fd, false);
    }
    catch (...) {
        close(fd);
        throw;
    }

#if HAVE_BLUETOOTH
    if(family == kBLUETOOTH) {
        int opt = 0;
        opt |= RFCOMM_LM_AUTH;
        opt |= RFCOMM_LM_ENCRYPT;
        opt |= RFCOMM_LM_SECURE;
        setsockopt(fd, SOL_RFCOMM, RFCOMM_LM, &opt, sizeof(opt));
    }
#endif

    // allocate socket object
    ArchSocketImpl* newSocket = new ArchSocketImpl;
    newSocket->m_fd            = fd;
    newSocket->m_refCount      = 1;
    newSocket->m_family        = family;
    return newSocket;
}

ArchSocket
ArchNetworkBSD::copySocket(ArchSocket s)
{
    assert(s != NULL);

    // ref the socket and return it
    ARCH->lockMutex(m_mutex);
    ++s->m_refCount;
    ARCH->unlockMutex(m_mutex);
    return s;
}

void
ArchNetworkBSD::closeSocket(ArchSocket s)
{
    assert(s != NULL);

#if HAVE_BLUETOOTH
    if(s->m_family == kBLUETOOTH && sdp_session != NULL)
        sdp_close(sdp_session);
#endif

    // unref the socket and note if it should be released
    ARCH->lockMutex(m_mutex);
    const bool doClose = (--s->m_refCount == 0);
    ARCH->unlockMutex(m_mutex);

    // close the socket if necessary
    if (doClose) {
        if (close(s->m_fd) == -1) {
            // close failed.  restore the last ref and throw.
            int err = errno;
            ARCH->lockMutex(m_mutex);
            ++s->m_refCount;
            ARCH->unlockMutex(m_mutex);
            throwError(err);
        }
        delete s;
    }
}

void
ArchNetworkBSD::closeSocketForRead(ArchSocket s)
{
    assert(s != NULL);

    if (shutdown(s->m_fd, 0) == -1) {
        if (errno != ENOTCONN) {
            throwError(errno);
        }
    }
}

void
ArchNetworkBSD::closeSocketForWrite(ArchSocket s)
{
    assert(s != NULL);

    if (shutdown(s->m_fd, 1) == -1) {
        if (errno != ENOTCONN) {
            throwError(errno);
        }
    }
}

void
ArchNetworkBSD::bindSocket(ArchSocket s, ArchNetAddress addr)
{
    assert(s    != NULL);
    assert(addr != NULL);

    if (bind(s->m_fd, TYPED_ADDR(struct sockaddr, addr), addr->m_len) == -1) {
        throwError(errno);
    }
}

void
ArchNetworkBSD::listenOnSocket(ArchSocket s)
{
    assert(s != NULL);

    // hardcoding backlog
    if (listen(s->m_fd, 3) == -1) {
        throwError(errno);
    }

#if HAVE_BLUETOOTH
    if(s->m_family == kBLUETOOTH) {
        struct sockaddr Addr;
        memset(&Addr, 0, sizeof(Addr));
        socklen_t size = sizeof(Addr);
        getsockname(s->m_fd, &Addr, &size);
        struct sockaddr_rc* BAddr =
            reinterpret_cast<struct sockaddr_rc*>(&Addr);
        register_service((uint8_t)BAddr->rc_channel);
    }
#endif
}

ArchSocket
ArchNetworkBSD::acceptSocket(ArchSocket s, ArchNetAddress* addr)
{
    assert(s != NULL);

    // if user passed NULL in addr then use scratch space
    ArchNetAddress dummy;
    if (addr == NULL) {
        addr = &dummy;
    }

    // create new socket and address
    ArchSocketImpl* newSocket = new ArchSocketImpl;
    *addr                      = new ArchNetAddressImpl;

    // accept on socket
    ACCEPT_TYPE_ARG3 len = (ACCEPT_TYPE_ARG3)((*addr)->m_len);
    int fd = accept(s->m_fd, TYPED_ADDR(struct sockaddr, (*addr)), &len);
    (*addr)->m_len = (socklen_t)len;
    if (fd == -1) {
        int err = errno;
        delete newSocket;
        delete *addr;
        *addr = NULL;
        if (err == EAGAIN) {
            return NULL;
        }
        throwError(err);
    }

    try {
        setBlockingOnSocket(fd, false);
    }
    catch (...) {
        close(fd);
        delete newSocket;
        delete *addr;
        *addr = NULL;
        throw;
    }

    // initialize socket
    newSocket->m_fd       = fd;
    newSocket->m_refCount = 1;

    // discard address if not requested
    if (addr == &dummy) {
        ARCH->closeAddr(dummy);
    }

    return newSocket;
}

bool
ArchNetworkBSD::connectSocket(ArchSocket s, ArchNetAddress addr)
{
    assert(s    != NULL);
    assert(addr != NULL);

#if HAVE_BLUETOOTH
    if (getAddrFamily(addr) == kBLUETOOTH)
        addr = find_service(addr);
#endif

    if (connect(s->m_fd, TYPED_ADDR(struct sockaddr, addr), addr->m_len) == -1) {
        if (errno == EISCONN) {
            return true;
        }
        if (errno == EINPROGRESS) {
            return false;
        }
        throwError(errno);
    }
    return true;
}

#if HAVE_POLL

int
ArchNetworkBSD::pollSocket(PollEntry pe[], int num, double timeout)
{
    assert(pe != NULL || num == 0);

    // return if nothing to do
    if (num == 0) {
        if (timeout > 0.0) {
            ARCH->sleep(timeout);
        }
        return 0;
    }

    // allocate space for translated query
    struct pollfd* pfd = new struct pollfd[1 + num];

    // translate query
    for (int i = 0; i < num; ++i) {
        pfd[i].fd     = (pe[i].m_socket == NULL) ? -1 : pe[i].m_socket->m_fd;
        pfd[i].events = 0;
        if ((pe[i].m_events & kPOLLIN) != 0) {
            pfd[i].events |= POLLIN;
        }
        if ((pe[i].m_events & kPOLLOUT) != 0) {
            pfd[i].events |= POLLOUT;
        }
    }
    int n = num;

    // add the unblock pipe
    const int* unblockPipe = getUnblockPipe();
    if (unblockPipe != NULL) {
        pfd[n].fd     = unblockPipe[0];
        pfd[n].events = POLLIN;
        ++n;
    }

    // prepare timeout
    int t = (timeout < 0.0) ? -1 : static_cast<int>(1000.0 * timeout);

    // do the poll
    n = poll(pfd, n, t);

    // reset the unblock pipe
    if (n > 0 && unblockPipe != NULL && (pfd[num].revents & POLLIN) != 0) {
        // the unblock event was signalled.  flush the pipe.
        char dummy[100];
        int ignore;

        do {
            ignore = read(unblockPipe[0], dummy, sizeof(dummy));
        } while (errno != EAGAIN);

        // don't count this unblock pipe in return value
        --n;
    }

    // handle results
    if (n == -1) {
        if (errno == EINTR) {
            // interrupted system call
            ARCH->testCancelThread();
            delete[] pfd;
            return 0;
        }
        delete[] pfd;
        throwError(errno);
    }

    // translate back
    for (int i = 0; i < num; ++i) {
        pe[i].m_revents = 0;
        if ((pfd[i].revents & POLLIN) != 0) {
            pe[i].m_revents |= kPOLLIN;
        }
        if ((pfd[i].revents & POLLOUT) != 0) {
            pe[i].m_revents |= kPOLLOUT;
        }
        if ((pfd[i].revents & POLLERR) != 0) {
            pe[i].m_revents |= kPOLLERR;
        }
        if ((pfd[i].revents & POLLNVAL) != 0) {
            pe[i].m_revents |= kPOLLNVAL;
        }
    }

    delete[] pfd;
    return n;
}

#else

int
ArchNetworkBSD::pollSocket(PollEntry pe[], int num, double timeout)
{
    int i, n;

    // prepare sets for select
    n = 0;
    fd_set readSet, writeSet, errSet;
    fd_set* readSetP  = NULL;
    fd_set* writeSetP = NULL;
    fd_set* errSetP   = NULL;
    FD_ZERO(&readSet);
    FD_ZERO(&writeSet);
    FD_ZERO(&errSet);
    for (i = 0; i < num; ++i) {
        // reset return flags
        pe[i].m_revents = 0;

        // set invalid flag if socket is bogus then go to next socket
        if (pe[i].m_socket == NULL) {
            pe[i].m_revents |= kPOLLNVAL;
            continue;
        }

        int fdi = pe[i].m_socket->m_fd;
        if (pe[i].m_events & kPOLLIN) {
            FD_SET(pe[i].m_socket->m_fd, &readSet);
            readSetP = &readSet;
            if (fdi > n) {
                n = fdi;
            }
        }
        if (pe[i].m_events & kPOLLOUT) {
            FD_SET(pe[i].m_socket->m_fd, &writeSet);
            writeSetP = &writeSet;
            if (fdi > n) {
                n = fdi;
            }
        }
        if (true) {
            FD_SET(pe[i].m_socket->m_fd, &errSet);
            errSetP = &errSet;
            if (fdi > n) {
                n = fdi;
            }
        }
    }

    // add the unblock pipe
    const int* unblockPipe = getUnblockPipe();
    if (unblockPipe != NULL) {
        FD_SET(unblockPipe[0], &readSet);
        readSetP = &readSet;
        if (unblockPipe[0] > n) {
            n = unblockPipe[0];
        }
    }

    // if there are no sockets then don't block forever
    if (n == 0 && timeout < 0.0) {
        timeout = 0.0;
    }

    // prepare timeout for select
    struct timeval timeout2;
    struct timeval* timeout2P;
    if (timeout < 0.0) {
        timeout2P = NULL;
    }
    else {
        timeout2P = &timeout2;
        timeout2.tv_sec  = static_cast<int>(timeout);
        timeout2.tv_usec = static_cast<int>(1.0e+6 *
                                        (timeout - timeout2.tv_sec));
    }

    // do the select
    n = select((SELECT_TYPE_ARG1)  n + 1,
                SELECT_TYPE_ARG234 readSetP,
                SELECT_TYPE_ARG234 writeSetP,
                SELECT_TYPE_ARG234 errSetP,
                SELECT_TYPE_ARG5   timeout2P);

    // reset the unblock pipe
    if (n > 0 && unblockPipe != NULL && FD_ISSET(unblockPipe[0], &readSet)) {
        // the unblock event was signalled.  flush the pipe.
        char dummy[100];
        do {
            read(unblockPipe[0], dummy, sizeof(dummy));
        } while (errno != EAGAIN);
    }

    // handle results
    if (n == -1) {
        if (errno == EINTR) {
            // interrupted system call
            ARCH->testCancelThread();
            return 0;
        }
        throwError(errno);
    }
    n = 0;
    for (i = 0; i < num; ++i) {
        if (pe[i].m_socket != NULL) {
            if (FD_ISSET(pe[i].m_socket->m_fd, &readSet)) {
                pe[i].m_revents |= kPOLLIN;
            }
            if (FD_ISSET(pe[i].m_socket->m_fd, &writeSet)) {
                pe[i].m_revents |= kPOLLOUT;
            }
            if (FD_ISSET(pe[i].m_socket->m_fd, &errSet)) {
                pe[i].m_revents |= kPOLLERR;
            }
        }
        if (pe[i].m_revents != 0) {
            ++n;
        }
    }

    return n;
}

#endif

void
ArchNetworkBSD::unblockPollSocket(ArchThread thread)
{
    const int* unblockPipe = getUnblockPipeForThread(thread);
    if (unblockPipe != NULL) {
        char dummy = 0;
        int ignore;

        ignore = write(unblockPipe[1], &dummy, 1);
    }
}

size_t
ArchNetworkBSD::readSocket(ArchSocket s, void* buf, size_t len)
{
    assert(s != NULL);

    ssize_t n = read(s->m_fd, buf, len);
    if (n == -1) {
        if (errno == EINTR || errno == EAGAIN) {
            return 0;
        }
        throwError(errno);
    }
    return n;
}

size_t
ArchNetworkBSD::writeSocket(ArchSocket s, const void* buf, size_t len)
{
    assert(s != NULL);

    ssize_t n = write(s->m_fd, buf, len);
    if (n == -1) {
        if (errno == EINTR || errno == EAGAIN) {
            return 0;
        }
        throwError(errno);
    }
    return n;
}

void
ArchNetworkBSD::throwErrorOnSocket(ArchSocket s)
{
    assert(s != NULL);

    // get the error from the socket layer
    int err        = 0;
    socklen_t size = (socklen_t)sizeof(err);
    if (getsockopt(s->m_fd, SOL_SOCKET, SO_ERROR,
                            (optval_t*)&err, &size) == -1) {
        err = errno;
    }

    // throw if there's an error
    if (err != 0) {
        throwError(err);
    }
}

void
ArchNetworkBSD::setBlockingOnSocket(int fd, bool blocking)
{
    assert(fd != -1);

    int mode = fcntl(fd, F_GETFL, 0);
    if (mode == -1) {
        throwError(errno);
    }
    if (blocking) {
        mode &= ~O_NONBLOCK;
    }
    else {
        mode |= O_NONBLOCK;
    }
    if (fcntl(fd, F_SETFL, mode) == -1) {
        throwError(errno);
    }
}

bool
ArchNetworkBSD::setNoDelayOnSocket(ArchSocket s, bool noDelay)
{
    assert(s != NULL);

    if(s->m_family == kBLUETOOTH)
        return true;

    // get old state
    int oflag;
    socklen_t size = (socklen_t)sizeof(oflag);
    if (getsockopt(s->m_fd, IPPROTO_TCP, TCP_NODELAY,
                            (optval_t*)&oflag, &size) == -1) {
        throwError(errno);
    }

    int flag = noDelay ? 1 : 0;
    size     = (socklen_t)sizeof(flag);
    if (setsockopt(s->m_fd, IPPROTO_TCP, TCP_NODELAY,
                            (optval_t*)&flag, size) == -1) {
        throwError(errno);
    }

    return (oflag != 0);
}

bool
ArchNetworkBSD::setReuseAddrOnSocket(ArchSocket s, bool reuse)
{
    assert(s != NULL);

    // get old state
    int oflag;
    socklen_t size = (socklen_t)sizeof(oflag);
    if (getsockopt(s->m_fd, SOL_SOCKET, SO_REUSEADDR,
                            (optval_t*)&oflag, &size) == -1) {
        throwError(errno);
    }

    int flag = reuse ? 1 : 0;
    size     = (socklen_t)sizeof(flag);
    if (setsockopt(s->m_fd, SOL_SOCKET, SO_REUSEADDR,
                            (optval_t*)&flag, size) == -1) {
        throwError(errno);
    }

    return (oflag != 0);
}

std::string
ArchNetworkBSD::getHostName()
{
    char name[256];
    if (gethostname(name, sizeof(name)) == -1) {
        name[0] = '\0';
    }
    else {
        name[sizeof(name) - 1] = '\0';
    }
    return name;
}

ArchNetAddress
ArchNetworkBSD::newAnyAddr(EAddressFamily family)
{
    // allocate address
    ArchNetAddressImpl* addr = new ArchNetAddressImpl;

    // fill it in
    switch (family) {
    case kINET: {
        auto* ipAddr = TYPED_ADDR(struct sockaddr_in, addr);
        ipAddr->sin_family         = AF_INET;
        ipAddr->sin_port           = 0;
        ipAddr->sin_addr.s_addr    = INADDR_ANY;
        addr->m_len                = (socklen_t)sizeof(struct sockaddr_in);
        break;
    }

    case kINET6: {
        auto* ipAddr = TYPED_ADDR(struct sockaddr_in6, addr);
        ipAddr->sin6_family         = AF_INET6;
        ipAddr->sin6_port           = 0;
        memcpy(&ipAddr->sin6_addr, &in6addr_any, sizeof(in6addr_any));
        addr->m_len                = (socklen_t)sizeof(struct sockaddr_in6);
        break;
    }

#if HAVE_BLUETOOTH
    case kBLUETOOTH: {
        struct sockaddr_rc* BAddr =
            reinterpret_cast<struct sockaddr_rc*>(&addr->m_addr);
        BAddr->rc_family         = AF_BLUETOOTH;
        BAddr->rc_bdaddr            = ((bdaddr_t) {{0, 0, 0, 0, 0, 0}});
        BAddr->rc_channel       = (uint8_t) 0;
        addr->m_len                = sizeof(struct sockaddr_rc);
        break;
    }
#endif

    default:
        delete addr;
        assert(0 && "invalid family");
    }

    return addr;
}

ArchNetAddress
ArchNetworkBSD::copyAddr(ArchNetAddress addr)
{
    assert(addr != NULL);

    // allocate and copy address
    return new ArchNetAddressImpl(*addr);
}

ArchNetAddress
ArchNetworkBSD::nameToAddr(const std::string& name)
{
    // allocate address
    ArchNetAddressImpl* addr = new ArchNetAddressImpl;

    char ipstr[INET6_ADDRSTRLEN];
    struct addrinfo hints;
    struct addrinfo *p;
    int ret;

#if HAVE_BLUETOOTH
    if (s->m_family == kBLUETOOTH) {
        struct sockaddr_rc inaddr;
        memset(&inaddr, 0, sizeof(inaddr));
        if(str2ba(name.c_str(), &inaddr.rc_bdaddr) != 0) {
            ARCH->unlockMutex(m_mutex);
            delete addr;
            throwNameError(ret);
        }

        addr->m_len = sizeof(struct sockaddr_rc);
        inaddr.rc_family = AF_BLUETOOTH;
        inaddr.rc_channel = (uint8_t) 0;
        memcpy(&addr->m_addr, &inaddr, addr->m_len);

        ARCH->unlockMutex(m_mutex);

        return addr;
    }
#endif

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;

    ARCH->lockMutex(m_mutex);
    if ((ret = getaddrinfo(name.c_str(), NULL, &hints, &p)) != 0) {
        ARCH->unlockMutex(m_mutex);
        delete addr;
        throwNameError(ret);
    }

    if (p->ai_family == AF_INET) {
        addr->m_len = (socklen_t)sizeof(struct sockaddr_in);
    } else {
        addr->m_len = (socklen_t)sizeof(struct sockaddr_in6);
    }

    memcpy(&addr->m_addr, p->ai_addr, addr->m_len);
    freeaddrinfo(p);
    ARCH->unlockMutex(m_mutex);

    return addr;
}

void
ArchNetworkBSD::closeAddr(ArchNetAddress addr)
{
    assert(addr != NULL);

    delete addr;
}

std::string
ArchNetworkBSD::addrToName(ArchNetAddress addr)
{
    assert(addr != NULL);

    switch(getAddrFamily(addr)) {
    case kINET:
    case kINET6: {
        // mutexed name lookup (ugh)
        ARCH->lockMutex(m_mutex);
        char host[1024];
        char service[20];
        int ret = getnameinfo(TYPED_ADDR(struct sockaddr, addr), addr->m_len, host,
                sizeof(host), service, sizeof(service), 0);
        if (ret != 0) {
            ARCH->unlockMutex(m_mutex);
            throwNameError(ret);
        }

        // save (primary) name
        std::string name = host;

        // done with static buffer
        ARCH->unlockMutex(m_mutex);
        return name;
    }

#if HAVE_BLUETOOTH
    case kBLUETOOTH: {
        int sock = hci_open_dev( hci_get_route( NULL ) );
        struct sockaddr_rc *baddr = reinterpret_cast<sockaddr_rc*>(&addr->m_addr);

        char cname[256] = "00:00:00:00:00:00";
        if(hci_read_remote_name(sock, &baddr->rc_bdaddr, 255, cname, 3000))
            return addrToString(addr);

        close(sock);

        std::string name = cname;
        return name;
    }
#endif

    default:
        assert(0 && "unknown address family");
        return "";
    }
}

std::string
ArchNetworkBSD::addrToString(ArchNetAddress addr)
{
    assert(addr != NULL);

    switch (getAddrFamily(addr)) {
    case kINET: {
        auto* ipAddr = TYPED_ADDR(struct sockaddr_in, addr);
        ARCH->lockMutex(m_mutex);
        std::string s = inet_ntoa(ipAddr->sin_addr);
        ARCH->unlockMutex(m_mutex);
        return s;
    }

    case kINET6: {
        char strAddr[INET6_ADDRSTRLEN];
        auto* ipAddr = TYPED_ADDR(struct sockaddr_in6, addr);
        ARCH->lockMutex(m_mutex);
        inet_ntop(AF_INET6, &ipAddr->sin6_addr, strAddr, INET6_ADDRSTRLEN);
        ARCH->unlockMutex(m_mutex);
        return strAddr;
    }

#if HAVE_BLUETOOTH
    case kBLUETOOTH: {
        struct sockaddr_rc *baddr = reinterpret_cast<sockaddr_rc*>(&addr->m_addr);
        char cstr[18] = "00:00:00:00:00:00";
        ba2str(&baddr->rc_bdaddr, cstr);
        std::string s = cstr;
        return s;
    }
#endif

    default:
        assert(0 && "unknown address family");
        return "";
    }
}

IArchNetwork::EAddressFamily
ArchNetworkBSD::getAddrFamily(ArchNetAddress addr)
{
    assert(addr != NULL);

    switch (addr->m_addr.ss_family) {
    case AF_INET:
        return kINET;
    case AF_INET6:
        return kINET6;
    case AF_BLUETOOTH:
        return kBLUETOOTH;

    default:
        return kUNKNOWN;
    }
}

void
ArchNetworkBSD::setAddrPort(ArchNetAddress addr, int port)
{
    assert(addr != NULL);

    switch (getAddrFamily(addr)) {
    case kINET: {
        auto* ipAddr = TYPED_ADDR(struct sockaddr_in, addr);
        ipAddr->sin_port = htons(port);
        break;
    }

    case kINET6: {
        auto* ipAddr = TYPED_ADDR(struct sockaddr_in6, addr);
        ipAddr->sin6_port = htons(port);
        break;
    }

#if HAVE_BLUETOOTH
    case kBLUETOOTH: {
        struct sockaddr_rc* ipAddr =
                 reinterpret_cast<struct sockaddr_rc*>(&addr->m_addr);
        ipAddr->rc_channel = (uint8_t) 0;
        break;
    }
#endif

    default:
        assert(0 && "unknown address family");
        break;
    }
}

int
ArchNetworkBSD::getAddrPort(ArchNetAddress addr)
{
    assert(addr != NULL);

    switch (getAddrFamily(addr)) {
    case kINET: {
        auto* ipAddr = TYPED_ADDR(struct sockaddr_in, addr);
        return ntohs(ipAddr->sin_port);
    }

    case kINET6: {
        auto* ipAddr = TYPED_ADDR(struct sockaddr_in6, addr);
        return ntohs(ipAddr->sin6_port);
    }

#if HAVE_BLUETOOTH
    case kBLUETOOTH: {
        struct sockaddr_rc* ipAddr =
                reinterpret_cast<struct sockaddr_rc*>(&addr->m_addr);
        return ipAddr->rc_channel;
    }
#endif

    default:
        assert(0 && "unknown address family");
        return 0;
    }
}

bool
ArchNetworkBSD::isAnyAddr(ArchNetAddress addr)
{
    assert(addr != NULL);

    switch (getAddrFamily(addr)) {
    case kINET: {
        auto* ipAddr = TYPED_ADDR(struct sockaddr_in, addr);
        return (ipAddr->sin_addr.s_addr == INADDR_ANY &&
                addr->m_len == (socklen_t)sizeof(struct sockaddr_in));
    }

    case kINET6: {
        auto* ipAddr = TYPED_ADDR(struct sockaddr_in6, addr);
        return (memcmp(&ipAddr->sin6_addr, &in6addr_any, sizeof(in6addr_any)) == 0 &&
                addr->m_len == (socklen_t)sizeof(struct sockaddr_in6));
    }

#if HAVE_BLUETOOTH
    case kBLUETOOTH: {
        struct sockaddr_rc* BAddr =
                reinterpret_cast<struct sockaddr_rc*>(&addr->m_addr);
        char *a, *b;
        int n;
        a = (char*)&BAddr->rc_bdaddr;
        b = (char*)(&bdaddr_any);
        for(n=0;n<6;n++) {
            if(a[n]!=b[n]) return false;
        }
        return (addr->m_len == sizeof(struct sockaddr_rc));
    }
#endif

    default:
        assert(0 && "unknown address family");
        return true;
    }
}

bool
ArchNetworkBSD::isEqualAddr(ArchNetAddress a, ArchNetAddress b)
{
    return (a->m_len == b->m_len &&
            memcmp(&a->m_addr, &b->m_addr, a->m_len) == 0);
}

const int*
ArchNetworkBSD::getUnblockPipe()
{
    ArchMultithreadPosix* mt = ArchMultithreadPosix::getInstance();
    ArchThread thread        = mt->newCurrentThread();
    const int* p              = getUnblockPipeForThread(thread);
    ARCH->closeThread(thread);
    return p;
}

const int*
ArchNetworkBSD::getUnblockPipeForThread(ArchThread thread)
{
    ArchMultithreadPosix* mt = ArchMultithreadPosix::getInstance();
    int* unblockPipe          = (int*)mt->getNetworkDataForThread(thread);
    if (unblockPipe == NULL) {
        unblockPipe = new int[2];
        if (pipe(unblockPipe) != -1) {
            try {
                setBlockingOnSocket(unblockPipe[0], false);
                mt->setNetworkDataForCurrentThread(unblockPipe);
            }
            catch (...) {
                delete[] unblockPipe;
                unblockPipe = NULL;
            }
        }
        else {
            delete[] unblockPipe;
            unblockPipe = NULL;
        }
    }
    return unblockPipe;
}

void
ArchNetworkBSD::throwError(int err)
{
    switch (err) {
    case EINTR:
        ARCH->testCancelThread();
        throw XArchNetworkInterrupted(new XArchEvalUnix(err));

    case EACCES:
    case EPERM:
        throw XArchNetworkAccess(new XArchEvalUnix(err));

    case ENFILE:
    case EMFILE:
    case ENODEV:
    case ENOBUFS:
    case ENOMEM:
    case ENETDOWN:
#if defined(ENOSR)
    case ENOSR:
#endif
        throw XArchNetworkResource(new XArchEvalUnix(err));

    case EPROTOTYPE:
    case EPROTONOSUPPORT:
    case EAFNOSUPPORT:
    case EPFNOSUPPORT:
    case ESOCKTNOSUPPORT:
    case EINVAL:
    case ENOPROTOOPT:
    case EOPNOTSUPP:
    case ESHUTDOWN:
#if defined(ENOPKG)
    case ENOPKG:
#endif
        throw XArchNetworkSupport(new XArchEvalUnix(err));

    case EIO:
        throw XArchNetworkIO(new XArchEvalUnix(err));

    case EADDRNOTAVAIL:
        throw XArchNetworkNoAddress(new XArchEvalUnix(err));

    case EADDRINUSE:
        throw XArchNetworkAddressInUse(new XArchEvalUnix(err));

    case EHOSTUNREACH:
    case ENETUNREACH:
        throw XArchNetworkNoRoute(new XArchEvalUnix(err));

    case ENOTCONN:
        throw XArchNetworkNotConnected(new XArchEvalUnix(err));

    case EPIPE:
        throw XArchNetworkShutdown(new XArchEvalUnix(err));

    case ECONNABORTED:
    case ECONNRESET:
        throw XArchNetworkDisconnected(new XArchEvalUnix(err));

    case ECONNREFUSED:
        throw XArchNetworkConnectionRefused(new XArchEvalUnix(err));

    case EHOSTDOWN:
    case ETIMEDOUT:
        throw XArchNetworkTimedOut(new XArchEvalUnix(err));

    default:
        throw XArchNetwork(new XArchEvalUnix(err));
    }
}

void
ArchNetworkBSD::throwNameError(int err)
{
    static const char* s_msg[] = {
        "The specified host is unknown",
        "The requested name is valid but does not have an IP address",
        "A non-recoverable name server error occurred",
        "A temporary error occurred on an authoritative name server",
        "An unknown name server error occurred"
    };

    switch (err) {
    case HOST_NOT_FOUND:
        throw XArchNetworkNameUnknown(s_msg[0]);

    case NO_DATA:
        throw XArchNetworkNameNoAddress(s_msg[1]);

    case NO_RECOVERY:
        throw XArchNetworkNameFailure(s_msg[2]);

    case TRY_AGAIN:
        throw XArchNetworkNameUnavailable(s_msg[3]);

    default:
        throw XArchNetworkName(s_msg[4]);
    }
}
