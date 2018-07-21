#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "get_http_file.h"
#include <QtWidgets>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)\
  : QMainWindow(parent)
  , ui(new Ui::MainWindow)
  , _pHttpFile(0)
  , _pSpeed(0)
{
    ui->setupUi(this);

    _pSpeed = new QLabel();
    _pSpeed->setTextInteractionFlags(Qt::TextSelectableByMouse);
    ui->statusBar->addPermanentWidget(_pSpeed);

    ui->progressBar->setMinimum(0);
    ui->progressBar->setMaximum(0);
    ui->progressBar->setValue(0);
    ui->progressBar->setTextVisible(true);

    testInit();

    connect(ui->toolButton_select_directory, &QToolButton::clicked, this, &MainWindow::selectDir);
    connect(ui->pushButton_download_file, &QToolButton::clicked, this, &MainWindow::startDownload);
    connect(ui->pushButton_stop_download, &QToolButton::clicked, this, &MainWindow::stopDownload);
    connect(ui->pushButton_restart_download, &QToolButton::clicked, this, &MainWindow::restartDownlaod);
    connect(ui->pushButton_clear, &QToolButton::clicked, this, &MainWindow::clearLog);
    connect(ui->lineEdit_url, &QLineEdit::textChanged, this, &MainWindow::textChanged);

    ui->pushButton_download_file->installEventFilter(this);
    ui->pushButton_restart_download->installEventFilter(this);
    ui->pushButton_stop_download->installEventFilter(this);
    ui->pushButton_clear->installEventFilter(this);
}

MainWindow::~MainWindow()
{
    destroyHttpFile();
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *pEvent)
{
    if (!ui->pushButton_download_file->isEnabled()
            || !ui->pushButton_restart_download->isEnabled()){
        QMessageBox::warning(nullptr, tr("HTTP FILE"), tr("The file is being downloaded."), QMessageBox::Ok);
        pEvent->ignore();
    } else {
        pEvent->accept();
    }
}

bool MainWindow::eventFilter(QObject *pWatched, QEvent *pEvent)
{
    if (pWatched == ui->pushButton_download_file && pEvent->type() == QEvent::Enter){
        ui->statusBar->showMessage(tr("Continue download file."));
    } else if (pWatched == ui->pushButton_restart_download && pEvent->type() == QEvent::Enter){
        ui->statusBar->showMessage(tr("Start or restart the download file."));
    } else if (pWatched == ui->pushButton_stop_download && pEvent->type() == QEvent::Enter){
        ui->statusBar->showMessage(tr("Pauses download file."));
    } else if (pWatched == ui->pushButton_clear && pEvent->type() == QEvent::Enter){
        ui->statusBar->showMessage(tr("Clear output log information."));
    }
    return QMainWindow::eventFilter(pWatched, pEvent);
}

bool MainWindow::createHttpFile()
{
    if (!_pHttpFile) {
        HttpFileInfo hf;
        hf.url = ui->lineEdit_url->text();
        hf.dir = ui->lineEdit_download_directory->text();
        hf.name = ui->lineEdit_save_file_name->text();

        _pHttpFile = new GetHttpFile(hf);
		_pHttpFile->setDuration(100);
        Q_ASSERT(_pHttpFile);
        connect(_pHttpFile, &GetHttpFile::sendError, this, &MainWindow::displayInfo);
        connect(_pHttpFile, &GetHttpFile::downloadProgress, this, &MainWindow::downloadProgressSlot);
        connect(_pHttpFile, &GetHttpFile::finished, this, &MainWindow::finishedSlot);
        connect(_pHttpFile, &GetHttpFile::cancelDownload, this, &MainWindow::cancelDownloadSlot);
        connect(_pHttpFile, &GetHttpFile::sendSpeed, this, &MainWindow::displaySpeed);
        connect(this, &MainWindow::start, _pHttpFile, &GetHttpFile::start, Qt::QueuedConnection);
        connect(this, &MainWindow::restart, _pHttpFile, &GetHttpFile::restart, Qt::QueuedConnection);
        connect(this, &MainWindow::stop, _pHttpFile, &GetHttpFile::stop, Qt::QueuedConnection);
    }
    return true;
}

void MainWindow::destroyHttpFile()
{
    if (_pHttpFile) {
        delete _pHttpFile;
        _pHttpFile = 0;
    }
}

void MainWindow::testInit()
{
    ui->lineEdit_url->setText("http://www.winrar.com.cn/download/winrar-x64-550scp.exe");
    ui->lineEdit_download_directory->setText("C:/Users/zuoyu/Desktop/tmp");
    ui->lineEdit_save_file_name->setText("winrar-x64-550scp.exe");
}

void MainWindow::selectDir(bool)
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Open Directory"),
        "", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    ui->lineEdit_download_directory->setText(dir);
}

