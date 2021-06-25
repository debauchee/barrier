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

#include <iostream>

#include "MainWindow.h"

#include "Fingerprint.h"
#include "AboutDialog.h"
#include "ServerConfigDialog.h"
#include "SettingsDialog.h"
#include "ZeroconfService.h"
#include "DataDownloader.h"
#include "CommandProcess.h"
#include "QUtility.h"
#include "ProcessorArch.h"
#include "SslCertificate.h"
#include "ShutdownCh.h"

#include <QtCore>
#include <QtGui>
#include <QtNetwork>
#include <QNetworkAccessManager>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QFileDialog>
#include <QDesktopServices>
#include <QDesktopWidget>

#if defined(Q_OS_MAC)
#include <ApplicationServices/ApplicationServices.h>
#endif

#if defined(Q_OS_WIN)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

static const QString allFilesFilter(QObject::tr("All files (*.*)"));
#if defined(Q_OS_WIN)
static const char barrierConfigName[] = "barrier.sgc";
static const QString barrierConfigFilter(QObject::tr("Barrier Configurations (*.sgc)"));
static QString bonjourBaseUrl = "http://binaries.symless.com/bonjour/";
static const char bonjourFilename32[] = "Bonjour.msi";
static const char bonjourFilename64[] = "Bonjour64.msi";
static const char bonjourTargetFilename[] = "Bonjour.msi";
#else
static const char barrierConfigName[] = "barrier.conf";
static const QString barrierConfigFilter(QObject::tr("Barrier Configurations (*.conf)"));
#endif
static const QString barrierConfigOpenFilter(barrierConfigFilter + ";;" + allFilesFilter);
static const QString barrierConfigSaveFilter(barrierConfigFilter);

static const char* barrierIconFiles[] =
{
#if defined(Q_OS_MAC)
    ":/res/icons/32x32/barrier-disconnected-mask.png",
    ":/res/icons/32x32/barrier-disconnected-mask.png",
    ":/res/icons/32x32/barrier-connected-mask.png",
    ":/res/icons/32x32/barrier-transfering-mask.png"
#else
    ":/res/icons/16x16/barrier-disconnected.png",
    ":/res/icons/16x16/barrier-disconnected.png",
    ":/res/icons/16x16/barrier-connected.png",
    ":/res/icons/16x16/barrier-transfering.png"
#endif
};

static const char* barrierIconNames[] =
{
    "barrier-disconnected",
    "barrier-disconnected",
    "barrier-connected",
    "barrier-transfering"
};

static const char* barrierLargeIcon = ":/res/icons/256x256/barrier.ico";

MainWindow::MainWindow(QSettings& settings, AppConfig& appConfig) :
    m_Settings(settings),
    m_AppConfig(&appConfig),
    m_pBarrier(NULL),
    m_BarrierState(barrierDisconnected),
    m_ServerConfig(&m_Settings, 5, 3, m_AppConfig->screenName(), this),
    m_pTempConfigFile(NULL),
    m_pTrayIcon(NULL),
    m_pTrayIconMenu(NULL),
    m_AlreadyHidden(false),
    m_pMenuBar(NULL),
    m_pMenuBarrier(NULL),
    m_pMenuHelp(NULL),
    m_pZeroconfService(NULL),
    m_pDataDownloader(NULL),
    m_DownloadMessageBox(NULL),
    m_pCancelButton(NULL),
    m_SuppressAutoConfigWarning(false),
    m_BonjourInstall(NULL),
    m_SuppressEmptyServerWarning(false),
    m_ExpectedRunningState(kStopped),
    m_pSslCertificate(NULL),
    m_pLogWindow(new LogWindow(nullptr))
{
    // explicitly unset DeleteOnClose so the window can be show and hidden
    // repeatedly until Barrier is finished
    setAttribute(Qt::WA_DeleteOnClose, false);
    // mark the windows as sort of "dialog" window so that tiling window
    // managers will float it by default (X11)
    setAttribute(Qt::WA_X11NetWmWindowTypeDialog, true);

    setupUi(this);
    setWindowIcon(QIcon(barrierLargeIcon));
    createMenuBar();
    loadSettings();
    initConnections();

    m_pLabelScreenName->setText(getScreenName());
    m_pLabelIpAddresses->setText(getIPAddresses());

#if defined(Q_OS_WIN)
    // ipc must always be enabled, so that we can disable command when switching to desktop mode.
    connect(&m_IpcClient, SIGNAL(readLogLine(const QString&)), this, SLOT(appendLogRaw(const QString&)));
    connect(&m_IpcClient, SIGNAL(errorMessage(const QString&)), this, SLOT(appendLogError(const QString&)));
    connect(&m_IpcClient, SIGNAL(infoMessage(const QString&)), this, SLOT(appendLogInfo(const QString&)));
    m_IpcClient.connectToHost();
#endif

    // change default size based on os
#if defined(Q_OS_MAC)
    resize(720, 550);
    setMinimumSize(720, 0);
#elif defined(Q_OS_LINUX)
    resize(700, 530);
    setMinimumSize(700, 0);
#endif

    m_SuppressAutoConfigWarning = true;
    m_pCheckBoxAutoConfig->setChecked(appConfig.autoConfig());
    m_SuppressAutoConfigWarning = false;

    m_pComboServerList->hide();
    m_pLabelPadlock->hide();

    updateSSLFingerprint();

    // resize window to smallest reasonable size
    resize(0, 0);
}

