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

#include "server/ClientProxy1_7.h"

#include "server/Server.h"
#include "barrier/ProtocolUtil.h"
#include "barrier/StreamChunker.h"
#include "barrier/ClipboardChunk.h"
#include "io/IStream.h"
#include "base/TMethodEventJob.h"
#include "base/Log.h"

//
// ClientProxy1_7
//

ClientProxy1_7::ClientProxy1_7(const String& name, barrier::IStream* stream, Server* server, IEventQueue* events) :
    ClientProxy1_6(name, stream, server, events),
    m_events(events)
{
}

ClientProxy1_7::~ClientProxy1_7()
{
}

void
ClientProxy1_7::enter(SInt32 xAbs, SInt32 yAbs,
                UInt32 seqNum, KeyModifierMask mask, bool forScreensaver)
{
    LOG((CLOG_DEBUG1 "send enter to \"%s\", %d,%d %d %04x, forScreensaver=%d", getName().c_str(), xAbs, yAbs, seqNum, mask, forScreensaver ? 1 : 0));
    ProtocolUtil::writef(getStream(), kMsgCEnter,
                                xAbs, yAbs, seqNum, mask, forScreensaver ? 1 : 0);
}

bool
ClientProxy1_7::parseMessage(const UInt8* code)
{
    if (memcmp(code, kMsgCLocalInput, 4) == 0) {
        SInt16 x, y;
        ProtocolUtil::readf(getStream(), kMsgCLocalInput + 4, &x, &y);

        // Set up container for x and y coordinates
        IPlatformScreen::MotionInfo* info = IPlatformScreen::MotionInfo::alloc(x, y);

        m_events->addEvent(Event(m_events->forIScreen().localInput(), getEventTarget(), info));
    }
    else if (memcmp(code, kMsgCScreenSaver, 4) == 0) {
        rcvScreensaver();
    }
    else {
        return ClientProxy1_6::parseMessage(code);
    }

    return true;
}

void
ClientProxy1_7::rcvScreensaver()
{
    SInt8 activate;
    ProtocolUtil::readf(getStream(), kMsgCScreenSaver + 4, &activate);
    if (activate != 0) {
        m_events->addEvent(Event(m_events->forIPrimaryScreen().screensaverActivated(), getEventTarget()));
    } else {
        m_events->addEvent(Event(m_events->forIPrimaryScreen().screensaverDeactivated(), getEventTarget()));
    }
}
