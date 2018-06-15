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

#include "ActionDialog.h"

#include "Hotkey.h"
#include "Action.h"
#include "ServerConfig.h"
#include "KeySequence.h"

#include <QtCore>
#include <QtGui>
#include <QButtonGroup>

ActionDialog::ActionDialog(QWidget* parent, ServerConfig& config, Hotkey& hotkey, Action& action) :
    QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint),
    Ui::ActionDialogBase(),
    m_ServerConfig(config),
    m_Hotkey(hotkey),
    m_Action(action),
    m_pButtonGroupType(new QButtonGroup(this))
{
    setupUi(this);

    // work around Qt Designer's lack of a QButtonGroup; we need it to get
    // at the button id of the checked radio button
    QRadioButton* const typeButtons[] = { m_pRadioPress, m_pRadioRelease, m_pRadioPressAndRelease, m_pRadioSwitchToScreen, m_pRadioSwitchInDirection, m_pRadioLockCursorToScreen };

    for (unsigned int i = 0; i < sizeof(typeButtons) / sizeof(typeButtons[0]); i++)
        m_pButtonGroupType->addButton(typeButtons[i], i);

    m_pKeySequenceWidgetHotkey->setText(m_Action.keySequence().toString());
    m_pKeySequenceWidgetHotkey->setKeySequence(m_Action.keySequence());
    m_pButtonGroupType->button(m_Action.type())->setChecked(true);
    m_pComboSwitchInDirection->setCurrentIndex(m_Action.switchDirection());
    m_pComboLockCursorToScreen->setCurrentIndex(m_Action.lockCursorMode());

    if (m_Action.activeOnRelease())
        m_pRadioHotkeyReleased->setChecked(true);
    else
        m_pRadioHotkeyPressed->setChecked(true);

    m_pGroupBoxScreens->setChecked(m_Action.haveScreens());

    int idx = 0;
    foreach(const Screen& screen, serverConfig().screens())
        if (!screen.isNull())
        {
            QListWidgetItem *pListItem = new QListWidgetItem(screen.name());
            m_pListScreens->addItem(pListItem);
            if (m_Action.typeScreenNames().indexOf(screen.name()) != -1)
                m_pListScreens->setCurrentItem(pListItem);

            m_pComboSwitchToScreen->addItem(screen.name());
            if (screen.name() == m_Action.switchScreenName())
                m_pComboSwitchToScreen->setCurrentIndex(idx);

            idx++;
        }
}

void ActionDialog::accept()
{
    if (!sequenceWidget()->valid() && m_pButtonGroupType->checkedId() >= 0 && m_pButtonGroupType->checkedId() < 3)
        return;

    m_Action.setKeySequence(sequenceWidget()->keySequence());
    m_Action.setType(m_pButtonGroupType->checkedId());
    m_Action.setHaveScreens(m_pGroupBoxScreens->isChecked());

    m_Action.typeScreenNames().clear();
    foreach(const QListWidgetItem* pItem, m_pListScreens->selectedItems())
        m_Action.typeScreenNames().append(pItem->text());

    m_Action.setSwitchScreenName(m_pComboSwitchToScreen->currentText());
    m_Action.setSwitchDirection(m_pComboSwitchInDirection->currentIndex());
    m_Action.setLockCursorMode(m_pComboLockCursorToScreen->currentIndex());
    m_Action.setActiveOnRelease(m_pRadioHotkeyReleased->isChecked());

    QDialog::accept();
}

void ActionDialog::on_m_pKeySequenceWidgetHotkey_keySequenceChanged()
{
    if (sequenceWidget()->keySequence().isMouseButton())
    {
        m_pGroupBoxScreens->setEnabled(false);
        m_pListScreens->setEnabled(false);
    }
    else
    {
        m_pGroupBoxScreens->setEnabled(true);
        m_pListScreens->setEnabled(true);
    }
}
