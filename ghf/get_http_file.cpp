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
#include <QApplication>
#include <QStandardPaths>
#include <QDebug>
#include <QSqlQuery>
#include <QCryptographicHash>
#include <QSqlError>

#ifndef QT_FORCE_ASSERTS
# define QT_FORCE_ASSERTS
#endif //QT_FORCE_ASSERTS

GetHttpFile::GetHttpFile(const HttpFileInfo& info, QObject *parent)
    : QObject(parent)
    , _info(info)
    , _pReply(0)
    , _pFile(0)
    , _pDb(0)
    , _isManualCancel(false)
{
    init();
}

GetHttpFile::~GetHttpFile()
{
    closeDb();
    closeFile();
    if (_pReply){
        _pReply->deleteLater();
        _pReply = 0;
    }
}

void GetHttpFile::start()
{
    /*
     * todo:check...
     */

    const QUrl url = QUrl::fromUserInput(_info.actualurl);
    if (!url.isValid()) {
        emit sendError(4, QObject::tr("Invalid URL: %1: %2").arg(_info.actualurl).arg(url.errorString()));
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
     * todo: start download
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
    connect(_pQnam, &QNetworkAccessManager::sslErrors, this, &GetHttpFile::sslErrorsSlot);
#endif
    connect(_pQnam, &QNetworkAccessManager::authenticationRequired, this, &GetHttpFile::authenticationRequiredSlot);
    connect(_pReply, &QNetworkReply::readyRead, this, &GetHttpFile::readyReadSlot);
    connect(_pReply, &QNetworkReply::downloadProgress, this, &GetHttpFile::downloadProgressSlot);
    connect(_pReply, &QNetworkReply::finished, this, &GetHttpFile::finishedSlot);

    _pReply = _pQnam->get(q);
    Q_ASSERT(_pReply);
}

void GetHttpFile::stop()
{
    /*
     * todo: stop file downlaod :
     * two case:
     * 1._pReply is created
     * 2._pReply is not created
     */

    _isManualCancel = true;
    if (_pReply) {
        _pReply->abort();
    } else {
        closeFile();
        emit cancelDownload();
    }
}

void GetHttpFile::restart()
{
    /*
     * todo:reset info
     * todo:remove file
     */

    if (QFile::exists(_info.dir + _info.name)) {
        if (!QFile::remove(_info.dir + _info.name)) {
            emit sendError(4, QObject::tr("An error occurred deleting the file %1.").arg(_dbName));
            return;
        }
    }

    reset();

    /*
     * todo: start download file...
     */

    start();
}

void GetHttpFile::init()
{
    _pQnam = new QNetworkAccessManager(this);
    Q_ASSERT(_pQnam);

    _info.url = _info.url.trimmed();
    _info.dir = _info.dir.trimmed();
    _info.name = _info.name.trimmed();
    _info.actualurl = _info.url;

    _id = _getId();

    QString tmpdir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    _dbName = tmpdir + QApplication::instance()->applicationName();

    if (!openDb()) {
        emit sendError(4, QObject::tr("Open db file %1 is error.").arg(_dbName));
        return;
    }

    QString oldUrl = _info.url;
    QString oldActualUrl = _info.actualurl;
    QString oldMd5 = _info.md5;
    QString oldLastModify = _info.lastModify;
    if (!getFileInfoById(_id, _info)) {
        if (oldUrl != _info.url) {
            _info.curSize = 0;
        } else if (oldActualUrl != _info.actualurl) {
            _info.curSize = 0;
        } else if (oldMd5 != _info.md5) {
            _info.curSize = 0;
        } else if (oldLastModify != _info.lastModify) {
            _info.curSize = 0;
        }
    } else {
        _info.curSize = 0;
    }
}

void GetHttpFile::reset()
{
    _info.reset();
    _isManualCancel = false;
}


void GetHttpFile::readyReadSlot()
{
    QByteArray&& content = _pReply->readAll();
    int&& size = _pFile->write(content);
    if (-1 == size){
        emit sendError(4, tr("write file is error. size is %1").arg(size));
        _pReply->abort();
        return;
    }
    Q_ASSERT(size == content.size());//test
    _info.curSize += size;
}

void GetHttpFile::authenticationRequiredSlot(QNetworkReply *reply, QAuthenticator *authenticator)
{
    /*
     * todo:add user id and password
     */

    Q_UNUSED(reply);
    Q_UNUSED(authenticator);

}

void GetHttpFile::sslErrorsSlot(QNetworkReply *reply, const QList<QSslError> &errors)
{
    /*
     * todo: ssl error output
     */

    Q_UNUSED(reply);

    QString errorString;
    foreach (const QSslError &error, errors) {
        if (!errorString.isEmpty())
            errorString += '\n';
        errorString += error.errorString();
    }

    emit sendError(4, QObject::tr("ssl error is %1.").arg(errorString));
}

void GetHttpFile::downloadProgressSlot(qint64 bytesReceived, qint64 bytesTotal)
{
    qDebug() << __FUNCTION__
             << "bytesReceived:" << bytesReceived
             << "bytesTotal:" << bytesTotal
             << "curSize:" << _info.curSize
             << "allSize:" << _info.allSize;
    emit downloadProgress(_info.curSize, bytesTotal);
}

void GetHttpFile::finishedSlot()
{
    closeFile();

    if (_isManualCancel) {
        _pReply->deleteLater();
        _pReply = 0;
        emit cancelDownload();
        return;
    }

    QNetworkReply::NetworkError err = _pReply->error();
    if (QNetworkReply::NoError == err) {
        const QVariant redirectionTarget = _pReply->attribute(QNetworkRequest::RedirectionTargetAttribute);
        if (!redirectionTarget.isNull()) {
            const QUrl redirectedUrl = QUrl(_info.actualurl).resolved(redirectionTarget.toUrl());
            _info.actualurl = redirectedUrl.toString();
            emit sendError(1, tr("Actual url is %1.").arg(_info.actualurl));
            restart();
            return;
        }

        QByteArray allSize = _pReply->rawHeader("Content-Length");
        if (!allSize.isEmpty()) {
            _info.allSize = allSize.toULongLong();
        }
        QByteArray etag = _pReply->rawHeader("Etag");
        if (!etag.isEmpty()) {
            _info.md5 = etag;
        }
        QByteArray lastModify = _pReply->rawHeader("Last-Modified");
        if (!lastModify.isEmpty()) {
            _info.lastModify = lastModify;
        }
        if (!updateFileInfo(_info)) {
            emit sendError(4, tr("update file info failed."));
        }

        _pReply->deleteLater();
        _pReply = 0;

        emit finished();

    } else {
        QString errString = _pReply->errorString();

        _pReply->deleteLater();
        _pReply = 0;

        emit sendError(3, errString);
    }
}


bool GetHttpFile::openFile()
{
    if (!_pFile)
        _pFile = new QFile(_info.dir + _info.name);
    Q_ASSERT(_pFile);
    if (!_pFile->open(QIODevice::Append | QIODevice::WriteOnly)) {
        qWarning() << __FUNCTION__ << "open file is error. file name is" << _info.dir + _info.name;
        return false;
    }
    return true;
}

void GetHttpFile::closeFile()
{
    if (_pFile) {
        _pFile->close();
        delete _pFile;
        _pFile = 0;
    }
}

bool GetHttpFile::_createTabel()
{
    /*
     * 0：create tabels on 2018.07.12
     */

    int oldVersion = 0;
    int newVersion = 0;


    QSqlQuery q(*_pDb);
    QString sqlstmt("SELECT tbl_name FROM sqlite_master WHERE type='table' AND tbl_name='_version'");
    q.prepare(sqlstmt);
    if (!q.exec())
    {
        qWarning() << __FUNCTION__ << q.lastError();
        return false;
    }
    if (q.first())
    {
        QSqlQuery _q(*_pDb);
        sqlstmt = "SELECT version FROM _version ORDER BY update_datetime DESC LIMIT 1";
        _q.prepare(sqlstmt);
        if (!_q.exec())
        {
            qWarning() << __FUNCTION__ << _q.lastError();
            return false;
        }

        if (!_q.first())
        {
            qWarning() << __FUNCTION__ << "Not the case in theory.";
            return false;
        }

        int oldVersion = _q.value("version").toInt();
        if (oldVersion < newVersion)
        {
            if (!_pDb->transaction())
            {
                qWarning() << __FUNCTION__ << _pDb->lastError();
                return false;
            }


            int tmpVersion = oldVersion;
            while (tmpVersion != newVersion)
            {
                bool _rt = _createVersionTabel(tmpVersion);
                if (!_rt)
                {
                    qWarning() << __FUNCTION__ << "Failed to create the database.";
                    _pDb->rollback();
                    return false;
                }
                ++tmpVersion;
            }


            if (!_pDb->commit())
            {
                _pDb->rollback();
                qWarning() << __FUNCTION__ << _pDb->lastError();
                return false;
            }

        }
        else if (oldVersion > newVersion)
        {
            qWarning() << __FUNCTION__ << "Not the case in theory." << oldVersion << newVersion;
            return true;
        }
        else
        {
            return true;
        }
    }
    else
    {
        if (!_pDb->transaction())
        {
            qWarning() << __FUNCTION__ << _pDb->lastError();
            return false;
        }

        int tmpVersion = oldVersion;
        while (tmpVersion != newVersion)
        {
            bool _rt = _createVersionTabel(tmpVersion);
            if (!_rt)
            {
                qWarning() << __FUNCTION__ << "Failed to create the database.";
                _pDb->rollback();
                return false;
            }
            ++tmpVersion;
        }

        if (!_pDb->commit())
        {
            _pDb->rollback();
            qWarning() << __FUNCTION__ << _pDb->lastError();
            return false;
        }
    }

    return true;
}

bool GetHttpFile::_createVersionTabel(const int version)
{
    if (0 == version) {
        QSqlQuery q(*_pDb);
        QString sqlstmt = "CREATE TABLE IF NOT EXISTS http_file_info \
            ( id VARCHAR (16) NOT NULL PRIMARY KEY, \
            dir VARCHAR (64) NOT NULL, \
            name VARCHAR (32) NOT NULL, \
            all_size INTEGER, \
            cur_size INTEGER, \
            md5 VARCHAR (16), \
            last_modify VARCHAR (32), \
            url VARCHAR (260) NOT NULL, \
            actual_url VARCHAR (260))";
        q.prepare(sqlstmt);
        if (!q.exec()){
            qWarning() << __FUNCTION__ << q.lastError() << q.lastQuery();
            return false;
        }
    }
    else if (1 == version) {
        //todo:
        return false;
    }
    return true;
}

bool GetHttpFile::openDb()
{
    if (!_pDb){
        _pDb = new QSqlDatabase();
        Q_ASSERT(_pDb);
        *_pDb = QSqlDatabase::addDatabase("QSQLITE");
        _pDb->setConnectOptions();
        _pDb->setHostName("127.0.0.1");
        _pDb->setDatabaseName(_dbName);
        _pDb->setUserName(QApplication::instance()->applicationName());
        _pDb->setPassword(QApplication::instance()->applicationName());
        if (!_pDb->open()) {
            qWarning() << __FUNCTION__ << "DB file open is failed." << _dbName;
            return false;
        }
        if (!_createTabel()) {
            qWarning() << __FUNCTION__ << "Create or open tabel is failed.";
            return false;
        }
    }
    return true;
}

void GetHttpFile::closeDb()
{
    if (_pDb) {
        QSqlDatabase::cloneDatabase(*_pDb, _pDb->connectionName());
        delete _pDb;
        _pDb = 0;
    }
}

bool GetHttpFile::addFileInfo(const HttpFileInfo &fileInfo)
{
    if (!_pDb) return false;
    QSqlQuery q(*_pDb);
    QString sqlstmt = "REPLACE INTO http_file_info (\
        id, dir, name, all_size, cur_size, md5, last_modify, url, actual_url ) \
        VALUES (?,?,?,?,?,?,?,?,?)";
    q.prepare(sqlstmt);
    q.bindValue(0, _id);
    q.bindValue(1, fileInfo.dir);
    q.bindValue(2, fileInfo.name);
    q.bindValue(3, fileInfo.allSize);
    q.bindValue(4, fileInfo.curSize);
    q.bindValue(5, fileInfo.md5);
    q.bindValue(6, fileInfo.lastModify);
    q.bindValue(7, fileInfo.url);
    q.bindValue(8, fileInfo.actualurl);
    if (!q.exec()){
        qWarning() << __FUNCTION__ << q.lastError() << q.lastQuery();
        return false;
    }
    return true;
}

bool GetHttpFile::deleteFileInfoById(const QString &id)
{
    if (!_pDb) return false;
    QSqlQuery q(*_pDb);
    QString sqlstmt = "DELETE FROM http_file_info WHERE id = ?";
    q.prepare(sqlstmt);
    q.bindValue(0, id);
    if (!q.exec()){
        qWarning() << __FUNCTION__ << q.lastError() << q.lastQuery() << id;
        return false;
    }
    return true;
}

bool GetHttpFile::updateFileInfo(const HttpFileInfo &fileInfo)
{
    if (!_pDb) return false;
    QSqlQuery q(*_pDb);
    QString sqlstmt = "UPDATE http_file_info SET dir = ?, \
        name = ?, all_size = ?, cur_size = ?, md5 = ?, last_modify = ?, url = ?, actual_url = ? \
        WHERE id = ?";
    q.prepare(sqlstmt);
    q.bindValue(0, fileInfo.dir);
    q.bindValue(1, fileInfo.name);
    q.bindValue(2, fileInfo.allSize);
    q.bindValue(3, fileInfo.curSize);
    q.bindValue(4, fileInfo.md5);
    q.bindValue(5, fileInfo.lastModify);
    q.bindValue(6, fileInfo.url);
    q.bindValue(7, fileInfo.actualurl);
    q.bindValue(8, _id);
    if (!q.exec()){
        qWarning() << __FUNCTION__ << q.lastError() << q.lastQuery();
        return false;
    }
    return true;
}

bool GetHttpFile::getFileInfoById(const QString &id, HttpFileInfo &fileInfo)
{
    if (!_pDb) return false;
    QSqlQuery q(*_pDb);
    QString sqlstmt = "SELECT * FROM http_file_info WHERE id = ?";
    q.prepare(sqlstmt);
    q.bindValue(0, id);
    if (!q.exec()){
        qWarning() << __FUNCTION__ << q.lastError() << q.lastQuery();
        return false;
    }
    if (q.first()) {
        fileInfo.dir = q.value("dir").toString();
        fileInfo.name = q.value("name").toString();
        fileInfo.allSize = q.value("all_size").toULongLong();
        fileInfo.curSize = q.value("cur_size").toULongLong();
        fileInfo.md5 = q.value("md5").toString();
        fileInfo.lastModify = q.value("last_modify").toString();
        fileInfo.url = q.value("url").toString();
        fileInfo.actualurl = q.value("actual_url").toString();
        return true;
    }
    return false;
}

QString GetHttpFile::_getId()
{
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(_info.dir.toUtf8());
    hash.addData(_info.name.toUtf8());
    hash.addData(_info.url.toUtf8());
    return QString(hash.result());
}
