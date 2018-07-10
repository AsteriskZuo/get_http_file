/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtWidgets>
#include <QtNetwork>
#include <QUrl>
#include <QDebug>
#include <QSettings>

#include "httpwindow.h"
#include "ui_authenticationdialog.h"

#ifndef QT_NO_SSL
//static const char defaultUrl[] = "https://www.qt.io/";
//static const char defaultUrl[] = "http://download.qt.io/official_releases/qt/5.11/5.11.1/";
//static const char defaultUrl[] = "http://dldir1.qq.com/weixin/Windows/";
//static const char defaultUrl[] = "http://www.winrar.com.cn/download/";
static const char defaultUrl[] = "https://www.baidu.com/img/";
#else
//static const char defaultUrl[] = "http://www.qt.io/";
//static const char defaultUrl[] = "http://download.qt.io/official_releases/qt/5.11/5.11.1/";
static const char defaultUrl[] = "http://dldir1.qq.com/weixin/Windows/";
#endif
//static const char defaultFileName[] = "index.html";
//static const char defaultFileName[] = "qt-opensource-windows-x86-5.11.1.exe";
//static const char defaultFileName[] = "WeChatSetup.exe";
//static const char defaultFileName[] = "winrar-x64-550scp.exe";
static const char defaultFileName[] = "bdlogo.gif";

static quint64 _downloadFileSize = 0;
static quint64 _allFileSize = 0;

ProgressDialog::ProgressDialog(const QUrl &url, QWidget *parent)
  : QProgressDialog(parent)
{
    setWindowTitle(tr("Download Progress"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setLabelText(tr("Downloading %1.").arg(url.toDisplayString()));
    setMinimum(0);
    setValue(0);
    setMinimumDuration(500);
}

void ProgressDialog::networkReplyProgress(qint64 bytesRead, qint64 totalBytes)
{
    qDebug() << __FUNCTION__ << "bytesRead:" << bytesRead + _offsize << "totalBytes:" << totalBytes;
    static int scount = 0;
    if (1 == ++scount) {
        _allFileSize = totalBytes;
    }
    setMaximum(totalBytes);
    setValue(bytesRead + _offsize);
}

HttpWindow::HttpWindow(QWidget *parent)
    : QDialog(parent)
    , statusLabel(new QLabel(tr("Please enter the URL of a file you want to download.\n\n"), this))
    , urlLineEdit(new QLineEdit(defaultUrl))
    , downloadButton(new QPushButton(tr("Download")))
    , launchCheckBox(new QCheckBox("Launch file"))
    , defaultFileLineEdit(new QLineEdit(defaultFileName))
    , downloadDirectoryLineEdit(new QLineEdit)
    , reply(Q_NULLPTR)
    , file(Q_NULLPTR)
    , httpRequestAborted(false)
    //, _downloadFileSize(0)
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setWindowTitle(tr("HTTP"));

    connect(&qnam, &QNetworkAccessManager::authenticationRequired,
            this, &HttpWindow::slotAuthenticationRequired);
#ifndef QT_NO_SSL
    connect(&qnam, &QNetworkAccessManager::sslErrors,
            this, &HttpWindow::sslErrors);
#endif

    QFormLayout *formLayout = new QFormLayout;
    urlLineEdit->setClearButtonEnabled(true);
    connect(urlLineEdit, &QLineEdit::textChanged,
            this, &HttpWindow::enableDownloadButton);
    formLayout->addRow(tr("&URL:"), urlLineEdit);
    QString downloadDirectory = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (downloadDirectory.isEmpty() || !QFileInfo(downloadDirectory).isDir())
        downloadDirectory = QDir::currentPath();
    downloadDirectoryLineEdit->setText(QDir::toNativeSeparators(downloadDirectory));
    formLayout->addRow(tr("&Download directory:"), downloadDirectoryLineEdit);
    formLayout->addRow(tr("Default &file:"), defaultFileLineEdit);
    launchCheckBox->setChecked(true);
    formLayout->addRow(launchCheckBox);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(formLayout);

    mainLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Ignored, QSizePolicy::MinimumExpanding));

    statusLabel->setWordWrap(true);
    mainLayout->addWidget(statusLabel);

    downloadButton->setDefault(true);
    connect(downloadButton, &QAbstractButton::clicked, this, &HttpWindow::downloadFile);
    QPushButton *quitButton = new QPushButton(tr("Quit"));
    quitButton->setAutoDefault(false);
    connect(quitButton, &QAbstractButton::clicked, this, &QWidget::close);
    QDialogButtonBox *buttonBox = new QDialogButtonBox;
    buttonBox->addButton(downloadButton, QDialogButtonBox::ActionRole);
    buttonBox->addButton(quitButton, QDialogButtonBox::RejectRole);
    mainLayout->addWidget(buttonBox);

    downloadDirectoryLineEdit->setText("F:\\Desktop\\pic");
    urlLineEdit->setFocus();

    getDownloadFileSize();
}

