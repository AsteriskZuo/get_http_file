#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>

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

protected:
    void closeEvent(QCloseEvent* pEvent) override;
    bool eventFilter(QObject *pWatched, QEvent *pEvent) override;

private:
    bool createHttpFile();
    void destroyHttpFile();
    void testInit();

private:
    Ui::MainWindow *ui;
    GetHttpFile* _pHttpFile;
    QLabel* _pSpeed;

private slots:
    void selectDir(bool);
    void startDownload(bool);
    void restartDownlaod(bool);
    void stopDownload(bool);
    void clearLog(bool);
    void textChanged(const QString &content);

    void displayInfo(const int level, const QString info);//level: 0.reserve 1.info 2.bug 3.warning 4.error 5.fatal
    void downloadProgressSlot(qint64 bytesReceived, qint64 bytesTotal);
    void finishedSlot();
    void cancelDownloadSlot();
    void displaySpeed(const float speednum, const int speedtype);//Displays the download speed. speedtype: 1.byte 2.kilobyte(kb) 3.megabyte(mb) 4.gigabyte(gb) 5.

signals:
    void start();//start download file
    void stop();//stop download file
    void restart(bool isUserClick = true);//remove pre file, and restart download file

};

#endif // MAINWINDOW_H
