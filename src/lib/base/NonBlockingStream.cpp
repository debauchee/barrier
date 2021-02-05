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

#include "base/NonBlockingStream.h"

#include <unistd.h> // tcgetattr/tcsetattr, read
#include <termios.h> // tcgetattr/tcsetattr
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

NonBlockingStream::NonBlockingStream(int fd) :
    _fd(fd)
{
    // disable ICANON & ECHO so we don't have to wait for a newline
    // before we get data (and to keep it from being echoed back out)
    termios ta;
    tcgetattr(fd, &ta);
    _p_ta_previous = new termios(ta);
    ta.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(fd, TCSANOW, &ta);

    // prevent IO from blocking so we can poll (read())
    int _cntl_previous = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, _cntl_previous | O_NONBLOCK);
}

NonBlockingStream::~NonBlockingStream()
{
    tcsetattr(_fd, TCSANOW, _p_ta_previous);
    fcntl(_fd, F_SETFL, _cntl_previous);
    delete _p_ta_previous;
}

bool NonBlockingStream::try_read_char(char &ch) const
{
    int result = read(_fd, &ch, 1);
    if (result == 1)
        return true;
    assert(result == -1 && (errno == EAGAIN || errno == EWOULDBLOCK));
    return false;
}

void NonBlockingStream::restoreTerminal() const
{
    tcsetattr(_fd, TCSANOW, _p_ta_previous);
    fcntl(_fd, F_SETFL, _cntl_previous);
}

#endif // !defined(_WIN32)
