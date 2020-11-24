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

#include "server/PrimaryClient.h"

#include "barrier/Screen.h"
#include "barrier/Clipboard.h"
#include "base/Log.h"

//
// PrimaryClient
//

PrimaryClient::PrimaryClient(const std::string& name, barrier::Screen* screen) :
    BaseClientProxy(name),
    m_screen(screen),
    m_fakeInputCount(0)
{
    // all clipboards are clean
    for (UInt32 i = 0; i < kClipboardEnd; ++i) {
        m_clipboardDirty[i] = false;
    }
}

PrimaryClient::~PrimaryClient()
{
    // do nothing
}

void
PrimaryClient::reconfigure(UInt32 activeSides)
{
    m_screen->reconfigure(activeSides);
}

UInt32
PrimaryClient::registerHotKey(KeyID key, KeyModifierMask mask)
{
    return m_screen->registerHotKey(key, mask);
}

void
PrimaryClient::unregisterHotKey(UInt32 id)
{
    m_screen->unregisterHotKey(id);
}

void
PrimaryClient::fakeInputBegin()
{
    if (++m_fakeInputCount == 1) {
        m_screen->fakeInputBegin();
    }
}

void
PrimaryClient::fakeInputEnd()
{
    if (--m_fakeInputCount == 0) {
        m_screen->fakeInputEnd();
    }
}

SInt32
PrimaryClient::getJumpZoneSize() const
{
    return m_screen->getJumpZoneSize();
}

void
PrimaryClient::getCursorCenter(SInt32& x, SInt32& y) const
{
    m_screen->getCursorCenter(x, y);
}

KeyModifierMask
PrimaryClient::getToggleMask() const
{
    return m_screen->pollActiveModifiers();
}

bool
PrimaryClient::isLockedToScreen() const
{
    return m_screen->isLockedToScreen();
}

void*
PrimaryClient::getEventTarget() const
{
    return m_screen->getEventTarget();
}

bool
PrimaryClient::getClipboard(ClipboardID id, IClipboard* clipboard) const
{
    return m_screen->getClipboard(id, clipboard);
}

void
PrimaryClient::getShape(SInt32& x, SInt32& y,
                SInt32& width, SInt32& height) const
{
    m_screen->getShape(x, y, width, height);
}

void
PrimaryClient::getCursorPos(SInt32& x, SInt32& y) const
{
    m_screen->getCursorPos(x, y);
}

void
PrimaryClient::enable()
{
    m_screen->enable();
}

void
PrimaryClient::disable()
{
    m_screen->disable();
}

void
PrimaryClient::enter(SInt32 xAbs, SInt32 yAbs,
                UInt32 seqNum, KeyModifierMask mask, bool forScreensaver)
{
    m_screen->setSequenceNumber(seqNum);
    if (!forScreensaver) {
        m_screen->warpCursor(xAbs, yAbs);
    }
    m_screen->enter(mask);
}

bool
PrimaryClient::leave()
{
    return m_screen->leave();
}

void
PrimaryClient::setClipboard(ClipboardID id, const IClipboard* clipboard)
{
    // ignore if this clipboard is already clean
    if (m_clipboardDirty[id]) {
        // this clipboard is now clean
        m_clipboardDirty[id] = false;

        // set clipboard
        m_screen->setClipboard(id, clipboard);
    }
}

void
PrimaryClient::grabClipboard(ClipboardID id)
{
    // grab clipboard
    m_screen->grabClipboard(id);

    // clipboard is dirty (because someone else owns it now)
    m_clipboardDirty[id] = true;
}

void
PrimaryClient::setClipboardDirty(ClipboardID id, bool dirty)
{
    m_clipboardDirty[id] = dirty;
}

void
PrimaryClient::keyDown(KeyID key, KeyModifierMask mask, KeyButton button)
{
    if (m_fakeInputCount > 0) {
// XXX -- don't forward keystrokes to primary screen for now
        (void)key;
        (void)mask;
        (void)button;
//        m_screen->keyDown(key, mask, button);
    }
}

void
PrimaryClient::keyRepeat(KeyID, KeyModifierMask, SInt32, KeyButton)
{
    // ignore
}

void
PrimaryClient::keyUp(KeyID key, KeyModifierMask mask, KeyButton button)
{
    if (m_fakeInputCount > 0) {
// XXX -- don't forward keystrokes to primary screen for now
        (void)key;
        (void)mask;
        (void)button;
//        m_screen->keyUp(key, mask, button);
    }
}

void
PrimaryClient::mouseDown(ButtonID)
{
    // ignore
}

void
PrimaryClient::mouseUp(ButtonID)
{
    // ignore
}

void
PrimaryClient::mouseMove(SInt32 x, SInt32 y)
{
    m_screen->warpCursor(x, y);
}

void
PrimaryClient::mouseRelativeMove(SInt32, SInt32)
{
    // ignore
}

void
PrimaryClient::mouseWheel(SInt32, SInt32)
{
    // ignore
}

void
PrimaryClient::screensaver(bool activate)
{
    m_screen->screensaver(activate);
}

void
PrimaryClient::sendDragInfo(UInt32 fileCount, const char* info, size_t size)
{
    // ignore
}

void
PrimaryClient::fileChunkSending(UInt8 mark, char* data, size_t dataSize)
{
    // ignore
}

void
PrimaryClient::resetOptions()
{
    m_screen->resetOptions();
}

void
PrimaryClient::setOptions(const OptionsList& options)
{
    m_screen->setOptions(options);
}
