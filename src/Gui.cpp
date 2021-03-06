#include <QtWidgets/QFileDialog>
#include <QPixmap>
#include <QtWidgets/QMessageBox>
#include <QThread>
#include "Gui.h"
#include "Settings.h"
#include "Device.h"
#include <QDir>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>
#include <string>
#include "QtWidgets/QApplication"
#include "QTextStream"
#include "QDebug"
#include "QDateTime"
#include "QDesktopServices"
#include "QList"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <string>
#include "const.h"
#include "icon.xpm"
#include <cstring>
#include "include/libusb-1.0/libusb.h"
#ifdef _WIN32
#include <windows.h>
#include <QWinTaskbarProgress>

#include <bits/stl_list.h>
#else
#define _XOPEN_SOURCE 600
#include <time.h>
#include <unistd.h>
#endif

Gui::Gui (QWidget * parent):QWidget (parent)
{
    QThread::currentThread ()->setPriority (QThread::NormalPriority);
    path = ".";			//current startup dir'
  if (Settings::darkmode == true)
  {
      QFile f(":qdarkstyle/style.qss");
      if (!f.exists())
      {
          console->print("Unable to set stylesheet, file not found\n");
      }
      else
      {
          f.open(QFile::ReadOnly | QFile::Text);
          QTextStream ts(&f);
          qApp->setStyleSheet(ts.readAll());
      }
  }

  this->setWindowIcon (QIcon (QPixmap (icon)));
  this->setWindowTitle (tr ("JoeyQT Version ") + VER);
  grid = new QGridLayout (this);
  left = new QVBoxLayout ();
  right = new QVBoxLayout ();
  center = new QVBoxLayout ();
  down = new QHBoxLayout ();

  image = new QLabel (this);

  image->setFixedSize (200, 40);
  settings = new Settings (this);
  left->addWidget (settings);
  device = new Device (this);
  device->setFixedSize (280, 105);
  left->addWidget (image);
  left->addWidget (device);
  left->addStretch (1);
  grid->addLayout (left, 0, 0);
  console = new Console (this);
  right->addWidget (console);
  progress = new QProgressBar (this);
  #if defined (_WIN32)
     winTaskbar = new QWinTaskbarButton(this);
     QWinTaskbarProgress *winProgress = winTaskbar->progress();
  #endif
  down->addWidget (progress);
  cancel_btn = new QPushButton (tr ("Cancel"), this);
  cancel_btn->setEnabled (false);
  down->addWidget (cancel_btn);
  right->addLayout (down);
  grid->addLayout (right, 0, 2);
  status_btn = new QPushButton (tr ("Read Cart Info"), this);
  rflash_btn = new QPushButton (tr ("Read ROM"), this);
  wflash_btn = new QPushButton (tr ("Write ROM"), this);
  rram_btn = new QPushButton (tr ("Backup Save"), this);
  wram_btn = new QPushButton (tr ("Restore Save"), this);
  eflash_btn = new QPushButton (tr ("Erase ROM"), this);
  eram_btn = new QPushButton (tr ("Erase Save"), this);

  center->addWidget (status_btn, Qt::AlignTop);
  center->addWidget (rflash_btn);
  center->addWidget (wflash_btn);
  center->addWidget (rram_btn);
  center->addWidget (wram_btn);
  center->addWidget (eflash_btn);
  center->addWidget (eram_btn);
  center->addStretch (1);
  grid->addLayout (center, 0, 1);
  #if defined (_WIN32)
      winProgress->setVisible(true);
  #endif

  connect (status_btn, SIGNAL (clicked ()), this, SLOT (show_info ()));

  setProgress (0, 1);
  console->setTextColor(Qt::white);

  console->print (tr ("JoeyQT version ") + VER + tr (" started."));
  console->line();

}

libusb_context *ctx = nullptr;
int r = libusb_init(&ctx);
libusb_device_handle *dev = libusb_open_device_with_vid_pid(ctx, 0x046d, 0x1234);

