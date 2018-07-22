#include "get_http_file.h"
#include "common_define.h"

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
#include <QDir>
#include <QDesktopServices>
#include <QTimer>

#ifndef QT_FORCE_ASSERTS
# define QT_FORCE_ASSERTS
#endif //QT_FORCE_ASSERTS


GetHttpFile::GetHttpFile(const HttpFileInfo& info, QObject *parent)
    : QObject(parent)
    , _info(info)
    , _pReply(nullptr)
    , _pFile(nullptr)
    , _pDb(nullptr)
    , _pQnam(nullptr)
    , _isManualCancel(false)
    , _isContinue(false)
    , _curSize(0)
    , _allSize(0)
    , _isOpenAfterFinished(false)
    , _pSpeed(nullptr)
    , _speedUnit(0)
	, _speedDuration(1000)
	, _pConnectTimeout(nullptr)
{
    this->moveToThread(&_thisThread);
    connect(&_thisThread, &QThread::started, this, &GetHttpFile::threadStartedSlot, Qt::QueuedConnection);
    connect(&_thisThread, &QThread::finished, this, &GetHttpFile::threadFinishedSlot, Qt::DirectConnection);
    _thisThread.start();
}

GetHttpFile::~GetHttpFile()
{
    if (_thisThread.isRunning()) {
        _thisThread.quit();
        _thisThread.wait();
    }
}

void GetHttpFile::updateInfo(const HttpFileInfo &info)
{
    _info.url = info.url.trimmed();
    _info.dir = info.dir.trimmed();
    _info.name = info.name.trimmed();
    _id = getId();
}

bool GetHttpFile::isFinished()
{
    if (_info.curSize == _info.allSize && _info.curSize
            && QFile::exists(getFullFileName())){
        return true;
    }
    return false;
}


void GetHttpFile::innerStart()
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

    if (!QDir(_info.dir).exists()) {
        emit sendError(4, QObject::tr("dir is not exist."));
        return;
    }

    if (_info.name.isEmpty()){
        emit sendError(4, QObject::tr("Please input download file name."));
        return;
    }

    if (_info.name.contains(' ')){
        emit sendError(4, QObject::tr("file name contains space char."));
        return;
    }

    if (!openFile()) {
        emit sendError(4, QObject::tr("Open file %1 is error.").arg(_info.dir + _info.name));
        return;
    }

    /*
     * todo: start download
     */

    if (!addFileInfo(_info)){
        emit sendError(4, tr("add file info failed."));
    }

    startCalculateSpeed();

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

    _pReply = _pQnam->get(q);
    Q_ASSERT(_pReply);

	_pConnectTimeout->setParam(_pReply, 10000);
	_pConnectTimeout->startTimer();

    connect(_pReply, &QNetworkReply::readyRead, this, &GetHttpFile::readyReadSlot);
    connect(_pReply, &QNetworkReply::channelReadyRead, this, &GetHttpFile::channelReadyReadSlot);
    connect(_pReply, &QNetworkReply::downloadProgress, this, &GetHttpFile::downloadProgressSlot);
    connect(_pReply, &QNetworkReply::finished, this, &GetHttpFile::finishedSlot);
}

void GetHttpFile::start()
{
    if (_info.curSize) _isContinue = true;//Breakpoint Continuation
    else _isContinue = false;
    innerStart();
}

void GetHttpFile::stop()
{
    /*
     * todo: stop file downlaod :
     * two case:
     * 1._pReply is created
     * 2._pReply is not created
     */

    if (_pReply) {
		_isManualCancel = true;
		_pReply->abort();
    } else {
        emit cancelDownload(1);
    }
}

void GetHttpFile::restart(bool isUserClick)
{
    /*
     * todo:reset info
     * todo:remove file
     */

    reset();
    if (isUserClick) _info.actualurl = _info.url;

    if (QFile::exists(getFullFileName())) {
        if (!QFile::remove(getFullFileName())) {
            emit sendError(4, QObject::tr("An error occurred deleting the file %1.").arg(getFullFileName()));
            return;
        }
    }

    /*
     * todo: start download file...
     */

    _isContinue = false;//no Breakpoint Continuation
    innerStart();
}