void HttpWindow::startRequest(const QUrl &requestedUrl)
{
    qDebug() << __FUNCTION__
             << "\ntoString:" << requestedUrl.toString()
             << "\ntoDisplayString:" << requestedUrl.toDisplayString()
             << "\nfragment:" << requestedUrl.fragment()
             << "\nfileName:" << requestedUrl.fileName()
             << "\npath:" << requestedUrl.path()
             << "\nhost:" << requestedUrl.host()
             << "\nport:" << requestedUrl.port()
             << "\npassword:" << requestedUrl.password()
             << "\nuserName:" << requestedUrl.userName()
             << "\nurl:" << requestedUrl.url();
    url = requestedUrl;
    httpRequestAborted = false;

    QNetworkRequest q(url);

    if (url.toString().startsWith("https://"))
    {
        QSslConfiguration config;

        config.setPeerVerifyMode(QSslSocket::VerifyNone);
//        config.setProtocol(QSsl::SslV3);
        q.setSslConfiguration(config);
    }


    QByteArray rangevalue = QString("bytes=%1-").arg(_downloadFileSize).toUtf8();
    q.setRawHeader("Range", rangevalue);
    //qDebug() << __FUNCTION__ << "Range:" << rangevalue;


    reply = qnam.get(q);
    connect(reply, &QNetworkReply::finished, this, &HttpWindow::httpFinished);
    connect(reply, &QIODevice::readyRead, this, &HttpWindow::httpReadyRead);

    ProgressDialog *progressDialog = new ProgressDialog(url, this);
    progressDialog->setOffsize(_downloadFileSize);
    progressDialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(progressDialog, &QProgressDialog::canceled, this, &HttpWindow::cancelDownload);
    connect(reply, &QNetworkReply::downloadProgress, progressDialog, &ProgressDialog::networkReplyProgress);
    connect(reply, &QNetworkReply::finished, progressDialog, &ProgressDialog::hide);
    progressDialog->show();

    statusLabel->setText(tr("Downloading %1...").arg(url.toString()));
}

