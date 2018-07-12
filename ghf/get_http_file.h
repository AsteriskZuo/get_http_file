#ifndef GET_HTTP_FILE_H
#define GET_HTTP_FILE_H

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class QFile;
class QSqlDatabase;
class QAuthenticator;
class QSslError;

struct HttpFileInfo
{
    QString dir;//primary key
    QString name;//primary key
    QString url;//primary key
    quint64 allSize;
    quint64 curSize;
    QString md5;//from server ,is not id
    QString lastModify;//from server
    QString actualurl;
    inline void reset(){
        allSize = 0;
        curSize = 0;
        md5.clear();
        lastModify.clear();
    }
};

class GetHttpFile : public QObject
{
    Q_OBJECT
public:
    explicit GetHttpFile(const HttpFileInfo& info, QObject *parent = 0);
    ~GetHttpFile();
    void start();//start download file
    void stop();//stop download file
    void restart();//remove pre file, and restart download file

private:
    void init();//initialized data
    void reset();//reset data
    bool openFile();//open file for write data
    void closeFile();//close file
    bool openDb();//open or create db and tabels;
    void closeDb();//close db
    bool addFileInfo(const HttpFileInfo& fileInfo);//add file info to db
    bool deleteFileInfoById(const QString& id);//delete file info by id
    bool updateFileInfo(const HttpFileInfo& fileInfo);//update file info
    bool getFileInfoById(const QString& id, HttpFileInfo& fileInfo);//get file info by id

private:
    bool _createTabel();//create file info tabel if not exist
    bool _createVersionTabel(const int version);//create tabels on version
    QString _getId();//calculate dir + name + url

signals:
    void sendError(const int level, const QString info);//level: 0.reserve 1.info 2.bug 3.warning 4.error 5.fatal
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void finished();
    void cancelDownload();

public slots:
private slots:
    void readyReadSlot();
    void authenticationRequiredSlot(QNetworkReply *reply, QAuthenticator *authenticator);
    void sslErrorsSlot(QNetworkReply *reply, const QList<QSslError> &errors);
    void downloadProgressSlot(qint64 bytesReceived, qint64 bytesTotal);
    void finishedSlot();

private:
    HttpFileInfo _info;
    QNetworkAccessManager* _pQnam;
    QNetworkReply* _pReply;
    QFile* _pFile;
    QSqlDatabase* _pDb;
    QString _dbName;
    bool _isManualCancel;
    QString _id;
};

#endif // GET_HTTP_FILE_H
