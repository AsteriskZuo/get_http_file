#include "get_http_file.h"
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QAuthenticator>
#include <QSslError>
#include <QByteArray>
#include <QFile>
#include <QSqlDatabase>

#ifndef QT_FORCE_ASSERTS
# define QT_FORCE_ASSERTS
#endif //QT_FORCE_ASSERTS

GetHttpFile::GetHttpFile(const HttpFileInfo& info, QObject *parent)
    : QObject(parent)
    , _info(info)
    , _pReply(0)
    , _pFile(0)
    , _pDb(0)
{

}

void GetHttpFile::start()
{
    /*
     * todo:有效性判断
     */

    _info.url = _info.url.trimmed();//remove space
    const QUrl url = QUrl::fromUserInput(_info.url);
    if (!url.isValid()) {
        emit sendError(4, QObject::tr("Invalid URL: %1: %2").arg(_info.url).arg(url.errorString()));
        return;
    }

    if (_info.dir.isEmpty()){
        emit sendError(4, QObject::tr("Please input download file dir."));
        return;
    }

    if (_info.name.isEmpty()){
        emit sendError(4, QObject::tr("Please input download file name."));
        return;
    }

    if (!openFile()) {
        emit sendError(4, QObject::tr("Open file %1 is error.").arg(_info.dir + _info.name));
        return;
    }

    /*
     * todo:下载
     */

    QNetworkRequest q(url);

    if (url.toString().startsWith("https://"))
    {
        QSslConfiguration config;
        config.setPeerVerifyMode(QSslSocket::VerifyNone);
//        config.setProtocol(QSsl::SslV3);
        q.setSslConfiguration(config);
    }

    QByteArray rangevalue = QString("bytes=%1-").arg(_info.curSize).toUtf8();
    q.setRawHeader("Range", rangevalue);

#ifndef QT_NO_SSL
    connect(_pQnam, &QNetworkAccessManager::sslErrors, this, &GetHttpFile::sslErrors);
#endif
    connect(_pQnam, &QNetworkAccessManager::authenticationRequired, this, &GetHttpFile::authenticationRequired);
    connect(_pReply, &QNetworkReply::readyRead, this, &GetHttpFile::readyRead);

}

void GetHttpFile::stop()
{
    /*
     * todo:停止下载
     */


}

void GetHttpFile::restart()
{
    /*
     * todo:重置信息
     * todo:删除文件
     */

    /*
     * todo:开始下载
     */

    start();
}

void GetHttpFile::init()
{
    _pQnam = new QNetworkAccessManager(this);
    Q_ASSERT(_pQnam);

}

bool GetHttpFile::openFile()
{
    return false;
}

void GetHttpFile::closeFile()
{

}

bool GetHttpFile::openDb()
{
    return false;
}

void GetHttpFile::closeDb()
{

}

bool GetHttpFile::addFileInfo(const HttpFileInfo &fileInfo)
{
    return false;
}

bool GetHttpFile::deleteFileInfoById(const QString &id)
{
    return false;
}

bool GetHttpFile::updateFileInfo(const HttpFileInfo &fileInfo)
{
    return false;
}

bool GetHttpFile::getFileInfoById(const QString &id, HttpFileInfo &fileInfo)
{
    return false;
}

void GetHttpFile::readyRead()
{

}

void GetHttpFile::authenticationRequired(QNetworkReply *reply, QAuthenticator *authenticator)
{

}

void GetHttpFile::sslErrors(QNetworkReply *reply, const QList<QSslError> &errors)
{

}
