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

#if !defined(QBARRIERAPPLICATION__H)

#define QBARRIERAPPLICATION__H

#include <QApplication>

class QSessionManager;

class QBarrierApplication : public QApplication
{
    public:
        QBarrierApplication(int& argc, char** argv);
        ~QBarrierApplication();

    public:
        void commitData(QSessionManager& manager);
        void switchTranslator(QString lang);
        void setTranslator(QTranslator* translator);

        static QBarrierApplication* getInstance();

    private Q_SLOTS:
        void onLastWindowClosed();

    private:
        QTranslator* m_Translator;

        static QBarrierApplication* s_Instance;
};

#endif