void Gui::startup_info (void) {
    if(r == 0){
        if(dev == nullptr){
            console->print("No Joey device connected");
        } else {
            int ci = libusb_claim_interface(dev, 0);
            if(ci == 0){
                libusb_bulk_transfer(dev, (0x01 | LIBUSB_ENDPOINT_OUT), *version, sizeof(version), nullptr, 0);
                libusb_bulk_transfer(dev, (0x81 | LIBUSB_ENDPOINT_IN), buffer, 64, nullptr, 0);
                QString bufferString((char*) buffer);
                device->firm_label->setText("Firmware Version: " + bufferString.left(5));

                libusb_bulk_transfer(dev, (0x01 | LIBUSB_ENDPOINT_OUT), &HexID, sizeof(&HexID), nullptr, 0);
                libusb_bulk_transfer(dev, (0x81 | LIBUSB_ENDPOINT_IN), idBuffer, sizeof(idBuffer), nullptr, 0);
                AStream << std::hex << (idBuffer[0]) + (idBuffer[1]<<8) + (idBuffer[2]<<16) + (idBuffer[3]<<24); AStream >> A; HexA.setNum(A,16);
                BStream << std::hex << (idBuffer[4]) + (idBuffer[5]<<8) + (idBuffer[6]<<16) + (idBuffer[7]<<24); BStream >> B; HexB.setNum(B,16);
                CStream << std::hex << (idBuffer[8]) + (idBuffer[9]<<8) + (idBuffer[10]<<16) + (idBuffer[11]<<24); CStream >> C; HexC.setNum(C, 16);

                uint res = ((A ^ 0x4210005) >> 5 | (A ^ 0x4210005) << 0x1b) ^ ((B ^ 0x30041523) >> 3 | (B ^ 0x30041523) << 0x1d) ^ ((C ^ 0x6517bebe) >> 0xc | (C ^ 0x6517bebe) << 0x14);for(int i = 0; i < 100; i++){ res = res >> 1 | (res ^ res >> 0x1f ^ (res & 0x200000) >> 0x15 ^ (res & 2) >> 1 ^ res & 1) << 0x1f; }
                device->dID_label->setText("Device ID: 0x"+HexA+",0x"+HexB+",0x"+HexC.right(8));
                device->key_label->setText("Update Key: 0x" + hex.setNum(res,16));
            }
        }

    } else {
        console->print("libusb Init error");
    }

}

void Gui::show_info () {
    main_readCartHeader();
}

void Gui::main_BV_SetBank(unsigned char blk, unsigned char sublk){
    sublk = sublk * 64;
    unsigned char data[64] = {0x0A,0x00,0x03,0x70,0x00,sublk,0x70,0x01,0xE0,0x70,0x02,blk};
    libusb_bulk_transfer(dev, (0x01 | LIBUSB_ENDPOINT_OUT), data, sizeof(data), nullptr, 0);
}

void Gui::main_ROMBankSwitch(int bank){
    unsigned char bhi = bank>>8;
    unsigned char blo = bank&0xFF;
    unsigned char data1[64] = {0x0A,0x00,0x01,0x30,0x00,bhi};
    unsigned char data2[64] = {0x0A,0x00,0x01,0x21,0x00,blo};
    unsigned char tmp[64];
    libusb_bulk_transfer(dev, (0x01 | LIBUSB_ENDPOINT_OUT), data1, sizeof(data1), nullptr, 0);
    libusb_bulk_transfer(dev, (0x01 | LIBUSB_ENDPOINT_OUT), data2, sizeof(data2), nullptr, 0);
    libusb_bulk_transfer(dev, (0x81 | LIBUSB_ENDPOINT_IN), tmp, 64, nullptr, 0);
}