void HttpWindow::downloadFile()
{
    const QString urlSpec = urlLineEdit->text().trimmed();
    if (urlSpec.isEmpty())
        return;

    const QString fileSpec = defaultFileLineEdit->text().trimmed();
    if (fileSpec.isEmpty())
        return;

    const QString url(urlSpec + fileSpec);

    const QUrl newUrl = QUrl::fromUserInput(url);
    if (!newUrl.isValid()) {
        QMessageBox::information(this, tr("Error"),
                                 tr("Invalid URL: %1: %2").arg(urlSpec, newUrl.errorString()));
        return;
    }

    QString fileName = newUrl.fileName();
    if (fileName.isEmpty())
        fileName = defaultFileLineEdit->text().trimmed();
    if (fileName.isEmpty())
        fileName = defaultFileName;
    QString downloadDirectory = QDir::cleanPath(downloadDirectoryLineEdit->text().trimmed());
    if (!downloadDirectory.isEmpty() && QFileInfo(downloadDirectory).isDir())
        fileName.prepend(downloadDirectory + '/');
    if (QFile::exists(fileName)
            && _allFileSize == _downloadFileSize) {
//        if (QMessageBox::question(this, tr("Overwrite Existing File"),
//                                  tr("There already exists a file called %1 in "
//                                     "the current directory. Overwrite?").arg(fileName),
//                                  QMessageBox::Yes|QMessageBox::No, QMessageBox::No)
//            == QMessageBox::No)
//            return;
        setDownloadFileSize(fileName, 0);
        setAllFileSize(fileName, 0);
        QFile::remove(fileName);
    }

    _fileName = fileName;

    file = openFileForWrite(fileName);
    if (!file)
        return;

    downloadButton->setEnabled(false);

    // schedule the request
    startRequest(newUrl);
}

QFile *HttpWindow::openFileForWrite(const QString &fileName)
{
    QScopedPointer<QFile> file(new QFile(fileName));
    if (!file->open(QIODevice::WriteOnly | QIODevice::Append))
    {
        QMessageBox::information(this, tr("Error"),
                                 tr("Unable to save the file %1: %2.")
                                 .arg(QDir::toNativeSeparators(fileName),
                                      file->errorString()));
        return Q_NULLPTR;
    }
    return file.take();
}

void HttpWindow::cancelDownload()
{
    QList<QByteArray> testlist = reply->rawHeaderList();
    foreach(QByteArray a, testlist)
    {
        if (!a.isEmpty()){
            qDebug() << __FUNCTION__ << a;
            if (a == "Content-Range") {
                qDebug() << __FUNCTION__ << "Content-Range:" << reply->rawHeader("Content-Range");
            }
            else if (a == "Content-Length") {
                qDebug() << __FUNCTION__ << reply->rawHeader("Content-Length");
            }
        }
    }


    QByteArray contentsize = reply->rawHeader("Content-Length");
    if (!contentsize.isEmpty()) {
        _allFileSize = contentsize.toULongLong();
        qDebug() << __FUNCTION__ << "Content-Length" << _allFileSize;
    }

    statusLabel->setText(tr("Download canceled."));
    httpRequestAborted = true;
    reply->abort();
    downloadButton->setEnabled(true);


    if (!_fileName.isEmpty()) {
        setDownloadFileSize(_fileName, _downloadFileSize);
        setAllFileSize(_fileName, _allFileSize);
    }
}

void HttpWindow::httpFinished()
{
    QFileInfo fi;
    if (file) {
        fi.setFile(file->fileName());
        file->close();
        delete file;
        file = Q_NULLPTR;
    }

    if (httpRequestAborted) {
        reply->deleteLater();
        reply = Q_NULLPTR;
        return;
    }

    if (reply->error()) {
        QFile::remove(fi.absoluteFilePath());
        statusLabel->setText(tr("Download failed:\n%1.").arg(reply->errorString()));
        downloadButton->setEnabled(true);
        reply->deleteLater();
        reply = Q_NULLPTR;
        return;
    }

    const QVariant redirectionTarget = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);

    reply->deleteLater();
    reply = Q_NULLPTR;

    if (!redirectionTarget.isNull()) {
        const QUrl redirectedUrl = url.resolved(redirectionTarget.toUrl());
        if (QMessageBox::question(this, tr("Redirect"),
                                  tr("Redirect to %1 ?").arg(redirectedUrl.toString()),
                                  QMessageBox::Yes | QMessageBox::No) == QMessageBox::No) {
            downloadButton->setEnabled(true);
            return;
        }
        file = openFileForWrite(fi.absoluteFilePath());
        if (!file) {
            downloadButton->setEnabled(true);
            return;
        }
        startRequest(redirectedUrl);
        return;
    }

    setDownloadFileSize(fi.fileName(), _allFileSize);
    setAllFileSize(fi.fileName(), _allFileSize);

    statusLabel->setText(tr("Downloaded %1 bytes to %2\nin\n%3")
                         .arg(fi.size()).arg(fi.fileName(), QDir::toNativeSeparators(fi.absolutePath())));
    if (launchCheckBox->isChecked())
        QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absoluteFilePath()));
    downloadButton->setEnabled(true);
}

