#ifndef GET_HTTP_FILE_H
#define GET_HTTP_FILE_H

#include "common_define.h"

#include <QObject>
#include <QString>
#include <QThread>

class QNetworkAccessManager;
class QNetworkReply;
class QFile;
class QSqlDatabase;
class QAuthenticator;
class QSslError;
class QTimer;


class GetHttpFile : public QObject
{
    Q_OBJECT
public:
    explicit GetHttpFile(const HttpFileInfo& info, QObject *parent = 0);
    ~GetHttpFile();
    void updateInfo(const HttpFileInfo& info);//update file info before start or restart
    bool isFinished();//Has the file been downloaded or completed?
    inline void setOpen(const bool isOpen){ _isOpenAfterFinished = isOpen; }//When the download is complete, open the file.
	void setDuration(const int ms){ _speedDuration = ms; }//set downlaod file speed duration, unit is millisecond (0 < ms <= 2000)

private:
    void init();//initialized data
    void uninit();//uninitialized data
    void reset();//reset data
    void innerStart();

private:
    bool openFile();//open file for write data
    void closeFile();//close file
    bool openDb();//open or create db and tabels;
    void closeDb();//close db
    bool addFileInfo(const HttpFileInfo& fileInfo);//add file info to db
    bool deleteFileInfoById(const QString& id);//delete file info by id
    bool updateFileInfo(const HttpFileInfo& fileInfo);//update file info
    bool getFileInfoById(const QString& id, HttpFileInfo& fileInfo);//get file info by id
    bool _createTabel();//create file info tabel if not exist
    bool _createVersionTabel(const int version);//create tabels on version
    QString getId();//calculate dir + name + url
    QString getFullFileName();//get full file name include dir
    void setFileInfoFromSvr();//set file info from reply
    bool openDownloadFile();//open file with program
    void startCalculateSpeed();//start or restart calculate download file speed.
    void stopCalculateSpeed();//stop calculate download file speed.

signals:
    void sendError(const int level, const QString info);//level: 0.reserve 1.info 2.bug 3.warning 4.error 5.fatal
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void finished();
    void cancelDownload(int reason);//reason 0.reserve 1.manual cancel 2.timeout cancel
    void sendSpeed(const float speednum, const int speedtype);//Displays the download speed. speedtype: 1.byte 2.kilobyte(kb) 3.megabyte(mb) 4.gigabyte(gb) 5.

public slots:
    void start();//start download file
    void stop();//stop download file
    void restart(bool isUserClick = true);//remove pre file, and restart download file

private slots:
    void readyReadSlot();
    void authenticationRequiredSlot(QNetworkReply *reply, QAuthenticator *authenticator);
    void sslErrorsSlot(QNetworkReply *reply, const QList<QSslError> &errors);
    void downloadProgressSlot(qint64 bytesReceived, qint64 bytesTotal);
    void finishedSlot();
    void channelReadyReadSlot(int channel);
    void threadStartedSlot();
    void threadFinishedSlot();
    void calSpeed();

private:
    QThread _thisThread;
    HttpFileInfo _info;

    QNetworkAccessManager* _pQnam;
    QNetworkReply* _pReply;
    QFile* _pFile;
    QSqlDatabase* _pDb;
    QTimer* _pSpeed;
	ConnectTimeout* _pConnectTimeout;

    QString _dbName;
    bool _isManualCancel;
    QString _id;
    quint64 _allSize;//from progress
    quint64 _curSize;//from progress
    bool _isContinue;
    bool _isOpenAfterFinished;
    qint64 _speedUnit;//transfer file size per second
	int _speedDuration;
};

#endif // GET_HTTP_FILE_H
