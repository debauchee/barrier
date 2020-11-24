/*
 * barrier -- mouse and keyboard sharing utility
 * Copyright (C) 2015-2016 Symless Ltd.
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

#include "server/ClientProxy1_6.h"

class Server;
class IEventQueue;

//! Proxy for client implementing protocol version 1.7
class ClientProxy1_7 : public ClientProxy1_6 {
public:
    ClientProxy1_7(const String& name, barrier::IStream* adoptedStream, Server* server, IEventQueue* events);
    ~ClientProxy1_7();

    virtual void        enter(SInt32 xAbs, SInt32 yAbs,
                            UInt32 seqNum, KeyModifierMask mask,
                            bool forScreensaver);

protected:
    virtual bool        parseMessage(const UInt8* code);

    virtual void        rcvScreensaver();

private:
    IEventQueue*        m_events;
};