void HttpWindow::httpReadyRead()
{
    // this slot gets called every time the QNetworkReply has new data.
    // We read all of its new data and write it into the file.
    // That way we use less RAM than when reading it at the finished()
    // signal of the QNetworkReply

    static int scount = 0;
    //qDebug() << __FUNCTION__ << reply->size();
    if (file) {
        QByteArray content = reply->readAll();
        //qDebug() << __FUNCTION__ << "pos:" << file->pos();
        int&& size = file->write(content);
        if (-1 != size) {
            _downloadFileSize += size;
        } else {
            qDebug() << __FUNCTION__ << "error:" << "write data to file.";
        }
        if (1 == ++scount)
            qDebug() << __FUNCTION__ << content;
    }
}

void HttpWindow::enableDownloadButton()
{
    downloadButton->setEnabled(!urlLineEdit->text().isEmpty());
}

void HttpWindow::slotAuthenticationRequired(QNetworkReply*,QAuthenticator *authenticator)
{
    QDialog authenticationDialog;
    Ui::Dialog ui;
    ui.setupUi(&authenticationDialog);
    authenticationDialog.adjustSize();
    ui.siteDescription->setText(tr("%1 at %2").arg(authenticator->realm(), url.host()));

    // Did the URL have information? Fill the UI
    // This is only relevant if the URL-supplied credentials were wrong
    ui.userEdit->setText(url.userName());
    ui.passwordEdit->setText(url.password());

    if (authenticationDialog.exec() == QDialog::Accepted) {
        authenticator->setUser(ui.userEdit->text());
        authenticator->setPassword(ui.passwordEdit->text());
    }
}

void HttpWindow::setDownloadFileSize(const QString& fileName, quint64 size)
{
    QSettings set("d:\\file_tmp", QSettings::IniFormat);
    set.setValue("file_name", fileName);
    set.setValue("file_download_size", size);
}

void HttpWindow::setAllFileSize(const QString& fileName, quint64 size)
{
    QSettings set("d:\\file_tmp", QSettings::IniFormat);
    set.setValue("file_name", fileName);
    set.setValue("file_all_size", size);
}

quint64 HttpWindow::getDownloadFileSize()
{
    QSettings set("d:\\file_tmp", QSettings::IniFormat);
    qDebug() << __FUNCTION__ << set.fileName();
    QVariant size = set.value("file_download_size");
    QVariant allsize = set.value("file_all_size");
    QVariant name = set.value("file_name");
    if (size.isValid() && name.isValid()
            && file && file->fileName() == name
            && allsize != size) {
        _downloadFileSize = size.toULongLong();
    } else {
        _downloadFileSize = 0;
    }
    return _downloadFileSize;
}


#ifndef QT_NO_SSL
void HttpWindow::sslErrors(QNetworkReply*,const QList<QSslError> &errors)
{
    QString errorString;
    foreach (const QSslError &error, errors) {
        if (!errorString.isEmpty())
            errorString += '\n';
        errorString += error.errorString();
    }

    if (QMessageBox::warning(this, tr("SSL Errors"),
                             tr("One or more SSL errors has occurred:\n%1").arg(errorString),
                             QMessageBox::Ignore | QMessageBox::Abort) == QMessageBox::Ignore) {
        reply->ignoreSslErrors();
    }
}
#endif
