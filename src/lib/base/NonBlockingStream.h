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

// windows doesn't have a unistd.h so this class won't work as-written.
// at the moment barrier doesn't need this functionality on windows so
// it's left as a stub to be optimized out
#if defined(_WIN32)

class NonBlockingStream
{
public:
    bool try_read_char(char &ch) const { return false; };
};

#else // non-windows platforms

struct termios;

class NonBlockingStream
{
public:
    explicit NonBlockingStream(int fd = 0);
    ~NonBlockingStream();

    bool try_read_char(char &ch) const;
	void restoreTerminal(void) const;

private:
    int _fd;
    termios * _p_ta_previous;
    int _cntl_previous;
};

#endif
