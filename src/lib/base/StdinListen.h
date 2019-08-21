/*
 * barrier -- mouse and keyboard sharing utility
 * Copyright (C) 2008 Debauchee Open Source Group
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

#if defined(_WIN32)

class StdinListen
{
public:
    void           stdinThread(void*) {};
};

#else

#include "base/IEventQueue.h"
#include "base/TMethodJob.h"
#include "mt/Thread.h"

struct termios;

class StdinListen
{
public:
    explicit StdinListen(IEventQueue* events);
    ~StdinListen();

private:
    void           stdinThread(void*);

    IEventQueue*   m_events;
    Thread*        m_thread;
    termios*       _p_ta_previous;
};

#endif