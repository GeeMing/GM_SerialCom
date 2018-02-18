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

void MainWindow::closeEvent(QCloseEvent *event)
{
    SaveConfig();
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->edt_autoSendPeriod->setValidator(new QIntValidator(1, 10000, this));

    connect(ui->actionAboutSoftware, &QAction::triggered, [](bool){
        QMessageBox about;
        QString msg;

        msg.append("The app is a serial tool, which is useful in MCU programing and other situation.\n");
        msg.append("Feature:\n");
        msg.append("  1.Support serial hot plug and reconnect(such as CH340)\n");
        msg.append("  2.Auto display only avaliable serial port\n");
        msg.append("  3.Encode changeable\n");
        msg.append("  4.Fonts setable\n");
        msg.append("  5.Divide show the recived or send package avaliable\n");
        msg.append("\nAuthor: GeeMing\n");
        msg.append("e-mail: geeming@foxmail.com");

        about.setText(msg);
        about.exec();
    });

    connect(ui->actionAbout_Qt, &QAction::triggered, [=](bool){
        QApplication::aboutQt();
    });
    connect(ui->chkBox_recvAsHex, &QCheckBox::toggled, [=](bool checked){
        flag_recvAsHex = checked;
    });
    connect(ui->chkBox_sendAsHex, &QCheckBox::toggled, [=](bool checked){
        flag_sendAsHex = checked;
    });
    connect(ui->chkBox_divideShow, &QCheckBox::toggled, [=](bool checked){
        flag_divideShow = checked;
    });
    connect(ui->chkBox_autoAddReturn, &QCheckBox::toggled, [=](bool checked){
        flag_autoAddReturn = checked;
    });
    connect(ui->actionSend_Escape_Char, &QAction::toggled, [=](bool checked){
        flag_sendEscapeChar = checked;
    });

    connect(ui->chkBox_autoSend, &QCheckBox::toggled, [=](bool checked){
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
                ui->lb_serialInfo->setText(serial.nameList[index] + "  " + serial.infoList[index]);
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

    LoadConfig();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_btn_comOpen_clicked()
{
    if (!flag_com_opened) {

        if (ui->cbox_port->currentText().isEmpty()){
            return;
        }

        if (serial.port.isOpen()) {
            ui->lb_serialInfo->setText("The serial port has already opened!");
        }

        /******************  Serial port config ********************/
        serial.curName = serial.nameList[ui->cbox_port->currentIndex()];
        serial.port.setPortName(serial.curName);
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
        serial.nameList.clear();
        ui->cbox_port->clear();
        tim_scan.start();
        ScanSerialPort();
    }
}

void MainWindow::ScanSerialPort()
{
    QStringList oldCom = serial.nameList;

    serial.nameList.clear();
    serial.infoList.clear();

    QSerialPort serialPort;
    foreach(QSerialPortInfo serialInfo, QSerialPortInfo::availablePorts()){
        serialPort.setPort(serialInfo);
        serial.nameList.append(serialInfo.portName());
        serial.infoList.append(serialInfo.description());
        serialPort.close();
    }

    if (!flag_serialErr) {
        if (oldCom == serial.nameList){
            return;
        }
        ui->cbox_port->clear();
        for (int i = 0; i < serial.nameList.count(); i++) {
            ui->cbox_port->addItem(serial.nameList[i] + "  " + serial.infoList[i]);
        }

    }else{
        if (serial.nameList.contains(serial.curName)) {
            if (!serial.port.open(QIODevice::ReadWrite)) {
                qDebug() << "reconnect: fail";
                return;
            }

            qDebug() << "reconnect: success";
            ui->cbox_port->clear();
            for (int i = 0; i < serial.nameList.count(); i++) {
                ui->cbox_port->addItem(serial.nameList[i] + "  " + serial.infoList[i]);
            }
            int index = serial.curName.indexOf(serial.curName);
            ui->cbox_port->setCurrentText(serial.curName+ "  " + serial.infoList[index]);
            ui->lb_led->setPixmap(QPixmap(":/img/led_green").scaled(24, 24));
            ui->lb_serialInfo->setText(serial.nameList[ui->cbox_port->currentIndex()]
                    + "  " + serial.infoList[ui->cbox_port->currentIndex()]);
            tim_scan.stop();
            flag_serialErr = false;
        }
    }
}

void MainWindow::SerialRead()
{
    QTime time = QTime::currentTime();
    QByteArray recvBuf = serial.port.readAll();

    recvBytes += recvBuf.size();

    QString info = QString("Recv: %1 B Send: %2 B").arg(recvBytes).arg(sendBytes)
            .replace(QRegExp("(\\d)(?=(\\d{3})+(?!\\d))"), "\\1,");
    ui->lb_countInfo->setText(info);

    ui->edt_recv->moveCursor(QTextCursor::End);

    if (flag_divideShow){
        QString timeStr;
        timeStr.sprintf("\n[%02d:%02d:%02d.%03d]<-:", time.hour(), time.minute(), time.second(), time.msec()%1000);
        ui->edt_recv->insertPlainText(timeStr);
    }

    if (flag_recvAsHex) {
        QByteArray hexArray;
        ConvHex2String(recvBuf, hexArray);
        ui->edt_recv->insertPlainText(QString(hexArray));
    } else {
        if (flag_encodeGBK){
            ui->edt_recv->insertPlainText(QString::fromLocal8Bit(recvBuf));
        }else{
            ui->edt_recv->insertPlainText(QString(recvBuf));
        }
    }
    ui->edt_recv->moveCursor(QTextCursor::End);
}

void MainWindow::SerialError(QSerialPort::SerialPortError error)
{
    qDebug() << "serial error" << error;

    if (!flag_com_opened){
        return;
    }

    if (error != QSerialPort::NoError) {
        if (!flag_serialErr) {
            flag_serialErr = true;
            serial.port.close();
            tim_scan.start();
            ui->lb_led->setPixmap(QPixmap(":/img/led_yellow").scaled(24, 24));
            ui->lb_serialInfo->setText(QString("Serial error:%1, trying to reconnect...").arg(error));
        }
    }
}

void MainWindow::on_btn_clearRecv_clicked()
{
    recvBytes = 0;
    ui->edt_recv->clear();
    ui->lb_countInfo->setText(QString("Recv: %1 B Send: %2 B").arg(recvBytes).arg(sendBytes));
}

void MainWindow::on_btn_send_clicked()
{
    QByteArray sendBuf;
    QByteArray hexArray;
    QString sendStr;
    QTime time;

    if (flag_serialErr){
        return;
    }

    time = QTime::currentTime();

    if (flag_com_opened) {
        sendStr = ui->edt_send->toPlainText();
        if (flag_sendEscapeChar && !flag_sendAsHex){
            RevertBackEscapeChar(sendStr);
        }

        if (flag_encodeGBK){
            sendBuf = sendStr.toLocal8Bit();
        }else{
            sendBuf = sendStr.toUtf8();
        }

        if (flag_autoAddReturn){
            if (flag_return_n){
                sendBuf += "\n";
            }else{
                sendBuf +="\r\n";
            }
        }
        if (sendBuf.size() == 0){
            return;//empty send
        }

        sendBytes += sendBuf.size();
        ui->lb_countInfo->setText(QString("Recv: %1 B Send: %2 B").arg(recvBytes).arg(sendBytes));

        if (flag_sendAsHex) {
            ConvString2Hex(sendBuf, hexArray);
            serial.port.write(hexArray);
        } else {
            serial.port.write(sendBuf);
        }

        if (flag_divideShow){
            QString timeStr;
            timeStr.sprintf("\n[%02d:%02d:%02d.%03d]->:", time.hour(), time.minute(), time.second(), time.msec()%1000);
            ui->edt_recv->moveCursor(QTextCursor::End);
            if (flag_recvAsHex) {
                QByteArray final;
                if (flag_sendAsHex){
                    ConvString2Hex(sendBuf, hexArray);
                    ConvHex2String(hexArray, final);
                }else{
                    ConvHex2String(sendBuf, final);
                }
                ui->edt_recv->insertPlainText(timeStr + final);
            }else{
                ui->edt_recv->insertPlainText(timeStr + sendBuf);
            }
            ui->edt_recv->moveCursor(QTextCursor::End);
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

void MainWindow::RevertBackEscapeChar(QString &str)
{
    const char *placeholders[6][2]={
        {"\\r", "\r"},
        {"\\n", "\n"},
        {"\\t", "\t"},
        {"\\v", "\v"},
        {"\\f", "\f"},
        {"\\0", "\0"}};

    for (int i=0; i<6; i++){
        str = str.replace(placeholders[i][0], placeholders[i][1]);
    }

    int val;
    int pos = 0;
    const QRegExp re( "\\\\((x[0-9a-f]{2})|([0-7]{3}))");// \xHH, \OOO
    while ((pos = re.indexIn(str, pos)) != -1) {
        bool ok;
        QString match = re.cap(1);
        if (match[0] == 'x'){//hex
            match = match.mid(1);
            val = match.toInt(&ok, 16);
        }else{//octal
            val = match.toInt(&ok, 8);
        }
        str = str.replace(pos, 4, QChar(val));
        pos++;
    }
}

void MainWindow::ConvHex2String(QByteArray &src, QByteArray &dest)
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

void MainWindow::ConvString2Hex(QByteArray &src, QByteArray &dest)
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
    QFont oldFont = ui->edt_recv->font();
    QFont newFont = QFontDialog::getFont(&ok, oldFont, this, "字体");
    if (ok){
        ui->edt_send->setFont(newFont);
        ui->edt_recv->setFont(newFont);
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

void MainWindow::ApplyConfig(const QJsonObject &json)
{
    ui->cbox_baudrate->setCurrentText(json.value("baudrate").toString("115200"));
    ui->cbox_parity->setCurrentText(json.value("parity").toString("None"));
    ui->cbox_stopbits->setCurrentText(json.value("stopbits").toString("1"));
    ui->chkBox_autoAddReturn->setChecked(json.value("autoAddReturn").toBool(false));
    ui->chkBox_autoSend->setChecked(json.value("autoSend").toBool(false));
    ui->chkBox_divideShow->setChecked(json.value("divideShow").toBool(false));
    ui->chkBox_recvAsHex->setChecked(json.value("recvAsHex").toBool(false));
    ui->chkBox_sendAsHex->setChecked(json.value("sendAsHex").toBool(false));
    ui->edt_autoSendPeriod->setText(json.value("autoSendPeriod").toString("1000"));
    ui->edt_send->setPlainText(json.value("sendBufferText").toString(""));

    if (json.value("sendEscapeChar").toBool(true)){
        ui->actionSend_Escape_Char->setChecked(true);
    }else{
        ui->actionSend_Escape_Char->setChecked(false);
    }

    if (json["encode"].toString() == "GBK"){
        ui->actionGBK->trigger();
        ui->actionGBK->setChecked(true);//without this, check will be unchecked
    }else{
        ui->actionUTF_8->trigger();
        ui->actionUTF_8->setChecked(true);//without this, check will be unchecked
    }

    if (json["returnType"].toString() == "_n"){
        ui->actionReturn_n->trigger();
        ui->actionReturn_n->setChecked(true);//without this, check will be unchecked
    }else{
        ui->actionReturn_r_n->trigger();
        ui->actionReturn_r_n->setChecked(true);//without this, check will be unchecked
    }

    QFont font;
    font.fromString(json.value("font").toString("YaHei Consolas Hybrid,10,-1,5,50,0,0,0,0,0"));
    ui->edt_send->setFont(font);
    ui->edt_recv->setFont(font);

}

QJsonObject MainWindow::GetCurrentConfig()
{
    QJsonObject json;

    json["version"] = APP_VERSION;
    json["baudrate"] = ui->cbox_baudrate->currentText();
    json["parity"] = ui->cbox_parity->currentText();
    json["stopbits"] = ui->cbox_stopbits->currentText();
    json["autoAddReturn"] = ui->chkBox_autoAddReturn->isChecked();
    json["autoSend"] = ui->chkBox_autoSend->isChecked();
    json["divideShow"] = ui->chkBox_divideShow->isChecked();
    json["recvAsHex"] = ui->chkBox_recvAsHex->isChecked();
    json["sendAsHex"] = ui->chkBox_sendAsHex->isChecked();
    json["font"] = ui->edt_recv->font().toString();
    json["autoSendPeriod"] = ui->edt_autoSendPeriod->text();
    json["sendBufferText"] = ui->edt_send->toPlainText();
//    json["saveRecv"] = ui->chkBox_saveRecv->isChecked();

    json["sendEscapeChar"] = ui->actionSend_Escape_Char->isChecked();
    if (ui->actionGBK->isChecked()){
        json["encode"] = "GBK";
    }else{
        json["encode"] = "UTF-8";
    }

    if (ui->actionReturn_n->isChecked()){
        json["returnType"] = "_n";
    }else{
        json["returnType"] = "_n_r";
    }

    return json;
}

void MainWindow::LoadConfig()
{
    QFile file("config.json");

    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "file open fail";
        this->setWindowTitle(this->windowTitle() +" (config file open fail)");
    }else{
        QByteArray configData = file.readAll();
        file.close();
        QJsonDocument jsonDoc(QJsonDocument::fromJson(configData));
        ApplyConfig(jsonDoc.object());
    }
}

void MainWindow::SaveConfig()
{
    qDebug() << "save config";

    QFile file("config.json");
    file.remove();

    if (!file.open(QIODevice::ReadWrite)) {
        qDebug() << "file open fail";
        this->setWindowTitle(this->windowTitle() +" (config file open fail)");
    }else{
        QJsonDocument jsonDoc(GetCurrentConfig());
        file.write(jsonDoc.toJson());
        file.close();
    }
}
