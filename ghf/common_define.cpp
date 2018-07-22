#include "common_define.h"
#include <QNetworkReply>


HttpFileInfo::HttpFileInfo()
    : allSize(0), curSize(0), _state(0)
{
    qDebug() << __FUNCTION__ ;
}











ConnectTimeout::ConnectTimeout(QObject* parent /*= nullptr*/)
    : QTimer(parent)
    , _pReply(nullptr)
    , _ms(3000)
    , _timeoutCancel(false)
{
    connect(this, &QTimer::timeout, this, &ConnectTimeout::stopConnect, Qt::QueuedConnection);
}

ConnectTimeout::~ConnectTimeout()
{
    _stop();
}

void ConnectTimeout::setParam(QNetworkReply* pReply, const int ms /*= 3000*/)
{
    _pReply = pReply;
    _ms = ms;
    connect(_pReply, &QNetworkReply::readyRead, this, &ConnectTimeout::readyReadSlot);
    connect(_pReply, &QNetworkReply::finished, this, &ConnectTimeout::finishedSlot, Qt::DirectConnection);
}

void ConnectTimeout::startTimer()
{
    _timeoutCancel = false;
    start(_ms);
}

void ConnectTimeout::_stop()
{
    if (isActive())
        stop();
}

void ConnectTimeout::stopConnect()
{
    _timeoutCancel = true;
    _stop();
    if (_pReply){
        _pReply->abort();
        _pReply = nullptr;
    }
}

void ConnectTimeout::readyReadSlot()
{
    if (isActive())
        start(_ms);
}

void ConnectTimeout::finishedSlot()
{
    _stop();
    if (_pReply){
        _pReply = nullptr;
    }
}