MainWindow::~MainWindow()
{
    if (appConfig().processMode() == Desktop) {
        m_ExpectedRunningState = kStopped;
        stopDesktop();
    }

    saveSettings();

    delete m_pZeroconfService;
    delete m_DownloadMessageBox;
    delete m_BonjourInstall;
    delete m_pSslCertificate;

    // LogWindow is created as a sibling of the MainWindow rather than a child
    // so that the main window can be hidden without hiding the log. because of
    // this it does not get properly cleaned up by the QObject system. also by
    // the time this destructor is called the event loop will no longer be able
    // to clean up the LogWindow so ->deleteLater() will not work
    delete m_pLogWindow;
}

void MainWindow::open()
{
    createTrayIcon();

    if (appConfig().getAutoHide()) {
        hide();
    } else {
        showNormal();
    }

    if (!appConfig().autoConfigPrompted()) {
        promptAutoConfig();
    }

    // only start if user has previously started. this stops the gui from
    // auto hiding before the user has configured barrier (which of course
    // confuses first time users, who think barrier has crashed).
    if (appConfig().startedBefore() && appConfig().getAutoStart()) {
        m_SuppressEmptyServerWarning = true;
        startBarrier();
        m_SuppressEmptyServerWarning = false;
    }
}

void MainWindow::setStatus(const QString &status)
{
    m_pStatusLabel->setText(status);
}

void MainWindow::createTrayIcon()
{
    m_pTrayIconMenu = new QMenu(this);

    m_pTrayIconMenu->addAction(m_pActionStartBarrier);
    m_pTrayIconMenu->addAction(m_pActionStopBarrier);
    m_pTrayIconMenu->addAction(m_pActionShowLog);
    m_pTrayIconMenu->addSeparator();

    m_pTrayIconMenu->addAction(m_pActionMinimize);
    m_pTrayIconMenu->addAction(m_pActionRestore);
    m_pTrayIconMenu->addSeparator();
    m_pTrayIconMenu->addAction(m_pActionQuit);

    m_pTrayIcon = new QSystemTrayIcon(this);
    m_pTrayIcon->setContextMenu(m_pTrayIconMenu);
    m_pTrayIcon->setToolTip("Barrier");

    connect(m_pTrayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(trayActivated(QSystemTrayIcon::ActivationReason)));

    setIcon(barrierDisconnected);

    m_pTrayIcon->show();
}

void MainWindow::retranslateMenuBar()
{
    m_pMenuBarrier->setTitle(tr("&Barrier"));
    m_pMenuHelp->setTitle(tr("&Help"));
}

void MainWindow::createMenuBar()
{
    m_pMenuBar = new QMenuBar(this);
    m_pMenuBarrier = new QMenu("", m_pMenuBar);
    m_pMenuHelp = new QMenu("", m_pMenuBar);
    retranslateMenuBar();

    m_pMenuBar->addAction(m_pMenuBarrier->menuAction());
    m_pMenuBar->addAction(m_pMenuHelp->menuAction());

    m_pMenuBarrier->addAction(m_pActionShowLog);
    m_pMenuBarrier->addAction(m_pActionSettings);
    m_pMenuBarrier->addAction(m_pActionMinimize);
    m_pMenuBarrier->addSeparator();
    m_pMenuBarrier->addAction(m_pActionSave);
    m_pMenuBarrier->addSeparator();
    m_pMenuBarrier->addAction(m_pActionQuit);
    m_pMenuHelp->addAction(m_pActionAbout);

    setMenuBar(m_pMenuBar);
}

void MainWindow::loadSettings()
{
    // the next two must come BEFORE loading groupServerChecked and groupClientChecked or
    // disabling and/or enabling the right widgets won't automatically work
    m_pRadioExternalConfig->setChecked(settings().value("useExternalConfig", false).toBool());
    m_pRadioInternalConfig->setChecked(settings().value("useInternalConfig", true).toBool());

    m_pGroupServer->setChecked(settings().value("groupServerChecked", false).toBool());
    m_pLineEditConfigFile->setText(settings().value("configFile", QDir::homePath() + "/" + barrierConfigName).toString());
    m_pGroupClient->setChecked(settings().value("groupClientChecked", true).toBool());
    m_pLineEditHostname->setText(settings().value("serverHostname").toString());
}

