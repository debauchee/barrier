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

#include "SettingsDialog.h"

#include "BarrierLocale.h"
#include "QBarrierApplication.h"
#include "QUtility.h"
#include "AppConfig.h"
#include "SslCertificate.h"
#include "MainWindow.h"

#include <QtCore>
#include <QtGui>
#include <QMessageBox>
#include <QFileDialog>
#include <QDir>

SettingsDialog::SettingsDialog(QWidget* parent, AppConfig& config) :
    QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint),
    Ui::SettingsDialogBase(),
       m_appConfig(config)
{
    setupUi(this);

    m_Locale.fillLanguageComboBox(m_pComboLanguage);

    m_pLineEditScreenName->setText(appConfig().screenName());
    m_pSpinBoxPort->setValue(appConfig().port());
    m_pLineEditInterface->setText(appConfig().networkInterface());
    m_pComboLogLevel->setCurrentIndex(appConfig().logLevel());
    m_pCheckBoxLogToFile->setChecked(appConfig().logToFile());
    m_pLineEditLogFilename->setText(appConfig().logFilename());
    setIndexFromItemData(m_pComboLanguage, appConfig().language());
    m_pCheckBoxAutoHide->setChecked(appConfig().getAutoHide());
    m_pCheckBoxAutoStart->setChecked(appConfig().getAutoStart());
    m_pCheckBoxMinimizeToTray->setChecked(appConfig().getMinimizeToTray());
    m_pCheckBoxEnableCrypto->setChecked(m_appConfig.getCryptoEnabled());

    // Don't allow auto hide or minimise to tray if it's not available
    m_pCheckBoxAutoHide->setEnabled(QSystemTrayIcon::isSystemTrayAvailable());
    m_pCheckBoxMinimizeToTray->setEnabled(QSystemTrayIcon::isSystemTrayAvailable());

#if defined(Q_OS_WIN)
    m_pComboElevate->setCurrentIndex(static_cast<int>(appConfig().elevateMode()));
#else
    // elevate checkbox is only useful on ms windows.
    m_pLabelElevate->hide();
    m_pComboElevate->hide();
#endif
}

void SettingsDialog::accept()
{
    m_appConfig.setScreenName(m_pLineEditScreenName->text());
    m_appConfig.setPort(m_pSpinBoxPort->value());
    m_appConfig.setNetworkInterface(m_pLineEditInterface->text());
    m_appConfig.setCryptoEnabled(m_pCheckBoxEnableCrypto->isChecked());
    m_appConfig.setLogLevel(m_pComboLogLevel->currentIndex());
    m_appConfig.setLogToFile(m_pCheckBoxLogToFile->isChecked());
    m_appConfig.setLogFilename(m_pLineEditLogFilename->text());
    m_appConfig.setLanguage(m_pComboLanguage->itemData(m_pComboLanguage->currentIndex()).toString());
    m_appConfig.setElevateMode(static_cast<ElevateMode>(m_pComboElevate->currentIndex()));
    m_appConfig.setAutoHide(m_pCheckBoxAutoHide->isChecked());
    m_appConfig.setAutoStart(m_pCheckBoxAutoStart->isChecked());
    m_appConfig.setMinimizeToTray(m_pCheckBoxMinimizeToTray->isChecked());
    m_appConfig.saveSettings();
    QDialog::accept();
}

void SettingsDialog::reject()
{
    if (m_appConfig.language() != m_pComboLanguage->itemData(m_pComboLanguage->currentIndex()).toString()) {
        QBarrierApplication::getInstance()->switchTranslator(m_appConfig.language());
    }
    QDialog::reject();
}

void SettingsDialog::changeEvent(QEvent* event)
{
    if (event != 0)
    {
        switch (event->type())
        {
        case QEvent::LanguageChange:
            {
                int logLevelIndex = m_pComboLogLevel->currentIndex();

                m_pComboLanguage->blockSignals(true);
                retranslateUi(this);
                m_pComboLanguage->blockSignals(false);

                m_pComboLogLevel->setCurrentIndex(logLevelIndex);
                break;
            }

        default:
            QDialog::changeEvent(event);
        }
    }
}

void SettingsDialog::on_m_pCheckBoxLogToFile_stateChanged(int i)
{
    bool checked = i == 2;

    m_pLineEditLogFilename->setEnabled(checked);
    m_pButtonBrowseLog->setEnabled(checked);
}

void SettingsDialog::on_m_pButtonBrowseLog_clicked()
{
    QString fileName = QFileDialog::getSaveFileName(
        this, tr("Save log file to..."),
        m_pLineEditLogFilename->text(),
        "Logs (*.log *.txt)");

    if (!fileName.isEmpty())
    {
        m_pLineEditLogFilename->setText(fileName);
    }
}

void SettingsDialog::on_m_pComboLanguage_currentIndexChanged(int index)
{
    QString ietfCode = m_pComboLanguage->itemData(index).toString();
    QBarrierApplication::getInstance()->switchTranslator(ietfCode);
}
