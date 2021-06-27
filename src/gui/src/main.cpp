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
#include "AppConfig.h"
#include "SetupWizard.h"
#include "DisplayIsValid.h"

#include <QtCore>
#include <QtGui>
#include <QSettings>
#include <QMessageBox>

#if defined(Q_OS_MAC)
#include <Carbon/Carbon.h>
#endif

#ifdef Q_OS_DARWIN
#include <cstdlib>
#endif

class QThreadImpl : public QThread
{
public:
	static void msleep(unsigned long msecs)
	{
		QThread::msleep(msecs);
	}
};

#if defined(Q_OS_MAC)
bool checkMacAssistiveDevices();
#endif

int main(int argc, char* argv[])
{
#ifdef WINAPI_XWINDOWS
    // QApplication's constructor will call a fscking abort() if
    // DISPLAY is bad. Let's check it first and handle it gracefully
    if (!display_is_valid()) {
        fprintf(stderr, "The Barrier GUI requires a display. Quitting...\n");
        return 1;
    }
#endif
#ifdef Q_OS_DARWIN
    /* Workaround for QTBUG-40332 - "High ping when QNetworkAccessManager is instantiated" */
    ::setenv ("QT_BEARER_POLL_TIMEOUT", "-1", 1);
#endif
	QCoreApplication::setOrganizationName("Debauchee");
	QCoreApplication::setOrganizationDomain("github.com");
	QCoreApplication::setApplicationName("Barrier");

	QBarrierApplication app(argc, argv);

#if defined(Q_OS_MAC)
	if (app.applicationDirPath().startsWith("/Volumes/")) {
        // macOS preferences track applications allowed assistive access by path
        // Unfortunately, there's no user-friendly way to allow assistive access
        // to applications that are not in default paths (/Applications),
        // especially if an identically named application already exists in
        // /Applications). Thus we require Barrier to reside in the /Applications
        // folder
		QMessageBox::information(
			NULL, "Barrier",
			"Please drag Barrier to the Applications folder, and open it from there.");
		return 1;
	}

	if (!checkMacAssistiveDevices())
	{
		return 1;
	}
#endif

	QSettings settings;
	AppConfig appConfig (&settings);

	app.switchTranslator(appConfig.language());

	MainWindow mainWindow(settings, appConfig);
	SetupWizard setupWizard(mainWindow, true);

	if (appConfig.wizardShouldRun())
	{
		setupWizard.show();
	}
	else
	{
		mainWindow.open();
	}

	return app.exec();
}

#if defined(Q_OS_MAC)
bool checkMacAssistiveDevices()
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090 // mavericks

	// new in mavericks, applications are trusted individually
	// with use of the accessibility api. this call will show a
	// prompt which can show the security/privacy/accessibility
	// tab, with a list of allowed applications. barrier should
	// show up there automatically, but will be unchecked.

	if (AXIsProcessTrusted()) {
		return true;
	}

	const void* keys[] = { kAXTrustedCheckOptionPrompt };
	const void* trueValue[] = { kCFBooleanTrue };
	CFDictionaryRef options = CFDictionaryCreate(NULL, keys, trueValue, 1, NULL, NULL);

	bool result = AXIsProcessTrustedWithOptions(options);
	CFRelease(options);
	return result;

#else

	// now deprecated in mavericks.
	bool result = AXAPIEnabled();
	if (!result) {
		QMessageBox::information(
			NULL, "Barrier",
			"Please enable access to assistive devices "
			"System Preferences -> Security & Privacy -> "
			"Privacy -> Accessibility, then re-open Barrier.");
	}
	return result;

#endif
}
#endif