void MainWindow::startDownload(bool)
{
    ui->pushButton_download_file->setEnabled(false);
    ui->pushButton_restart_download->setEnabled(false);
    ui->pushButton_stop_download->setEnabled(true);

    if (createHttpFile()) {
        HttpFileInfo hf;
        hf.url = ui->lineEdit_url->text();
        hf.dir = ui->lineEdit_download_directory->text();
        hf.name = ui->lineEdit_save_file_name->text();
        _pHttpFile->updateInfo(hf);
        _pHttpFile->setOpen(Qt::Checked == ui->checkBox_open_file_after_download->checkState());

        displayInfo(1, tr("\turl:%1 \n\tdir:%2 \n\tname:%3 ").arg(hf.url).arg(hf.dir).arg(hf.name));
        displayInfo(1, tr("continue download file..."));

        if (_pHttpFile->isFinished()){
            QMessageBox::StandardButton ret = QMessageBox::information(nullptr
                , tr("HTTP FILE"), tr("The file has been downloaded, is it overwritten?")
                , QMessageBox::Yes | QMessageBox::No);
            if (QMessageBox::Yes != ret){
                ui->pushButton_download_file->setEnabled(true);
                ui->pushButton_restart_download->setEnabled(true);
                displayInfo(1, tr("Cancels file overwrite."));
                return;
            }
        }

        emit start();
    }
}

void MainWindow::restartDownlaod(bool)
{
    ui->pushButton_download_file->setEnabled(false);
    ui->pushButton_restart_download->setEnabled(false);
    ui->pushButton_stop_download->setEnabled(true);

    if (createHttpFile()) {
//        ui->textEdit_tip->clear();
        HttpFileInfo hf;
        hf.url = ui->lineEdit_url->text();
        hf.dir = ui->lineEdit_download_directory->text();
        hf.name = ui->lineEdit_save_file_name->text();
        _pHttpFile->updateInfo(hf);
        _pHttpFile->setOpen(Qt::Checked == ui->checkBox_open_file_after_download->checkState());

        displayInfo(1, tr("\turl:%1 \n\tdir:%2 \n\tname:%3 ").arg(hf.url).arg(hf.dir).arg(hf.name));
        displayInfo(1, tr("start or restart download file..."));

        if (_pHttpFile->isFinished()){
            QMessageBox::StandardButton ret = QMessageBox::information(nullptr
                , tr("HTTP FILE"), tr("The file has been downloaded, is it overwritten?")
                , QMessageBox::Yes | QMessageBox::No);
            if (QMessageBox::Yes != ret){
                ui->pushButton_download_file->setEnabled(true);
                ui->pushButton_restart_download->setEnabled(true);
                displayInfo(1, tr("Cancels file overwrite."));
                return;
            }
        }

        emit restart();
    }
}

void MainWindow::stopDownload(bool)
{
    ui->pushButton_stop_download->setEnabled(false);
    ui->pushButton_download_file->setEnabled(false);
    ui->pushButton_restart_download->setEnabled(false);

    if (_pHttpFile) {
        displayInfo(1, tr("stop download file..."));
        emit stop();
    }
}

void MainWindow::clearLog(bool)
{
    ui->textEdit_tip->clear();
}

void MainWindow::textChanged(const QString &content)
{
    int start = content.lastIndexOf('/');
    if (-1!=start){
        QString&& name = content.mid(start + 1);
        ui->lineEdit_save_file_name->setText(name);
    }
}

void MainWindow::displayInfo(const int level, const QString info)
{
    //level: 0.reserve 1.info 2.bug 3.warning 4.error 5.fatal
    QString errInfo;
    switch (level){
    case 0:
        break;
    case 1:
        errInfo = "[info]" + info;
        break;
    case 2:
        errInfo = "[bug]" + info;
        break;
    case 3:
        errInfo = "[warning]" + info;
        break;
    case 4:
        errInfo = "[error]" + info;
        break;
    case 5:
        errInfo = "[fatal]" + info;
        break;
    default:
        break;
    }

    ui->textEdit_tip->append(errInfo);
}

void MainWindow::downloadProgressSlot(qint64 bytesReceived, qint64 bytesTotal)
{
//    constexpr qint64 intmax = INT_MAX ;
    static const qint64 intmax = INT_MAX ;
    if (intmax < bytesTotal){
        int percent = (double)bytesReceived / bytesTotal * 100;
        ui->progressBar->setValue(percent);
        ui->progressBar->setMaximum(100);
    } else {
        ui->progressBar->setValue(bytesReceived);
        ui->progressBar->setMaximum(bytesTotal);
    }
}

void MainWindow::finishedSlot()
{
    ui->pushButton_stop_download->setEnabled(true);
    ui->pushButton_download_file->setEnabled(true);
    ui->pushButton_restart_download->setEnabled(true);
    displayInfo(1, tr("finish download file..."));
}

void MainWindow::cancelDownloadSlot(int reason)
{
    ui->pushButton_stop_download->setEnabled(true);
    ui->pushButton_download_file->setEnabled(true);
    ui->pushButton_restart_download->setEnabled(true);
    displayInfo(1, tr("cancel download file... %1").arg(reason));
}

void MainWindow::displaySpeed(const float speednum, const int speedtype)
{
    QString speedunit;
    switch (speedtype)
    {
    case 0:
        speedunit = "!";
        break;
    case 1:
        speedunit = "b";
        break;
    case 2:
        speedunit = "kb";
        break;
    case 3:
        speedunit = "mb";
        break;
    case 4:
        speedunit = "gb";
        break;
    case 5:
        speedunit = "tb";
        break;
    default:
        speedunit = "?";
        break;
    }

    QString speed(QString::number(speednum, 'f', 2) + speedunit);
    _pSpeed->setText(tr("%1/s").arg(speed));
}
