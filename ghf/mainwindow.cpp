#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "get_http_file.h"
#include <QtWidgets>

MainWindow::MainWindow(QWidget *parent)\
  : QMainWindow(parent)
  , ui(new Ui::MainWindow)
  , _pHttpFile(0)
{
    ui->setupUi(this);

    ui->progressBar->setMinimum(0);
    ui->progressBar->setMaximum(0);
    ui->progressBar->setValue(0);
    ui->progressBar->setTextVisible(true);

    testInit();

    connect(ui->toolButton_select_directory, &QToolButton::clicked, this, &MainWindow::selectDir);
    connect(ui->pushButton_download_file, &QToolButton::clicked, this, &MainWindow::startDownload);
    connect(ui->pushButton_stop_download, &QToolButton::clicked, this, &MainWindow::stopDownload);
    connect(ui->pushButton_restart_download, &QToolButton::clicked, this, &MainWindow::restartDownlaod);
    connect(ui->lineEdit_url, &QLineEdit::textChanged, this, &MainWindow::textChanged);

    connect(_pHttpFile, &GetHttpFile::sendError, this, &MainWindow::displayError);
    connect(_pHttpFile, &GetHttpFile::downloadProgress, this, &MainWindow::downloadProgressSlot);
    connect(_pHttpFile, &GetHttpFile::finished, this, &MainWindow::finishedSlot);
    connect(_pHttpFile, &GetHttpFile::cancelDownload, this, &MainWindow::cancelDownloadSlot);
}

MainWindow::~MainWindow()
{
    delete ui;
    destroyHttpFile();
}

bool MainWindow::createHttpFile()
{
    if (!_pHttpFile) {
        HttpFileInfo hf;
        hf.url = ui->lineEdit_url->text();
        hf.dir = ui->lineEdit_download_directory->text();
        hf.name = ui->lineEdit_save_file_name->text();

        _pHttpFile = new GetHttpFile(hf);
        return true;
    }
    return false;
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
        _pHttpFile->start();
    }
}

void MainWindow::restartDownlaod(bool)
{
    ui->pushButton_download_file->setEnabled(false);
    ui->pushButton_restart_download->setEnabled(false);
    ui->pushButton_stop_download->setEnabled(true);

    if (createHttpFile()) {
        ui->textEdit_tip->clear();
        _pHttpFile->restart();
    }
}

void MainWindow::stopDownload(bool)
{
    ui->pushButton_stop_download->setEnabled(false);
    ui->pushButton_download_file->setEnabled(false);
    ui->pushButton_restart_download->setEnabled(false);

    if (_pHttpFile) {
        _pHttpFile->stop();
    }
}

void MainWindow::textChanged(const QString &content)
{
    int start = content.lastIndexOf('/');
    if (-1!=start){
        QString&& name = content.mid(start + 1);
        ui->lineEdit_save_file_name->setText(name);
    }
}

void MainWindow::displayError(const int level, const QString info)
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
    ui->progressBar->setValue(bytesReceived);
    ui->progressBar->setMaximum(bytesTotal);
}

void MainWindow::finishedSlot()
{
    ui->pushButton_stop_download->setEnabled(true);
    ui->pushButton_download_file->setEnabled(true);
    ui->pushButton_restart_download->setEnabled(true);

}

void MainWindow::cancelDownloadSlot()
{
    ui->pushButton_stop_download->setEnabled(true);
    ui->pushButton_download_file->setEnabled(true);
    ui->pushButton_restart_download->setEnabled(true);

}