void MainWindow::initConnections()
{
    connect(m_pActionMinimize, SIGNAL(triggered()), this, SLOT(hide()));
    connect(m_pActionRestore, SIGNAL(triggered()), this, SLOT(showNormal()));
    connect(m_pActionStartBarrier, SIGNAL(triggered()), this, SLOT(startBarrier()));
    connect(m_pActionStopBarrier, SIGNAL(triggered()), this, SLOT(stopBarrier()));
    connect(m_pActionShowLog, SIGNAL(triggered()), this, SLOT(showLogWindow()));
    connect(m_pActionQuit, SIGNAL(triggered()), qApp, SLOT(quit()));
}

void MainWindow::saveSettings()
{
    // program settings
    settings().setValue("groupServerChecked", m_pGroupServer->isChecked());
    settings().setValue("useExternalConfig", m_pRadioExternalConfig->isChecked());
    settings().setValue("configFile", m_pLineEditConfigFile->text());
    settings().setValue("useInternalConfig", m_pRadioInternalConfig->isChecked());
    settings().setValue("groupClientChecked", m_pGroupClient->isChecked());
    settings().setValue("serverHostname", m_pLineEditHostname->text());

    settings().sync();
}

void MainWindow::setIcon(qBarrierState state)
{
    if (m_pTrayIcon) {
        QIcon icon = QIcon::fromTheme(barrierIconNames[state], QIcon(barrierIconFiles[state]));
#if defined(Q_OS_MAC)
        icon.setIsMask(true);
#endif
        m_pTrayIcon->setIcon(icon);
    }
}

void MainWindow::trayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick)
    {
        if (isVisible())
        {
            hide();
        }
        else
        {
            showNormal();
            activateWindow();
        }
    }
}

void MainWindow::logOutput()
{
    if (m_pBarrier)
    {
        QString text(m_pBarrier->readAllStandardOutput());
        for (QString line : text.split(QRegExp("\r|\n|\r\n"))) {
            if (!line.isEmpty())
            {
                appendLogRaw(line);
            }
        }
    }
}

void MainWindow::logError()
{
    if (m_pBarrier)
    {
        appendLogRaw(m_pBarrier->readAllStandardError());
    }
}

void MainWindow::appendLogInfo(const QString& text)
{
    if (appConfig().logLevel() >= 3) {
        m_pLogWindow->appendInfo(text);
    }
}

void MainWindow::appendLogDebug(const QString& text) {
    if (appConfig().logLevel() >= 4) {
        m_pLogWindow->appendDebug(text);
    }
}

void MainWindow::appendLogError(const QString& text)
{
    m_pLogWindow->appendError(text);
}

void MainWindow::appendLogRaw(const QString& text)
{
    for (QString line : text.split(QRegExp("\r|\n|\r\n"))) {
        if (!line.isEmpty()) {
            m_pLogWindow->appendRaw(line);
            updateFromLogLine(line);
        }
    }
}

void MainWindow::updateFromLogLine(const QString &line)
{
    // TODO: this code makes Andrew cry
    checkConnected(line);
    checkFingerprint(line);
}

void MainWindow::checkConnected(const QString& line)
{
    // TODO: implement ipc connection state messages to replace this hack.
    if (line.contains("started server") ||
        line.contains("connected to server") ||
        line.contains("server status: active"))
    {
        setBarrierState(barrierConnected);

        if (!appConfig().startedBefore() && isVisible()) {
                QMessageBox::information(
                    this, "Barrier",
                    tr("Barrier is now connected. You can close the "
                    "config window and Barrier will remain connected in "
                    "the background."));

            appConfig().setStartedBefore(true);
            appConfig().saveSettings();
        }
    }
}

void MainWindow::checkFingerprint(const QString& line)
{
    QRegExp fingerprintRegex(".*server fingerprint: ([A-F0-9:]+)");
    if (!fingerprintRegex.exactMatch(line)) {
        return;
    }

    QString fingerprint = fingerprintRegex.cap(1);
    if (Fingerprint::trustedServers().isTrusted(fingerprint)) {
        return;
    }

    static bool messageBoxAlreadyShown = false;

    if (!messageBoxAlreadyShown) {
        stopBarrier();

        messageBoxAlreadyShown = true;
        QMessageBox::StandardButton fingerprintReply =
            QMessageBox::information(
            this, tr("Security question"),
            tr("Do you trust this fingerprint?\n\n"
               "%1\n\n"
               "This is a server fingerprint. You should compare this "
               "fingerprint to the one on your server's screen. If the "
               "two don't match exactly, then it's probably not the server "
               "you're expecting (it could be a malicious user).\n\n"
               "To automatically trust this fingerprint for future "
               "connections, click Yes. To reject this fingerprint and "
               "disconnect from the server, click No.")
            .arg(fingerprint),
            QMessageBox::Yes | QMessageBox::No);

        if (fingerprintReply == QMessageBox::Yes) {
            // restart core process after trusting fingerprint.
            Fingerprint::trustedServers().trust(fingerprint);
            startBarrier();
        }

        messageBoxAlreadyShown = false;
    }
}

