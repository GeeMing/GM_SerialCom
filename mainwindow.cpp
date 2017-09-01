#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QPushButton>
#include <QCheckBox>
#include <QDebug>
#include <QPixmap>
#include <QFile>
#include <QMessageBox>
#include <QFontDialog>
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>
#include <QtSerialPort/QtSerialPortDepends>

//TODO: Add Escape character support (\n \r \xXX, etc)

QFile file;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    file.setFileName("serial.dat");
    file.remove();// clear the file
    if (!file.open(QIODevice::ReadWrite)) {
        qDebug() << "file open fail";
    }

    ui->edt_autoSendPeriod->setValidator(new QIntValidator(1, 10000, this));

    connect(ui->actionAboutSoftware, &QAction::triggered, [](bool){
        QMessageBox about;
        QString msg;

        msg.append("The app is a serial tool, which is useful in MCU programing and other situation.\n");
        msg.append("Special funtion:\n");
        msg.append("  1.Support serial hot plug and reconnect(such as CH340)\n");
        msg.append("  2.Auto display only avaliable serial port\n");
        msg.append("  3.Encode changeable\n");
        msg.append("  4.Fonts setable\n");
        msg.append("  5.Divide show the recived or send package avaliable\n");

        about.setText(msg);
        about.exec();
    });

    connect(ui->actionAbout_Qt, &QAction::triggered, [=](bool){
        QApplication::aboutQt();
    });
    connect(ui->chkBox_recvAsHex, &QCheckBox::clicked, [=](bool checked){
        flag_recvAsHex = checked;
    });
    connect(ui->chkBox_sendAsHex, &QCheckBox::clicked, [=](bool checked){
        flag_sendAsHex = checked;
    });
    connect(ui->chkBox_divideShow, &QCheckBox::clicked, [=](bool checked){
        flag_divideShow = checked;
    });
    connect(ui->chkBox_autoAddReturn, &QCheckBox::clicked, [=](bool checked){
        flag_autoAddReturn = checked;
    });

    connect(ui->chkBox_autoSend, &QCheckBox::clicked, [=](bool checked){
       if (checked){
           tim_autoSend.start(ui->edt_autoSendPeriod->text().toInt());
       }else{
           tim_autoSend.stop();
       }
    });
    connect(ui->edt_autoSendPeriod, &QLineEdit::textChanged, [=](QString interval){
        tim_autoSend.setInterval(interval.toInt());
    });

    connect(&tim_autoSend, &QTimer::timeout, this, &MainWindow::on_btn_send_clicked);

    connect(&serial.port, &QSerialPort::readyRead, this, SerialRead);
    connect(&serial.port, static_cast<void (QSerialPort::*)(QSerialPort::SerialPortError)>(&QSerialPort::error),
            this, SerialError);

    connect(&tim_scan, &QTimer::timeout, this, &MainWindow::ScanSerialPort);
    tim_scan.start(500);

    connect(ui->cbox_port, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            [ = ](int index){
        if (index != -1) {
            if (!flag_serialErr) {
                ui->lb_serialInfo->setText(serial.comName[index] + "  " + serial.info[index]);
            }
        }
    });

    QStringList strList;
    strList << "1200"
            << "2400"
            << "4800"
            << "9600"
            << "19200"
            << "38400"
            << "57600"
            << "115200"
            << "230400"
            << "460800";
    ui->cbox_baudrate->addItems(strList);
    ui->cbox_baudrate->setCurrentIndex(7);//115200

    strList.clear();
    strList << "None"
            << "Even"
            << "Odd";
    ui->cbox_parity->addItems(strList);
    ui->cbox_parity->setCurrentIndex(0);

    strList.clear();
    strList << "1"
            << "1.5"
            << "2";
    ui->cbox_stopbits->addItems(strList);
    ui->cbox_stopbits->setCurrentIndex(0);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_btn_comOpen_clicked()
{
    if (!flag_com_opened) {

        if (ui->cbox_port->currentText() == ""){
            return;
        }

        if (serial.port.isOpen()) {
            qDebug() << "already opened";
            ui->lb_serialInfo->setText("The serial port has already opened!");
        }

        /******************  Serial port config ********************/
        serial.curComName = serial.comName[ui->cbox_port->currentIndex()];
        serial.port.setPortName(serial.curComName);
        serial.port.setDataBits(QSerialPort::Data8);
        serial.port.setFlowControl(QSerialPort::NoFlowControl);
        serial.port.setBaudRate(ui->cbox_baudrate->currentText().toInt());
        QSerialPort::Parity parity;
        switch (ui->cbox_parity->currentIndex()) {
        default:
        case 0:
            parity = QSerialPort::NoParity;
            break;
        case 1:
            parity = QSerialPort::EvenParity;
            break;
        case 2:
            parity = QSerialPort::OddParity;
            break;
        }
        serial.port.setParity(parity);

        if (ui->cbox_stopbits->currentText() == "1") {
            serial.port.setStopBits(QSerialPort::OneStop);
        } else if (ui->cbox_stopbits->currentText() == "1.5") {
            serial.port.setStopBits(QSerialPort::OneAndHalfStop);
        } else if (ui->cbox_stopbits->currentText() == "2") {
            serial.port.setStopBits(QSerialPort::TwoStop);
        }
        /*******************************************************/


        if (!serial.port.open(QIODevice::ReadWrite)) {
            ui->lb_serialInfo->setText("Serial open fail.");
            return;
        }

        flag_com_opened = true;
        ui->lb_led->setPixmap(QPixmap(":/img/led_green").scaled(24, 24));
        ui->btn_comOpen->setText("Close");
        ui->cbox_port->setEnabled(false);
        tim_scan.stop();
    } else {
        serial.port.close();
        flag_com_opened = false;
        flag_serialErr = false;
        ui->lb_led->setPixmap(QPixmap(":/img/led_red").scaled(24, 24));
        ui->btn_comOpen->setText("Open");
        ui->cbox_port->setEnabled(true);
        tim_scan.start();
    }
}