void Gui::main_readCartHeader(){
    if(settings->GB_check->checkState() == Qt::Checked){
        main_BV_SetBank(0,0);
        main_ROMBankSwitch(1);
        unsigned char data1[64] = {0x10,0x00,0x00,0x01,0x0};
        libusb_bulk_transfer(dev, (0x01 | LIBUSB_ENDPOINT_OUT), data1, sizeof(data1), nullptr, 0);
        libusb_bulk_transfer(dev, (0x81 | LIBUSB_ENDPOINT_IN), rHeader1, 64, nullptr, 0);
        unsigned char data2[64] = {0x10,0x00,0x00,0x01,0x40};
        libusb_bulk_transfer(dev, (0x01 | LIBUSB_ENDPOINT_OUT), data2, sizeof(data2), nullptr, 0);
        libusb_bulk_transfer(dev, (0x81 | LIBUSB_ENDPOINT_IN), rHeader2, 64, nullptr, 0);
        unsigned char data3[64] = {0x10,0x00,0x00,0x01,0x80};
        libusb_bulk_transfer(dev, (0x01 | LIBUSB_ENDPOINT_OUT), data3, sizeof(data3), nullptr, 0);
        libusb_bulk_transfer(dev, (0x81 | LIBUSB_ENDPOINT_IN), rHeader3, 64, nullptr, 0);
        header.str(std::string());
        header << rHeader1[52] << rHeader1[53] << rHeader1[54] << rHeader1[55] << rHeader1[56] << rHeader1[57] << rHeader1[58] << rHeader1[59] << rHeader1[60] << rHeader1[61] << rHeader1[62] << rHeader1[63] << rHeader2[0] << rHeader2[1] << rHeader2[2];
        console->clearConsole();
        console->print(QString::fromStdString(header.str()));
    }

    if(settings->GBA_check->checkState() == Qt::Checked){
        unsigned char data[64] = {0x30,0x00,0x00,0x00,0x40};
        libusb_bulk_transfer(dev, (0x01 | LIBUSB_ENDPOINT_OUT), data, sizeof(data), nullptr, 0);
        libusb_bulk_transfer(dev, (0x81 | LIBUSB_ENDPOINT_IN), gbaHeader, 64, nullptr, 0);
        gbaheader.str(std::string());
        gbaheader <<  gbaHeader[32] << gbaHeader[33] << gbaHeader[34] << gbaHeader[35] << gbaHeader[36] << gbaHeader[37] << gbaHeader[38] << gbaHeader[39] << gbaHeader[40] << gbaHeader[41] << gbaHeader[42] << gbaHeader[43] << gbaHeader[44];
        console->clearConsole();
        console->print(QString::fromStdString(gbaheader.str()));
    }
}

void Gui::read_flash (void) {

}

void Gui::write_flash (void) {

}

void Gui::write_ram (void) {

}

void Gui::erase_flash (void) {

}

void Gui::erase_ram (void) {

}



void Gui::setProgress (int ile, int max) {
  progress->setMinimum (0);
  progress->setMaximum (max);
  progress->setValue (ile);
  #if defined (_WIN32)
      winTaskbar->setWindow(this->windowHandle());
      winTaskbar->progress()->setVisible(true);
      winTaskbar->progress()->setMinimum (0);
      winTaskbar->progress()->setMaximum (max);
      winTaskbar->progress()->setValue (ile);
  #endif
}


void Gui::setEnabledButtons (bool state) {
  status_btn->setEnabled (state);
  rflash_btn->setEnabled (state);
  wflash_btn->setEnabled (state);
  eflash_btn->setEnabled (state);
  cancel_btn->setEnabled (!state);
  if (settings->isRamDisabled ())
    state = false;
  rram_btn->setEnabled (state);
  wram_btn->setEnabled (state);
  eram_btn->setEnabled (state);

}

void Gui::setRamButtons () {
  if (status_btn->isEnabled ())
    setEnabledButtons (true);
}

void Gui::exit() {
    libusb_close(dev);
}
