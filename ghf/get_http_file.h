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
    QString dir;
    QString name;
    quint64 allSize;
    quint64 curSize;
    QString md5;
    QString url;
};

class GetHttpFile : public QObject
{
    Q_OBJECT
public:
    explicit GetHttpFile(const HttpFileInfo& info, QObject *parent = 0);
    void start();//start download file
    void stop();//stop download file
    void restart();//remove pre file, and restart download file

private:
    void init();//initialized Data
    bool openFile();//open file for write data
    void closeFile();//close file
    bool openDb();//open or create db and tabels;
    void closeDb();//close db
    bool addFileInfo(const HttpFileInfo& fileInfo);//add file info to db
    bool deleteFileInfoById(const QString& id);//delete file info by dir + name
    bool updateFileInfo(const HttpFileInfo& fileInfo);//update file info
    bool getFileInfoById(const QString& id, HttpFileInfo& fileInfo);//get file info by dir + name

signals:
    void sendError(const int level, const QString info);//level: 0.reserve 1.info 2.bug 3.warning 4.error 5.fatal
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void finished();

public slots:
private slots:
    void readyRead();
    void authenticationRequired(QNetworkReply *reply, QAuthenticator *authenticator);
    void sslErrors(QNetworkReply *reply, const QList<QSslError> &errors);

private:
    HttpFileInfo _info;
    QNetworkAccessManager* _pQnam;
    QNetworkReply* _pReply;
    QFile* _pFile;
    QSqlDatabase* _pDb;
};

#endif // GET_HTTP_FILE_H
