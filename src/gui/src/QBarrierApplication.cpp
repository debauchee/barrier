/*
 * barrier -- mouse and keyboard sharing utility
 * Copyright (C) 2012-2016 Symless Ltd.
 * Copyright (C) 2008 Volker Lanz (vl@fidra.de)
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

#include "QBarrierApplication.h"
#include "MainWindow.h"

#include <QtCore>
#include <QtGui>

QBarrierApplication* QBarrierApplication::s_Instance = NULL;

QBarrierApplication::QBarrierApplication(int& argc, char** argv) :
    QApplication(argc, argv),
    m_Translator(NULL)
{
    s_Instance = this;

    // By default do not quit when the last window is closed as we minimise
    // to the system tray, but listen for the lastWindow closing so that
    // if the system tray is not available we can quit.
    setQuitOnLastWindowClosed(false);
    connect(this, &QApplication::lastWindowClosed, this, &QBarrierApplication::onLastWindowClosed);
}

QBarrierApplication::~QBarrierApplication()
{
    delete m_Translator;
}

void QBarrierApplication::commitData(QSessionManager&)
{
    for (QWidget* widget : topLevelWidgets()) {
        MainWindow* mainWindow = qobject_cast<MainWindow*>(widget);
        if (mainWindow)
            mainWindow->saveSettings();
    }
}

QBarrierApplication* QBarrierApplication::getInstance()
{
    return s_Instance;
}

void QBarrierApplication::switchTranslator(QString lang)
{
    if (m_Translator != NULL)
    {
        removeTranslator(m_Translator);
        delete m_Translator;
    }

    QResource locale(":/res/lang/gui_" + lang + ".qm");
    m_Translator = new QTranslator();
    m_Translator->load(locale.data(), locale.size());
    installTranslator(m_Translator);
}

void QBarrierApplication::setTranslator(QTranslator* translator)
{
    m_Translator = translator;
    installTranslator(m_Translator);
}

void QBarrierApplication::onLastWindowClosed()
{
    // If there is no system tray available then quit when the last window is closed
    if (!QSystemTrayIcon::isSystemTrayAvailable())
    {
        quit();
    }
}
