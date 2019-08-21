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

#if !defined(_WIN32)

#include "base/StdinListen.h"
#include "../gui/src/ShutdownCh.h"

#include <unistd.h> // tcgetattr/tcsetattr, read
#include <termios.h> // tcgetattr/tcsetattr

StdinListen::StdinListen(IEventQueue* events) :
    m_events(events),
    m_thread(NULL)
{
    // disable ICANON & ECHO so we don't have to wait for a newline
    // before we get data (and to keep it from being echoed back out)
    termios ta;
    tcgetattr(STDIN_FILENO, &ta);
    _p_ta_previous = new termios(ta);
    ta.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &ta);

    m_thread = new Thread(new TMethodJob<StdinListen>(
                                  this, &StdinListen::stdinThread));
}

StdinListen::~StdinListen()
{
    tcsetattr(STDIN_FILENO, TCSANOW, _p_ta_previous);
    delete _p_ta_previous;
}

void
StdinListen::stdinThread(void *)
{
    char ch;

    while (1) {
        if (read(STDIN_FILENO, &ch, 1) == 1) {
            if (ch == ShutdownCh) {
                m_events->addEvent(Event(Event::kQuit));
                break;
            }
        }
    }
}

#endif // !defined(_WIN32)