void GetHttpFile::init()
{
    if (!_pQnam)
        _pQnam = new QNetworkAccessManager(this);
    Q_ASSERT(_pQnam);

    _info.url = _info.url.trimmed();
    _info.dir = _info.dir.trimmed();
    _info.name = _info.name.trimmed();

    _id = getId();

	if (!_pConnectTimeout)
		_pConnectTimeout = new ConnectTimeout(this);
	Q_ASSERT(_pConnectTimeout);

    QString tmpdir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    _dbName = tmpdir + "/" + QApplication::instance()->applicationName();

    if (!openDb()) {
        emit sendError(4, QObject::tr("Open db file %1 is error.").arg(_dbName));
        return;
    }

    QString oldUrl = _info.url;
    QString oldActualUrl = _info.actualurl;
    QString oldMd5 = _info.md5;
    QString oldLastModify = _info.lastModify;
    if (getFileInfoById(_id, _info)) {
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

void GetHttpFile::uninit()
{
    closeDb();
    closeFile();
    if (_pReply){
        qCritical() << __FUNCTION__ << "Please first stop download file.";
        _pReply->deleteLater();
        _pReply = 0;
    }
}

void GetHttpFile::reset()
{
    _info.reset();
}


void GetHttpFile::readyReadSlot()
{
    QByteArray&& content = _pReply->readAll();
    int&& size = _pFile->write(content);
    if (-1 == size){
        emit sendError(5, tr("write file is error. size is %1").arg(size));
        _pReply->abort();
        return;
    }
    Q_ASSERT(size == content.size());//test

    if (!_info.curSize && !_isContinue){
        setFileInfoFromSvr();
    }

    _info.curSize += size;
    _speedUnit += size;

    emit downloadProgress(_info.curSize, _info.allSize);
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
    _curSize = bytesReceived;
    _allSize = bytesTotal;
//    emit downloadProgress(_info.curSize, bytesTotal);
}

void GetHttpFile::finishedSlot()
{
    closeFile();
    stopCalculateSpeed();

    if (_isManualCancel) {
        _isManualCancel = false;
//        setFileInfoFromSvr();
        if (!updateFileInfo(_info)) {
            emit sendError(4, tr("update file info failed."));
        }

        _pReply->deleteLater();
        _pReply = 0;

        emit cancelDownload(1);
        return;
    }

	if (_pConnectTimeout->isTimeoutCancel()){
//        setFileInfoFromSvr();
		if (!updateFileInfo(_info)) {
			emit sendError(4, tr("update file info failed."));
		}

		_pReply->deleteLater();
		_pReply = 0;

		emit cancelDownload(2);
		return;
	}

    QNetworkReply::NetworkError err = _pReply->error();
    if (QNetworkReply::NoError == err) {
        const QVariant redirectionTarget = _pReply->attribute(QNetworkRequest::RedirectionTargetAttribute);
        if (!redirectionTarget.isNull()) {
            const QUrl redirectedUrl = QUrl(_info.actualurl).resolved(redirectionTarget.toUrl());
            _info.actualurl = redirectedUrl.toString();
            emit sendError(1, tr("Actual url is %1.").arg(_info.actualurl));
            restart(false);
            return;
        }

//        setFileInfoFromSvr();
        Q_ASSERT(_info.allSize == _info.curSize);
        if (!updateFileInfo(_info)) {
            emit sendError(4, tr("update file info failed."));
        }

        _pReply->deleteLater();
        _pReply = 0;

        emit finished();

        openDownloadFile();
    } else {
		QString errString = QString::number(_pReply->error()) + _pReply->errorString();

		if (!updateFileInfo(_info)) {
			emit sendError(4, tr("update file info failed."));
		}

        _pReply->deleteLater();
        _pReply = 0;

        emit sendError(3, errString);
    }
}

void GetHttpFile::channelReadyReadSlot(int channel)
{
    qDebug() << __FUNCTION__ << channel;
}

void GetHttpFile::threadStartedSlot()
{
    qDebug() << __FUNCTION__;
    init();
}

void GetHttpFile::threadFinishedSlot()
{
    qDebug() << __FUNCTION__;
    uninit();
}

void GetHttpFile::calSpeed()
{
    float result;
	qint64 cu = (_speedUnit * ((double)1000.0 / _speedDuration));
    int count = 1;//init byte
    while (1024 < cu){
        qint64 tu = cu;
        tu >>= 10;
        if (1024 < tu){
            cu = tu;
            ++count;
        } else {
            result = (float)cu / 1024;
            ++count;
            break;
        }
    }
    _speedUnit = 0;//reset
    emit sendSpeed(result, count);
}


bool GetHttpFile::openFile()
{
    if (!_pFile)
        _pFile = new QFile();
    Q_ASSERT(_pFile);
    if (_pFile->isOpen())
        _pFile->close();
    _pFile->setFileName(getFullFileName());
    if (!_pFile->open(QIODevice::Append | QIODevice::WriteOnly)) {
        qWarning() << __FUNCTION__ << "open file is error. file name is" << getFullFileName();
        return false;
    }
    return true;
}

void GetHttpFile::closeFile()
{
    if (_pFile) {
        if (_pFile->isOpen())
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

    if (!_pDb || !_pDb->isOpen()) return false;

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
        //Patch created special versions
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

        oldVersion = _q.value("version").toInt();
        if (oldVersion < newVersion)
        {
            if (!_pDb->transaction())
            {
                qWarning() << __FUNCTION__ << _pDb->lastError();
                return false;
            }


            int tmpVersion = oldVersion;
            while (tmpVersion < newVersion)
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
        //Fully created all versions
        if (!_pDb->transaction())
        {
            qWarning() << __FUNCTION__ << _pDb->lastError();
            return false;
        }

        int tmpVersion = 0;
        while (tmpVersion <= newVersion)
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
        if (!_pDb || !_pDb->isOpen()) return false;
        QSqlQuery q(*_pDb);
        QString sqlstmt = "CREATE TABLE IF NOT EXISTS _version ( \
            version INTEGER PRIMARY KEY NOT NULL, update_content VARCHAR (1000) \
            NOT NULL, update_datetime INTEGER, update_staff VARCHAR (256))";
        q.prepare(sqlstmt);
        if (!q.exec()){
            qWarning() << __FUNCTION__ << q.lastError() << q.lastQuery();
            return false;
        }

        sqlstmt = "CREATE TABLE IF NOT EXISTS http_file_info \
            ( id VARCHAR (32) NOT NULL PRIMARY KEY, \
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

        sqlstmt = "REPLACE INTO _version ( "
                  "version, update_content, update_datetime, update_staff ) "
                  "VALUES (0, 'New to create a database tables.', 1531471009, 'zuoyu')";
        q.prepare(sqlstmt);
        if (!q.exec()){
            qWarning() << __FUNCTION__ << q.lastError() << q.lastQuery();
            return false;
        }
    }
    else if (1 == version) {
        if (!_pDb || !_pDb->isOpen()) return false;
        return false;
    }
    return true;
}

bool GetHttpFile::openDb()
{
    if (!_pDb)
        _pDb = new QSqlDatabase();
    if (!_pDb->isOpen()){
        *_pDb = QSqlDatabase::addDatabase("QSQLITE", "test");
        _pDb->setHostName("127.0.0.1");
        _pDb->setDatabaseName(_dbName);
        _pDb->setUserName(QApplication::instance()->applicationName());
        _pDb->setPassword(QApplication::instance()->applicationName());
        if (!_pDb->open()) {
            qWarning() << __FUNCTION__ << "DB file open is failed." << _dbName;
            return false;
        }
        if (!_createTabel()) {
            _pDb->close();
            qWarning() << __FUNCTION__ << "Create or open tabel is failed.";
            return false;
        }
    }
    return true;
}

void GetHttpFile::closeDb()
{
    if (_pDb) {
        if (_pDb->isOpen())
            _pDb->close();
        QSqlDatabase::removeDatabase(_pDb->connectionName());
        delete _pDb;
        _pDb = 0;
    }
}

bool GetHttpFile::addFileInfo(const HttpFileInfo &fileInfo)
{
    if (!_pDb || !_pDb->isOpen()) return false;
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
    if (!_pDb || !_pDb->isOpen()) return false;
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
    if (!_pDb || !_pDb->isOpen()) return false;
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
    if (!_pDb || !_pDb->isOpen()) return false;
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

QString GetHttpFile::getId()
{
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(_info.dir.toUtf8());
    hash.addData(_info.name.toUtf8());
    hash.addData(_info.url.toUtf8());
    return QString(hash.result().toHex());
}

QString GetHttpFile::getFullFileName()
{
    QString fullFileName;
    if ("\\" != _info.dir.right(1) && "/" != _info.dir.right(1))
        fullFileName = _info.dir + "/" + _info.name;
    else
        fullFileName = _info.dir + _info.name;
    return std::move(fullFileName);
}

void GetHttpFile::setFileInfoFromSvr()
{
    qDebug() << __FUNCTION__ ;
    if (!_pReply) return;

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
}

bool GetHttpFile::openDownloadFile()
{
	if (_isOpenAfterFinished)
	{
		return QDesktopServices::openUrl(QUrl::fromLocalFile(getFullFileName()));
	}
	return true;
}

void GetHttpFile::startCalculateSpeed()
{
    if (!_pSpeed){
        _pSpeed = new QTimer(this);//ownship is this object, so manual delete it.
        Q_ASSERT(_pSpeed);
        connect(_pSpeed, &QTimer::timeout, this, &GetHttpFile::calSpeed);
    }
    if (!_pSpeed->isActive())
		_pSpeed->start(_speedDuration);
}

void GetHttpFile::stopCalculateSpeed()
{
    if (_pSpeed && _pSpeed->isActive()){
        _pSpeed->stop();
    }
}