void MainWindow::restartBarrier()
{
    stopBarrier();
    startBarrier();
}

void MainWindow::proofreadInfo()
{
    int oldState = m_BarrierState;
    m_BarrierState = barrierDisconnected;
    setBarrierState((qBarrierState)oldState);
}

void MainWindow::startBarrier()
{
    bool desktopMode = appConfig().processMode() == Desktop;
    bool serviceMode = appConfig().processMode() == Service;

    appendLogDebug("starting process");
    m_ExpectedRunningState = kStarted;
    setBarrierState(barrierConnecting);

    QString app;
    QStringList args;

    args << "-f" << "--no-tray" << "--debug" << appConfig().logLevelText();


    args << "--name" << getScreenName();

    if (desktopMode)
    {
        setBarrierProcess(new QProcess(this));
    }
    else
    {
        // tell client/server to talk to daemon through ipc.
        args << "--ipc";

#if defined(Q_OS_WIN)
        // tell the client/server to shut down when a ms windows desk
        // is switched; this is because we may need to elevate or not
        // based on which desk the user is in (login always needs
        // elevation, where as default desk does not).
        // Note that this is only enabled when barrier is set to elevate
        // 'as needed' (e.g. on a UAC dialog popup) in order to prevent
        // unnecessary restarts when barrier was started elevated or
        // when it is not allowed to elevate. In these cases restarting
        // the server is fruitless.
        if (appConfig().elevateMode() == ElevateAsNeeded) {
                args << "--stop-on-desk-switch";
        }
#endif
    }

#ifndef Q_OS_LINUX

    if (m_ServerConfig.enableDragAndDrop()) {
        args << "--enable-drag-drop";
    }

#endif

    if (m_AppConfig->getCryptoEnabled()) {
        args << "--enable-crypto";
    }

    if (m_AppConfig->getHeadlessMode()) {
        args << "--headless-mode";
    }
#if defined(Q_OS_WIN)
    // on windows, the profile directory changes depending on the user that
    // launched the process (e.g. when launched with elevation). setting the
    // profile dir on launch ensures it uses the same profile dir is used
    // no matter how its relaunched.
    args << "--profile-dir" << QString("\"%1\"").arg(profilePath());
#endif

    if ((barrierType() == barrierClient && !clientArgs(args, app))
        || (barrierType() == barrierServer && !serverArgs(args, app)))
    {
        stopBarrier();
        return;
    }

    if (desktopMode)
    {
        connect(barrierProcess(), SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(barrierFinished(int, QProcess::ExitStatus)));
        connect(barrierProcess(), SIGNAL(readyReadStandardOutput()), this, SLOT(logOutput()));
        connect(barrierProcess(), SIGNAL(readyReadStandardError()), this, SLOT(logError()));
    }

    m_pLogWindow->startNewInstance();

    appendLogInfo("starting " + QString(barrierType() == barrierServer ? "server" : "client"));

    qDebug() << args;

    appendLogDebug(QString("command: %1 %2").arg(app, args.join(" ")));

    appendLogInfo("config file: " + configFilename());
    appendLogInfo("log level: " + appConfig().logLevelText());

    if (appConfig().logToFile())
        appendLogInfo("log file: " + appConfig().logFilename());

    if (desktopMode)
    {
        barrierProcess()->start(app, args);
        if (!barrierProcess()->waitForStarted())
        {
            show();
            QMessageBox::warning(this, tr("Program can not be started"), QString(tr("The executable<br><br>%1<br><br>could not be successfully started, although it does exist. Please check if you have sufficient permissions to run this program.").arg(app)));
            return;
        }
    }

    if (serviceMode)
    {
        QString command(app + " " + args.join(" "));
        m_IpcClient.sendCommand(command, appConfig().elevateMode());
    }
}

