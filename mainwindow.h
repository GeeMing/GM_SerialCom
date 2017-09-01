#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStringList>
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>
#include <QtSerialPort/QtSerialPortDepends>


namespace Ui {
class MainWindow;
}

struct serialInfo {
    QStringList comName;
    QStringList info;
    QString curComName;
    QSerialPort port;
} ;



class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();


private slots:
    void on_btn_comOpen_clicked();

    void ScanSerialPort();
    void SerialRead();
    void SerialError(QSerialPort::SerialPortError error);

    void on_btn_clearRecv_clicked();
    void on_btn_send_clicked();
    void on_btn_clearRecvCount_clicked();
    void on_btn_clearSend_clicked();

    void on_actionFont_triggered();

    void on_actionReturn_r_n_triggered();

    void on_actionReturn_n_triggered();

    void on_actionGBK_triggered();

    void on_actionUTF_8_triggered();

private:
    Ui::MainWindow *ui;

    struct serialInfo serial;
    bool flag_com_opened = false;
    bool flag_serialErr = false;
    bool flag_recvAsHex = false;
    bool flag_sendAsHex = false;
    bool flag_divideShow = false;
    bool flag_encodeGBK = true;
    bool flag_return_n = false;
    bool flag_autoAddReturn = false;

    QTimer tim_scan;
    QTimer tim_autoSend;
    long recvBytes = 0;
    long sendBytes = 0;

    void ConvertToHex(QByteArray &src, QByteArray &dest);
    void ConvertFromHex(QByteArray &src, QByteArray &dest);
};

#endif // MAINWINDOW_H
