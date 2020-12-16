/*
 * barrier -- mouse and keyboard sharing utility
 * Copyright (C) 2014-2016 Symless Ltd.
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

#include "ZeroconfServer.h"
#include "ZeroconfRecord.h"

#include <QtCore/QObject>

typedef int32_t  DNSServiceErrorType;

class ZeroconfRegister;
class ZeroconfBrowser;
class MainWindow;

class ZeroconfService : public QObject
{
    Q_OBJECT

public:
    ZeroconfService(MainWindow* mainWindow);
    ~ZeroconfService();

private slots:
    void serverDetected(const QList<ZeroconfRecord>& list);
    void clientDetected(const QList<ZeroconfRecord>& list);
    void errorHandle(DNSServiceErrorType errorCode);

private:
    bool registerService(bool server);

private:
    MainWindow* m_pMainWindow;
    ZeroconfServer m_zeroconfServer;
    ZeroconfBrowser* m_pZeroconfBrowser;
    ZeroconfRegister* m_pZeroconfRegister;
    bool m_ServiceRegistered;

    static const char* m_ServerServiceName;
    static const char* m_ClientServiceName;
};