bool MainWindow::clientArgs(QStringList& args, QString& app)
{
    app = appPath(appConfig().barriercName());

    if (!QFile::exists(app))
    {
        show();
        QMessageBox::warning(this, tr("Barrier client not found"),
                             tr("The executable for the barrier client does not exist."));
        return false;
    }

#if defined(Q_OS_WIN)
    // wrap in quotes so a malicious user can't start \Program.exe as admin.
    app = QString("\"%1\"").arg(app);
#endif

    if (appConfig().logToFile())
    {
        appConfig().persistLogDir();
        args << "--log" << appConfig().logFilenameCmd();
    }

    // check auto config first, if it is disabled or no server detected,
    // use line edit host name if it is not empty
    if (m_pCheckBoxAutoConfig->isChecked()) {
        if (m_pComboServerList->count() != 0) {
            QString serverIp = m_pComboServerList->currentText();
            args << "[" + serverIp + "]:" + QString::number(appConfig().port());
            return true;
        }
    } else if (m_pLineEditHostname->text().isEmpty()) {
        show();
        if (!m_SuppressEmptyServerWarning) {
            QMessageBox::warning(this, tr("Hostname is empty"),
                             tr("Please fill in a hostname for the barrier client to connect to."));
        }
        return false;
    }

    args << "[" + m_pLineEditHostname->text() + "]:" + QString::number(appConfig().port());

    return true;
}

