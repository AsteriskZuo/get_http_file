#pragma once
#ifndef COMMON_DEFINE_H
#define COMMON_DEFINE_H

#include <QTimer>
#include <QString>
#include <QVariant>

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
    int _state;//file state: 0.reserve 1.waitting 2.transferring 3.finished 4.error
    explicit HttpFileInfo();
    inline void reset(){
        allSize = 0;
        curSize = 0;
        md5.clear();
        lastModify.clear();
    }
};

class QNetworkReply;
class ConnectTimeout : public QTimer
{
    Q_OBJECT
public:
    ConnectTimeout(QObject* parent = nullptr);
    ~ConnectTimeout();
    void setParam(QNetworkReply* pReply, const int ms = 3000);
    void startTimer();
    bool isTimeoutCancel(){ return _timeoutCancel; }
private:
    void _stop();
private slots:
    void stopConnect();
    void readyReadSlot();
    void finishedSlot();
private:
    QNetworkReply* _pReply;
    int _ms;
    bool _timeoutCancel;//Whether the timeout has expired
};


#endif // COMMON_DEFINE_H
