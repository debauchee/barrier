/*
 * barrier -- mouse and keyboard sharing utility
 * Copyright (C) 2012-2016 Symless Ltd.
 * Copyright (C) 2004 Chris Schoeneman
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

#include "barrier/IKeyState.h"
#include "base/EventQueue.h"

#include <cstring>
#include <cstdlib>

//
// IKeyState
//

IKeyState::IKeyState(IEventQueue* events)
{
}

//
// IKeyState::KeyInfo
//

IKeyState::KeyInfo*
IKeyState::KeyInfo::alloc(KeyID id,
                KeyModifierMask mask, KeyButton button, SInt32 count)
{
    KeyInfo* info           = (KeyInfo*)malloc(sizeof(KeyInfo));
    info->m_key              = id;
    info->m_mask             = mask;
    info->m_button           = button;
    info->m_count            = count;
    info->m_screens          = NULL;
    info->m_screensBuffer[0] = '\0';
    return info;
}

IKeyState::KeyInfo*
IKeyState::KeyInfo::alloc(KeyID id,
                KeyModifierMask mask, KeyButton button, SInt32 count,
                const std::set<String>& destinations)
{
    String screens = join(destinations);

    // build structure
    KeyInfo* info  = (KeyInfo*)malloc(sizeof(KeyInfo) + screens.size());
    info->m_key     = id;
    info->m_mask    = mask;
    info->m_button  = button;
    info->m_count   = count;
    info->m_screens = info->m_screensBuffer;
    strcpy(info->m_screensBuffer, screens.c_str()); // Compliant: String type is safe
    return info;
}

IKeyState::KeyInfo*
IKeyState::KeyInfo::alloc(const KeyInfo& x)
{
    auto bufferLen  = strnlen(x.m_screensBuffer, SIZE_MAX);
    auto info       = (KeyInfo*)malloc(sizeof(KeyInfo) + bufferLen);
    info->m_key     = x.m_key;
    info->m_mask    = x.m_mask;
    info->m_button  = x.m_button;
    info->m_count   = x.m_count;
    info->m_screens = x.m_screens ? info->m_screensBuffer : NULL;
    memcpy(info->m_screensBuffer, x.m_screensBuffer, bufferLen + 1);
    return info;
}

bool
IKeyState::KeyInfo::isDefault(const char* screens)
{
    return (screens == NULL || screens[0] == '\0');
}

bool
IKeyState::KeyInfo::contains(const char* screens, const String& name)
{
    // special cases
    if (isDefault(screens)) {
        return false;
    }
    if (screens[0] == '*') {
        return true;
    }

    // search
    String match;
    match.reserve(name.size() + 2);
    match += ":";
    match += name;
    match += ":";
    return (strstr(screens, match.c_str()) != NULL);
}

bool
IKeyState::KeyInfo::equal(const KeyInfo* a, const KeyInfo* b)
{
    return (a->m_key    == b->m_key &&
            a->m_mask   == b->m_mask &&
            a->m_button == b->m_button &&
            a->m_count  == b->m_count &&
            strcmp(a->m_screensBuffer, b->m_screensBuffer) == 0);
}

String
IKeyState::KeyInfo::join(const std::set<String>& destinations)
{
    // collect destinations into a string.  names are surrounded by ':'
    // which makes searching easy.  the string is empty if there are no
    // destinations and "*" means all destinations.
    String screens;
    for (std::set<String>::const_iterator i = destinations.begin();
                                i != destinations.end(); ++i) {
        if (*i == "*") {
            screens = "*";
            break;
        }
        else {
            if (screens.empty()) {
                screens = ":";
            }
            screens += *i;
            screens += ":";
        }
    }
    return screens;
}

void
IKeyState::KeyInfo::split(const char* screens, std::set<String>& dst)
{
    dst.clear();
    if (isDefault(screens)) {
        return;
    }
    if (screens[0] == '*') {
        dst.insert("*");
        return;
    }

    const char* i = screens + 1;
    while (*i != '\0') {
        const char* j = strchr(i, ':');
        dst.insert(String(i, j - i));
        i = j + 1;
    }
}