QString MainWindow::configFilename()
{
    QString filename;
    if (m_pRadioInternalConfig->isChecked())
    {
        // TODO: no need to use a temporary file, since we need it to
        // be permenant (since it'll be used for Windows services, etc).
        m_pTempConfigFile = new QTemporaryFile();
        if (!m_pTempConfigFile->open())
        {
            QMessageBox::critical(this, tr("Cannot write configuration file"), tr("The temporary configuration file required to start barrier can not be written."));
            return "";
        }

        serverConfig().save(*m_pTempConfigFile);
        filename = m_pTempConfigFile->fileName();

        m_pTempConfigFile->close();
    }
    else
    {
        if (!QFile::exists(m_pLineEditConfigFile->text()))
        {
            if (QMessageBox::warning(this, tr("Configuration filename invalid"),
                tr("You have not filled in a valid configuration file for the barrier server. "
                        "Do you want to browse for the configuration file now?"), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes
                    || !on_m_pButtonBrowseConfigFile_clicked())
                return "";
        }

        filename = m_pLineEditConfigFile->text();
    }
    return filename;
}

QString MainWindow::address()
{
    QString address = appConfig().networkInterface();
    if (!address.isEmpty())
        address = "[" + address + "]";
    return address + ":" + QString::number(appConfig().port());
}

QString MainWindow::appPath(const QString& name)
{
    return appConfig().barrierProgramDir() + name;
}

bool MainWindow::serverArgs(QStringList& args, QString& app)
{
    app = appPath(appConfig().barriersName());

    if (!QFile::exists(app))
    {
        QMessageBox::warning(this, tr("Barrier server not found"),
                             tr("The executable for the barrier server does not exist."));
        return false;
    }

#if defined(Q_OS_WIN)
    // wrap in quotes so a malicious user can't start \Program.exe as admin.
    app = QString("\"%1\"").arg(app);
#endif

    if (appConfig().logToFile())
    {
        appConfig().persistLogDir();

        args << "--log" << appConfig().logFilenameCmd();
    }

    QString configFilename = this->configFilename();
#if defined(Q_OS_WIN)
    // wrap in quotes in case username contains spaces.
    configFilename = QString("\"%1\"").arg(configFilename);
#endif
    args << "-c" << configFilename << "--address" << address();

    return true;
}

void MainWindow::stopBarrier()
{
    appendLogDebug("stopping process");

    m_ExpectedRunningState = kStopped;

    if (appConfig().processMode() == Service)
    {
        stopService();
    }
    else if (appConfig().processMode() == Desktop)
    {
        stopDesktop();
    }

    setBarrierState(barrierDisconnected);

    // HACK: deleting the object deletes the physical file, which is
    // bad, since it could be in use by the Windows service!
#if !defined(Q_OS_WIN)
    delete m_pTempConfigFile;
#endif
    m_pTempConfigFile = NULL;

    // reset so that new connects cause auto-hide.
    m_AlreadyHidden = false;
}

void MainWindow::stopService()
{
    // send empty command to stop service from laucning anything.
    m_IpcClient.sendCommand("", appConfig().elevateMode());
}

void MainWindow::stopDesktop()
{
    QMutexLocker locker(&m_StopDesktopMutex);
    if (!barrierProcess()) {
        return;
    }

    appendLogInfo("stopping barrier desktop process");

    if (barrierProcess()->isOpen()) {
        // try to shutdown child gracefully
        barrierProcess()->write(&ShutdownCh, 1);
        barrierProcess()->waitForFinished(5000);
        barrierProcess()->close();
    }

    delete barrierProcess();
    setBarrierProcess(NULL);
}

void MainWindow::barrierFinished(int exitCode, QProcess::ExitStatus)
{
    if (exitCode == 0) {
        appendLogInfo(QString("process exited normally"));
    }
    else {
        appendLogError(QString("process exited with error code: %1").arg(exitCode));
    }

    if (m_ExpectedRunningState == kStarted) {
        QTimer::singleShot(1000, this, SLOT(startBarrier()));
        appendLogInfo(QString("detected process not running, auto restarting"));
    }
    else {
        setBarrierState(barrierDisconnected);
    }
}

void MainWindow::setBarrierState(qBarrierState state)
{
    if (barrierState() == state)
        return;

    if (state == barrierConnected || state == barrierConnecting)
    {
        disconnect (m_pButtonToggleStart, SIGNAL(clicked()), m_pActionStartBarrier, SLOT(trigger()));
        connect (m_pButtonToggleStart, SIGNAL(clicked()), m_pActionStopBarrier, SLOT(trigger()));
        m_pButtonToggleStart->setText(tr("&Stop"));
        m_pButtonReload->setEnabled(true);
    }
    else if (state == barrierDisconnected)
    {
        disconnect (m_pButtonToggleStart, SIGNAL(clicked()), m_pActionStopBarrier, SLOT(trigger()));
        connect (m_pButtonToggleStart, SIGNAL(clicked()), m_pActionStartBarrier, SLOT(trigger()));
        m_pButtonToggleStart->setText(tr("&Start"));
        m_pButtonReload->setEnabled(false);
    }

    bool connected = false;
    if (state == barrierConnected || state == barrierTransfering) {
        connected = true;
    }

    m_pActionStartBarrier->setEnabled(!connected);
    m_pActionStopBarrier->setEnabled(connected);

    switch (state)
    {
    case barrierConnected: {
        if (m_AppConfig->getCryptoEnabled()) {
            m_pLabelPadlock->show();
        }
        else {
            m_pLabelPadlock->hide();
        }

        setStatus(tr("Barrier is running."));

        break;
    }
    case barrierConnecting:
        m_pLabelPadlock->hide();
        setStatus(tr("Barrier is starting."));
        break;
    case barrierDisconnected:
        m_pLabelPadlock->hide();
        setStatus(tr("Barrier is not running."));
        break;
    case barrierTransfering:
        break;
    }

    setIcon(state);

    m_BarrierState = state;
}

void MainWindow::setVisible(bool visible)
{
    QMainWindow::setVisible(visible);
    m_pActionMinimize->setEnabled(visible);
    m_pActionRestore->setEnabled(!visible);

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1070 // lion
    // dock hide only supported on lion :(
    ProcessSerialNumber psn = { 0, kCurrentProcess };
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    GetCurrentProcess(&psn);
#pragma GCC diagnostic pop
    if (visible)
        TransformProcessType(&psn, kProcessTransformToForegroundApplication);
    else
        TransformProcessType(&psn, kProcessTransformToBackgroundApplication);
#endif
}

QString MainWindow::getIPAddresses()
{
    QList<QHostAddress> addresses = QNetworkInterface::allAddresses();

    bool hinted = false;
    QString result;
    for (int i = 0; i < addresses.size(); i++) {
        if (addresses[i].protocol() == QAbstractSocket::IPv4Protocol &&
            addresses[i] != QHostAddress(QHostAddress::LocalHost)) {

            QString address = addresses[i].toString();
            QString format = "%1, ";

            // usually 192.168.x.x is a useful ip for the user, so indicate
            // this by making it bold.
            if (!hinted && address.startsWith("192.168")) {
                hinted = true;
                format = "<b>%1</b>, ";
            }

            result += format.arg(address);
        }
    }

    if (result == "") {
        return tr("Unknown");
    }

    // remove trailing comma.
    result.chop(2);

    return result;
}

QString MainWindow::getScreenName()
{
    if (appConfig().screenName() == "") {
        return QHostInfo::localHostName();
    }
    else {
        return appConfig().screenName();
    }
}

void MainWindow::changeEvent(QEvent* event)
{
    if (event != 0)
    {
        switch (event->type())
        {
        case QEvent::LanguageChange:
        {
            retranslateUi(this);
            retranslateMenuBar();

            proofreadInfo();

            break;
        }
        case QEvent::WindowStateChange:
        {
            windowStateChanged();
            break;
        }
        default:
        {
            break;
        }
        }
    }
    // all that do not return are allowing the event to propagate
    QMainWindow::changeEvent(event);
}

void MainWindow::updateZeroconfService()
{
    QMutexLocker locker(&m_UpdateZeroconfMutex);

    if (isBonjourRunning()) {
        if (!m_AppConfig->wizardShouldRun()) {
            if (m_pZeroconfService) {
                delete m_pZeroconfService;
                m_pZeroconfService = NULL;
            }

            if (m_AppConfig->autoConfig() || barrierType() == barrierServer) {
                m_pZeroconfService = new ZeroconfService(this);
            }
        }
    }
}

void MainWindow::serverDetected(const QString name)
{
    if (m_pComboServerList->findText(name) == -1) {
        // Note: the first added item triggers startBarrier
        m_pComboServerList->addItem(name);
    }

    if (m_pComboServerList->count() > 1) {
        m_pComboServerList->show();
    }
}

void MainWindow::updateSSLFingerprint()
{
    if (m_AppConfig->getCryptoEnabled() && m_pSslCertificate == nullptr) {
        m_pSslCertificate = new SslCertificate(this);
        connect(m_pSslCertificate, &SslCertificate::info, [&](QString info)
        {
            appendLogInfo(info);
        });
        m_pSslCertificate->generateCertificate();
    }
    if (m_AppConfig->getCryptoEnabled() && Fingerprint::local().fileExists()) {
        m_pLabelLocalFingerprint->setText(Fingerprint::local().readFirst());
        m_pLabelLocalFingerprint->setTextInteractionFlags(Qt::TextSelectableByMouse);
    } else {
        m_pLabelLocalFingerprint->setText("Disabled");
    }
}

void MainWindow::on_m_pGroupClient_toggled(bool on)
{
    m_pGroupServer->setChecked(!on);
    if (on) {
        updateZeroconfService();
    }
}

void MainWindow::on_m_pGroupServer_toggled(bool on)
{
    m_pGroupClient->setChecked(!on);
    if (on) {
        updateZeroconfService();
    }
}

bool MainWindow::on_m_pButtonBrowseConfigFile_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Browse for a barriers config file"), QString(), barrierConfigOpenFilter);

    if (!fileName.isEmpty())
    {
        m_pLineEditConfigFile->setText(fileName);
        return true;
    }

    return false;
}