void MainWindow::ScanSerialPort()
{
    QStringList oldCom = serial.comName;

    serial.comName.clear();
    serial.info.clear();

    QSerialPort serialPort;
    foreach(QSerialPortInfo serialInfo, QSerialPortInfo::availablePorts()){
        serialPort.setPort(serialInfo);
        serial.comName.append(serialInfo.portName());
        serial.info.append(serialInfo.description());
        serialPort.close();
    }

    if (flag_serialErr) {
        if (serial.comName.contains(serial.curComName)) {
            if (!serial.port.open(QIODevice::ReadWrite)) {
                qDebug() << "reconnect: Serial open fail";
                return;
            }

            qDebug() << "serial reconnect";
            ui->lb_led->setPixmap(QPixmap(":/img/led_green").scaled(24, 24));

            tim_scan.stop();
            flag_serialErr = false;
        }
    }

    if (oldCom == serial.comName)
        return;

    ui->cbox_port->clear();

    if (serial.comName.count() > 0) {
        for (int i = 0; i < serial.comName.count(); i++) {
            ui->cbox_port->addItem(serial.comName[i] + "  " + serial.info[i]);
        }
    }
}

void MainWindow::SerialRead()
{
    QTime time = QTime::currentTime();
    QByteArray recvBuf = serial.port.readAll();

    recvBytes += recvBuf.size();
    ui->lb_countInfo->setText(QString("Recv: %1 B Send: %2 B").arg(recvBytes).arg(sendBytes));

    ui->edt_Recv->moveCursor(QTextCursor::End);

    if (flag_divideShow){
        QString timeStr;
        timeStr.sprintf("\n[%02d:%02d:%02d.%03d]<-:", time.hour(), time.minute(), time.second(), time.msec()%1000);
        ui->edt_Recv->insertPlainText(timeStr);
    }

    if (flag_recvAsHex) {
        QByteArray hexArray;
        ConvertToHex(recvBuf, hexArray);

        ui->edt_Recv->insertPlainText(QString(hexArray));
    } else {
        if (flag_encodeGBK){
            ui->edt_Recv->insertPlainText(QString::fromLocal8Bit(recvBuf));
        }else{
            ui->edt_Recv->insertPlainText(QString(recvBuf));
        }
    }
    ui->edt_Recv->moveCursor(QTextCursor::End);
}

void MainWindow::SerialError(QSerialPort::SerialPortError error)
{
    qDebug() << "serial error" << error;

    if (error == QSerialPort::ResourceError) {// hot plug
        if (!flag_serialErr) {
            flag_serialErr = true;
            serial.port.close();
            tim_scan.start();

            ui->lb_led->setPixmap(QPixmap(":/img/led_yellow").scaled(24, 24));
        }
    }
}

void MainWindow::on_btn_clearRecv_clicked()
{
    recvBytes = 0;
    ui->edt_Recv->clear();
    ui->lb_countInfo->setText(QString("Recv: %1 B Send: %2 B").arg(recvBytes).arg(sendBytes));
}

