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

#include "Action.h"

#include <QSettings>
#include <QTextStream>

const char* Action::m_ActionTypeNames[] =
{
    "keyDown", "keyUp", "keystroke",
    "switchToScreen", "toggleScreen",
    "switchInDirection", "lockCursorToScreen",
    "userScript",
    "mouseDown", "mouseUp", "mousebutton",
};

const char* Action::m_SwitchDirectionNames[] = { "left", "right", "up", "down" };
const char* Action::m_LockCursorModeNames[] = { "toggle", "on", "off" };

Action::Action() :
    m_KeySequence(),
    m_Type(keystroke),
    m_TypeScreenNames(),
    m_SwitchScreenName(),
    m_UserScriptCommand(),
    m_SwitchDirection(switchLeft),
    m_LockCursorMode(lockCursorToggle),
    m_ActiveOnRelease(false),
    m_HasScreens(false)
{
}

QString Action::text() const
{
    /* This function is used to save to config file which is for barriers to
     * read. However the server config parse does not support functions with ()
     * in the end but now argument inside. If you need a function with no
     * argument, it can not have () in the end.
     */
    QString text = QString(m_ActionTypeNames[m_KeySequence.isMouseButton() ?
                                             type() + int(mouseDown) : type()]);

    switch (type())
    {
        case keyDown:
        case keyUp:
        case keystroke:
            {
                text += "(";
                text += m_KeySequence.toString();

                if (!m_KeySequence.isMouseButton())
                {
                    const QStringList& screens = typeScreenNames();
                    if (haveScreens() && !screens.isEmpty())
                    {
                        text += ",";

                        for (int i = 0; i < screens.size(); i++)
                        {
                            text += screens[i];
                            if (i != screens.size() - 1)
                                text += ":";
                        }
                    }
                    else
                        text += ",*";
                }
                text += ")";
            }
            break;

        case switchToScreen:
            text += "(";
            text += switchScreenName();
            text += ")";
            break;

        case toggleScreen:
            break;

        case switchInDirection:
            text += "(";
            text += m_SwitchDirectionNames[m_SwitchDirection];
            text += ")";
            break;

        case lockCursorToScreen:
            text += "(";
            text += m_LockCursorModeNames[m_LockCursorMode];
            text += ")";
            break;

        case userScript:
            text += "(";
            text += userScriptCommand();
            text += ")";
            break;
        default:
            Q_ASSERT(0);
            break;
    }


    return text;
}

void Action::loadSettings(QSettings& settings)
{
    m_KeySequence.loadSettings(settings);
    setType(settings.value("type", keyDown).toInt());

    m_TypeScreenNames.clear();
    int numTypeScreens = settings.beginReadArray("typeScreenNames");
    for (int i = 0; i < numTypeScreens; i++)
    {
        settings.setArrayIndex(i);
        m_TypeScreenNames.append(settings.value("typeScreenName").toString());
    }
    settings.endArray();

    setSwitchScreenName(settings.value("switchScreenName").toString());
    setSwitchDirection(settings.value("switchInDirection", switchLeft).toInt());
    setLockCursorMode(settings.value("lockCursorToScreen", lockCursorToggle).toInt());
    setActiveOnRelease(settings.value("activeOnRelease", false).toBool());
    setHaveScreens(settings.value("hasScreens", false).toBool());
    setUserScriptCommand(settings.value("userScriptCommand").toString());
}

void Action::saveSettings(QSettings& settings) const
{
    m_KeySequence.saveSettings(settings);
    settings.setValue("type", type());

    settings.beginWriteArray("typeScreenNames");
    for (int i = 0; i < typeScreenNames().size(); i++)
    {
        settings.setArrayIndex(i);
        settings.setValue("typeScreenName", typeScreenNames()[i]);
    }
    settings.endArray();

    settings.setValue("switchScreenName", switchScreenName());
    settings.setValue("switchInDirection", switchDirection());
    settings.setValue("lockCursorToScreen", lockCursorMode());
    settings.setValue("activeOnRelease", activeOnRelease());
    settings.setValue("hasScreens", haveScreens());
    settings.setValue("userScriptCommand", userScriptCommand());
}

QTextStream& operator<<(QTextStream& outStream, const Action& action)
{
    if (action.activeOnRelease())
        outStream << ";";

    outStream << action.text();

    return outStream;
}