bool  MainWindow::on_m_pActionSave_triggered()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save configuration as..."), QString(), barrierConfigSaveFilter);

    if (!fileName.isEmpty() && !serverConfig().save(fileName))
    {
        QMessageBox::warning(this, tr("Save failed"), tr("Could not save configuration to file."));
        return true;
    }

    return false;
}

void MainWindow::on_m_pActionAbout_triggered()
{
    AboutDialog(this, appPath(appConfig().barriercName())).exec();
}

void MainWindow::on_m_pActionSettings_triggered()
{
    if (SettingsDialog(this, appConfig()).exec() == QDialog::Accepted)
        updateSSLFingerprint();
}

void MainWindow::autoAddScreen(const QString name)
{
    if (!m_ServerConfig.ignoreAutoConfigClient()) {
        int r = m_ServerConfig.autoAddScreen(name);
        if (r != kAutoAddScreenOk) {
            switch (r) {
            case kAutoAddScreenManualServer:
                showConfigureServer(
                    tr("Please add the server (%1) to the grid.")
                        .arg(appConfig().screenName()));
                break;

            case kAutoAddScreenManualClient:
                showConfigureServer(
                    tr("Please drag the new client screen (%1) "
                        "to the desired position on the grid.")
                        .arg(name));
                break;
            }
        }
        else {
            restartBarrier();
        }
    }
}

void MainWindow::showConfigureServer(const QString& message)
{
    ServerConfigDialog dlg(this, serverConfig(), appConfig().screenName());
    dlg.message(message);
    dlg.exec();
}

void MainWindow::on_m_pButtonConfigureServer_clicked()
{
    showConfigureServer();
}

void MainWindow::on_m_pButtonReload_clicked()
{
    restartBarrier();
}

#if defined(Q_OS_WIN)
bool MainWindow::isServiceRunning(QString name)
{
    SC_HANDLE hSCManager;
    hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    if (hSCManager == NULL) {
        appendLogError("failed to open a service controller manager, error: " +
            GetLastError());
        return false;
    }

    auto array = name.toLocal8Bit();
    SC_HANDLE hService = OpenService(hSCManager, array.data(), SERVICE_QUERY_STATUS);

    if (hService == NULL) {
        appendLogDebug("failed to open service: " + name);
        return false;
    }

    SERVICE_STATUS status;
    if (QueryServiceStatus(hService, &status)) {
        if (status.dwCurrentState == SERVICE_RUNNING) {
            return true;
        }
    }

    return false;
}
#else
bool MainWindow::isServiceRunning()
{
    return false;
}
#endif

bool MainWindow::isBonjourRunning()
{
    bool result = false;

#if defined(Q_OS_WIN)
    result = isServiceRunning("Bonjour Service");
#else
    result = true;
#endif

    return result;
}

