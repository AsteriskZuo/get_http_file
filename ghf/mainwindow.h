#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class GetHttpFile;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private:
    bool createHttpFile();
    void destroyHttpFile();
    void testInit();

private:
    Ui::MainWindow *ui;
    GetHttpFile* _pHttpFile;

private slots:
    void selectDir(bool);
    void startDownload(bool);
    void restartDownlaod(bool);
    void stopDownload(bool);
    void textChanged(const QString &content);

    void displayError(const int level, const QString info);//level: 0.reserve 1.info 2.bug 3.warning 4.error 5.fatal
    void downloadProgressSlot(qint64 bytesReceived, qint64 bytesTotal);
    void finishedSlot();
    void cancelDownloadSlot();

};

#endif // MAINWINDOW_H
