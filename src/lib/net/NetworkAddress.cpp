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

#include "net/NetworkAddress.h"

#include "net/XSocket.h"
#include "arch/Arch.h"
#include "arch/XArch.h"

#include <cstdlib>

//
// NetworkAddress
//

// name re-resolution adapted from a patch by Brent Priddy.

NetworkAddress::NetworkAddress() :
    m_address(NULL),
    m_hostname(),
    m_port(0)
{
    // note -- make no calls to Network socket interface here;
    // we're often called prior to Network::init().
}

NetworkAddress::NetworkAddress(int port) :
    m_address(NULL),
    m_hostname(),
    m_port(port)
{
    checkPort();
    m_address = ARCH->newAnyAddr(IArchNetwork::kBLUETOOTH);
    ARCH->setAddrPort(m_address, m_port);
}

NetworkAddress::NetworkAddress(const NetworkAddress& addr) :
    m_address(addr.m_address != NULL ? ARCH->copyAddr(addr.m_address) : NULL),
    m_hostname(addr.m_hostname),
    m_port(addr.m_port)
{
    // do nothing
}

NetworkAddress::NetworkAddress(const String& hostname, int port) :
    m_address(NULL),
    m_hostname(hostname),
    m_port(port)
{
    // check for port suffix
    String::size_type i = m_hostname.rfind(':');
    if (i != String::npos && i + 1 < m_hostname.size()) {
        // found a colon.  see if it looks like an IPv6 address.
        bool colonNotation = false;
        bool dotNotation   = false;
        bool doubleColon   = false;
        for (String::size_type j = 0; j < i; ++j) {
            if (m_hostname[j] == ':') {
                colonNotation = true;
                dotNotation   = false;
                if (m_hostname[j + 1] == ':') {
                    doubleColon = true;
                }
            }
            else if (m_hostname[j] == '.' && colonNotation) {
                dotNotation = true;
            }
        }

        // port suffix is ambiguous with IPv6 notation if there's
        // a double colon and the end of the address is not in dot
        // notation.  in that case we assume it's not a port suffix.
        // the user can replace the double colon with zeros to
        // disambiguate.
        if ((!doubleColon || dotNotation) && !colonNotation) {
            // parse port from hostname
            char* end;
            const char* chostname = m_hostname.c_str();
            long suffixPort = strtol(chostname + i + 1, &end, 10);
            if (end == chostname + i + 1 || *end != '\0') {
                throw XSocketAddress(XSocketAddress::kBadPort,
                                            m_hostname, m_port);
            }

            // trim port from hostname
            m_hostname.erase(i);

            // save port
            m_port = static_cast<int>(suffixPort);
        }
    }

    // check port number
    checkPort();
}

NetworkAddress::~NetworkAddress()
{
    if (m_address != NULL) {
        ARCH->closeAddr(m_address);
    }
}

NetworkAddress&
NetworkAddress::operator=(const NetworkAddress& addr)
{
    ArchNetAddress newAddr = NULL;
    if (addr.m_address != NULL) {
        newAddr = ARCH->copyAddr(addr.m_address);
    }
    if (m_address != NULL) {
        ARCH->closeAddr(m_address);
    }
    m_address  = newAddr;
    m_hostname = addr.m_hostname;
    m_port     = addr.m_port;
    return *this;
}

void
NetworkAddress::resolve()
{
    // discard previous address
    if (m_address != NULL) {
        ARCH->closeAddr(m_address);
        m_address = NULL;
    }

    try {
        // if hostname is empty then use wildcard address otherwise look
        // up the name.
        if (m_hostname.empty()) {
            m_address = ARCH->newAnyAddr(IArchNetwork::kBLUETOOTH);
        }
        else {
            m_address = ARCH->nameToAddr(m_hostname);
        }
    }
    catch (XArchNetworkNameUnknown&) {
        throw XSocketAddress(XSocketAddress::kNotFound, m_hostname, m_port);
    }
    catch (XArchNetworkNameNoAddress&) {
        throw XSocketAddress(XSocketAddress::kNoAddress, m_hostname, m_port);
    }
    catch (XArchNetworkNameUnsupported&) {
        throw XSocketAddress(XSocketAddress::kUnsupported, m_hostname, m_port);
    }
    catch (XArchNetworkName&) {
        throw XSocketAddress(XSocketAddress::kUnknown, m_hostname, m_port);
    }

    // set port in address
    ARCH->setAddrPort(m_address, m_port);
}

bool
NetworkAddress::operator==(const NetworkAddress& addr) const
{
    return ARCH->isEqualAddr(m_address, addr.m_address);
}

bool
NetworkAddress::operator!=(const NetworkAddress& addr) const
{
    return !operator==(addr);
}

bool
NetworkAddress::isValid() const
{
    return (m_address != NULL);
}

const ArchNetAddress&
NetworkAddress::getAddress() const
{
    return m_address;
}

int
NetworkAddress::getPort() const
{
    return m_port;
}

String
NetworkAddress::getHostname() const
{
    return m_hostname;
}

void
NetworkAddress::checkPort()
{
    // check port number
    if (m_port <= 0 || m_port > 65535) {
        throw XSocketAddress(XSocketAddress::kBadPort, m_hostname, m_port);
    }
}