void MainWindow::downloadBonjour()
{
#if defined(Q_OS_WIN)
    QUrl url;
    int arch = getProcessorArch();
    if (arch == kProcessorArchWin32) {
        url.setUrl(bonjourBaseUrl + bonjourFilename32);
        appendLogInfo("downloading 32-bit Bonjour");
    }
    else if (arch == kProcessorArchWin64) {
        url.setUrl(bonjourBaseUrl + bonjourFilename64);
        appendLogInfo("downloading 64-bit Bonjour");
    }
    else {
        QMessageBox::critical(
            this, tr("Barrier"),
            tr("Failed to detect system architecture."));
        return;
    }

    if (m_pDataDownloader == NULL) {
        m_pDataDownloader = new DataDownloader(this);
        connect(m_pDataDownloader, SIGNAL(isComplete()), SLOT(installBonjour()));
    }

    m_pDataDownloader->download(url);

    if (m_DownloadMessageBox == NULL) {
        m_DownloadMessageBox = new QMessageBox(this);
        m_DownloadMessageBox->setWindowTitle("Barrier");
        m_DownloadMessageBox->setIcon(QMessageBox::Information);
        m_DownloadMessageBox->setText("Installing Bonjour, please wait...");
        m_DownloadMessageBox->setStandardButtons(0);
        m_pCancelButton = m_DownloadMessageBox->addButton(
            tr("Cancel"), QMessageBox::RejectRole);
    }

    m_DownloadMessageBox->exec();

    if (m_DownloadMessageBox->clickedButton() == m_pCancelButton) {
        m_pDataDownloader->cancel();
    }
#endif
}

void MainWindow::installBonjour()
{
#if defined(Q_OS_WIN)
#if QT_VERSION >= 0x050000
    QString tempLocation = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
#else
    QString tempLocation = QDesktopServices::storageLocation(
                                QDesktopServices::TempLocation);
#endif
    QString filename = tempLocation;
    filename.append("\\").append(bonjourTargetFilename);
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        m_DownloadMessageBox->hide();

        QMessageBox::warning(
            this, "Barrier",
            tr("Failed to download Bonjour installer to location: %1")
            .arg(tempLocation));
        return;
    }

    file.write(m_pDataDownloader->data());
    file.close();

    QStringList arguments;
    arguments.append("/i");
    QString winFilename = QDir::toNativeSeparators(filename);
    arguments.append(winFilename);
    arguments.append("/passive");
    if (m_BonjourInstall == NULL) {
        m_BonjourInstall = new CommandProcess("msiexec", arguments);
    }

    QThread* thread = new QThread;
    connect(m_BonjourInstall, SIGNAL(finished()), this,
        SLOT(bonjourInstallFinished()));
    connect(m_BonjourInstall, SIGNAL(finished()), thread, SLOT(quit()));
    connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));

    m_BonjourInstall->moveToThread(thread);
    thread->start();

    QMetaObject::invokeMethod(m_BonjourInstall, "run", Qt::QueuedConnection);

    m_DownloadMessageBox->hide();
#endif
}

void MainWindow::promptAutoConfig()
{
    if (!isBonjourRunning()) {
        int r = QMessageBox::question(
            this, tr("Barrier"),
            tr("Do you want to enable auto config and install Bonjour?\n\n"
               "This feature helps you establish the connection."),
            QMessageBox::Yes | QMessageBox::No);

        if (r == QMessageBox::Yes) {
            m_AppConfig->setAutoConfig(true);
            downloadBonjour();
        }
        else {
            m_AppConfig->setAutoConfig(false);
            m_pCheckBoxAutoConfig->setChecked(false);
        }
    }

    m_AppConfig->setAutoConfigPrompted(true);
}

void MainWindow::on_m_pComboServerList_currentIndexChanged(QString )
{
    if (m_pComboServerList->count() != 0) {
        restartBarrier();
    }
}

void MainWindow::on_m_pCheckBoxAutoConfig_toggled(bool checked)
{
    if (!isBonjourRunning() && checked) {
        if (!m_SuppressAutoConfigWarning) {
            int r = QMessageBox::information(
                this, tr("Barrier"),
                tr("Auto config feature requires Bonjour.\n\n"
                   "Do you want to install Bonjour?"),
                QMessageBox::Yes | QMessageBox::No);

            if (r == QMessageBox::Yes) {
                downloadBonjour();
            }
        }

        m_pCheckBoxAutoConfig->setChecked(false);
        return;
    }

    m_pLineEditHostname->setDisabled(checked);
    appConfig().setAutoConfig(checked);
    updateZeroconfService();

    if (!checked) {
        m_pComboServerList->clear();
        m_pComboServerList->hide();
    }
}

void MainWindow::bonjourInstallFinished()
{
    appendLogInfo("Bonjour install finished");

    m_pCheckBoxAutoConfig->setChecked(true);
}

void MainWindow::windowStateChanged()
{
    if (windowState() == Qt::WindowMinimized && appConfig().getMinimizeToTray())
        hide();
}

void MainWindow::showLogWindow()
{
    m_pLogWindow->show();
}
