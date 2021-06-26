/*
 * barrier -- mouse and keyboard sharing utility
 * Copyright (C) 2015-2016 Symless Ltd.
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

#include "SslCertificate.h"
#include "Fingerprint.h"
#include "QUtility.h"

#include <QProcess>
#include <QDir>
#include <QCoreApplication>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

static const char kCertificateLifetime[] = "365";
static const char kCertificateSubjectInfo[] = "/CN=Barrier";
static const char kCertificateFilename[] = "Barrier.pem";
static const char kSslDir[] = "SSL";
static const char kUnixOpenSslCommand[] = "openssl";

#if defined(Q_OS_WIN)
static const char kWinOpenSslBinary[] = "openssl.exe";
static const char kConfigFile[] = "barrier.conf";
#endif

SslCertificate::SslCertificate(QObject *parent) :
    QObject(parent)
{
    m_ProfileDir = profilePath();
    if (m_ProfileDir.isEmpty()) {
        emit error(tr("Failed to get profile directory."));
    }
}

std::pair<bool, QString> SslCertificate::runTool(const QStringList& args)
{
    QString program;
#if defined(Q_OS_WIN)
    program = QCoreApplication::applicationDirPath();
    program.append("\\").append(kWinOpenSslBinary);
#else
    program = kUnixOpenSslCommand;
#endif


    QStringList environment;
#if defined(Q_OS_WIN)
    environment << QString("OPENSSL_CONF=%1\\%2")
        .arg(QCoreApplication::applicationDirPath())
        .arg(kConfigFile);
#endif

    QProcess process;
    QString standardOutput, standardError;
    process.setEnvironment(environment);
    process.start(program, args);
    bool success = process.waitForStarted();

    if (success && process.waitForFinished())
    {
        standardOutput = QString::fromLocal8Bit(process.readAllStandardOutput().trimmed());
        standardError = QString::fromLocal8Bit(process.readAllStandardError().trimmed());
    }

    int code = process.exitCode();
    if (!success || code != 0)
    {
        emit error(
            QString("SSL tool failed: %1\n\nCode: %2\nError: %3")
                .arg(program)
                .arg(process.exitCode())
                .arg(standardError.isEmpty() ? "Unknown" : standardError));
        return {false, standardOutput};
    }

    return {true, standardOutput};
}

void SslCertificate::generateCertificate()
{
    auto filename = getCertificatePath();

    QFile file(filename);
    if (!file.exists() || !isCertificateValid(filename)) {
        QStringList arguments;

        // self signed certificate
        arguments.append("req");
        arguments.append("-x509");
        arguments.append("-nodes");

        // valide duration
        arguments.append("-days");
        arguments.append(kCertificateLifetime);

        // subject information
        arguments.append("-subj");

        QString subInfo(kCertificateSubjectInfo);
        arguments.append(subInfo);

        // private key
        arguments.append("-newkey");
        arguments.append("rsa:2048");

        QDir sslDir(getCertificateDirectory());
        if (!sslDir.exists()) {
            sslDir.mkpath(".");
        }

        // key output filename
        arguments.append("-keyout");
        arguments.append(filename);

        // certificate output filename
        arguments.append("-out");
        arguments.append(filename);

        if (!runTool(arguments).first) {
            return;
        }

        emit info(tr("SSL certificate generated."));
    }

    generateFingerprint(filename);

    emit generateFinished();
}

void SslCertificate::generateFingerprint(const QString& certificateFilename)
{
    QStringList arguments;
    arguments.append("x509");
    arguments.append("-fingerprint");
    arguments.append("-sha256");
    arguments.append("-noout");
    arguments.append("-in");
    arguments.append(certificateFilename);

    auto ret = runTool(arguments);
    bool success = ret.first;
    if (!success) {
        return;
    }

    // find the fingerprint from the tool output
    QString fingerprint = ret.second;
    auto i = fingerprint.indexOf('=');
    if (i != -1) {
        fingerprint.remove(0, i+1);

        Fingerprint::local().trust(fingerprint, false);
        emit info(tr("SSL fingerprint generated."));
    }
    else {
        emit error(tr("Failed to find SSL fingerprint."));
    }
}

QString SslCertificate::getCertificatePath()
{
    return getCertificateDirectory() + QDir::separator() + kCertificateFilename;
}

QString SslCertificate::getCertificateDirectory()
{
    return m_ProfileDir + QDir::separator() + kSslDir;
}

bool SslCertificate::isCertificateValid(const QString& path)
{
    OpenSSL_add_all_algorithms();
    ERR_load_BIO_strings();
    ERR_load_crypto_strings();

    BIO* bio = BIO_new(BIO_s_file());

    auto ret = BIO_read_filename(bio, path.toLocal8Bit().constData());
    if (!ret) {
        emit info(tr("Could not read from default certificate file."));
        BIO_free_all(bio);
        return false;
    }

    X509* cert = PEM_read_bio_X509(bio, NULL, 0, NULL);
    if (!cert) {
        emit info(tr("Error loading default certificate file to memory."));
        BIO_free_all(bio);
        return false;
    }

    EVP_PKEY* pubkey = X509_get_pubkey(cert);
    if (!pubkey) {
        emit info(tr("Default certificate key file does not contain valid public key"));
        X509_free(cert);
        BIO_free_all(bio);
        return false;
    }

    auto type = EVP_PKEY_type(EVP_PKEY_id(pubkey));
    if (type != EVP_PKEY_RSA && type != EVP_PKEY_DSA) {
        emit info(tr("Public key in default certificate key file is not RSA or DSA"));
        EVP_PKEY_free(pubkey);
        X509_free(cert);
        BIO_free_all(bio);
        return false;
    }

    auto bits = EVP_PKEY_bits(pubkey);
    if (bits < 2048) {
        // We could have small keys in old barrier installations
        emit info(tr("Public key in default certificate key file is too small."));
        EVP_PKEY_free(pubkey);
        X509_free(cert);
        BIO_free_all(bio);
        return false;
    }

    EVP_PKEY_free(pubkey);
    X509_free(cert);
    BIO_free_all(bio);
    return true;
}