void MainWindow::on_btn_send_clicked()
{
    QByteArray sendBuf;
    QTime time = QTime::currentTime();

    if (flag_com_opened) {
        if (flag_encodeGBK){
            sendBuf = ui->edt_send->toPlainText().toLocal8Bit();
        }else{
            sendBuf = ui->edt_send->toPlainText().toUtf8();
        }
        if (flag_autoAddReturn){
            if (flag_return_n){
                sendBuf += "\n";
            }else{
                sendBuf +="\r\n";
            }
        }
        sendBytes += sendBuf.size();
        if (sendBuf.size() == 0){
            return;//empty send
        }
        ui->lb_countInfo->setText(QString("Recv: %1 B Send: %2 B").arg(recvBytes).arg(sendBytes));

        if (flag_sendAsHex) {
            QByteArray hexArray;
            ConvertFromHex(sendBuf, hexArray);
            serial.port.write(hexArray);
        } else {
            serial.port.write(sendBuf);
        }

        if (flag_divideShow){
            QString timeStr;
            timeStr.sprintf("\n[%02d:%02d:%02d.%03d]->:", time.hour(), time.minute(), time.second(), time.msec()%1000);
            ui->edt_Recv->insertPlainText(timeStr + sendBuf);
            ui->edt_Recv->moveCursor(QTextCursor::End);
        }
    }
}

void MainWindow::on_btn_clearRecvCount_clicked()
{
    recvBytes = 0;
    sendBytes = 0;
    ui->lb_countInfo->setText(QString("Recv: %1 B   Send: %2 B").arg(recvBytes).arg(sendBytes));
}

void MainWindow::on_btn_clearSend_clicked()
{
    ui->edt_send->clear();
}

void MainWindow::ConvertToHex(QByteArray &src, QByteArray &dest)
{
    int len = src.size();

    dest.resize(3 * len);// 2 char + space

    src = src.toHex();

    int j = 0;
    for (int i = 0; i < len * 2; i += 2) {
        dest[j] = src[i];
        if (dest[j] >= 'a') {
            dest[j] = dest[j] & ~0x20; //to upper
        }
        dest[j + 1] = src[i + 1];
        if (dest[j + 1] >= 'a') {
            dest[j + 1] = dest[j + 1] & ~0x20; //to upper
        }
        dest[j + 2] = ' ';
        j += 3;
    }
}

void MainWindow::ConvertFromHex(QByteArray &src, QByteArray &dest)
{
#define IS_HEX(x)   (((x >= 'a') && (x <= 'f')) || ((x >= 'A') && (x <= 'F')) || ((x >= '0') && (x <= '9')))

    int len = src.count();

    dest.clear();
    dest.resize(len / 2 + 1);

    int index_src = 0;
    int index_dest = 0;
    while (index_src < len) {
        if (IS_HEX((src[index_src]))) {     // first char mean high 4bit
            if (src[index_src] <= '9') {
                dest[index_dest] = (src[index_src] - '0');
            } else {
                src[index_src] = src[index_src] & (~0x20);// to upper
                dest[index_dest] = (src[index_src] - 'A' + 10);
            }
            index_src++;
        } else {
            index_src++;
            continue; //ignore the invalid char
        }

        if (index_src >= len) {
            index_dest++;
            break;
        }

        if (IS_HEX((src[index_src]))) {                 //seconed char mean low 4bit
            dest[index_dest] = dest[index_dest] << 4;   //if the second char is invalid, regard as low 4bit

            if (src[index_src] <= '9') {
                dest[index_dest] = dest[index_dest] | (src[index_src] - '0');
            } else {
                src[index_src] = src[index_src] & (~0x20);
                dest[index_dest] = dest[index_dest] | (src[index_src] - 'A' + 10);
            }
        }

        index_dest++;
        index_src++;
    }

    dest.resize(index_dest);
}


void MainWindow::on_actionFont_triggered()
{
    bool ok;
    QFont oldFont = ui->edt_Recv->font();
    QFont newFont = QFontDialog::getFont(&ok, oldFont, this, "字体");
    if (ok){
        ui->edt_send->setFont(newFont);
        ui->edt_Recv->setFont(newFont);
    }
}

void MainWindow::on_actionReturn_r_n_triggered()
{
    flag_return_n = false;
    ui->actionReturn_n->setChecked(false);
}

void MainWindow::on_actionReturn_n_triggered()
{
    flag_return_n = true;
    ui->actionReturn_r_n->setChecked(false);
}

void MainWindow::on_actionGBK_triggered()
{
    flag_encodeGBK = true;
    ui->actionUTF_8->setChecked(false);
}

void MainWindow::on_actionUTF_8_triggered()
{
    flag_encodeGBK = false;
    ui->actionGBK->setChecked(false);
}