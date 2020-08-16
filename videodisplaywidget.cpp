/****************************************************************************
*
* Copyright (C) 2019-2020 MiaoQingrui. All rights reserved.
* Author: 缪庆瑞 <justdoit_mqr@163.com>
*
****************************************************************************/
/*
 *@author:  缪庆瑞
 *@date:    2019.08.07
 *@brief:   视频预览界面
 */
#include "videodisplaywidget.h"
#include <QGridLayout>

VideoDisplayWidget::VideoDisplayWidget(QWidget *parent) :
    QWidget(parent)
{
    v4l2Capture = new V4L2Capture(this);//视频采集对象
    v4l2Capture->initDevice();//视频采集前的初始化

    rgbFrameLen = FRAME_WIDTH*FRAME_HEIGHT*3;//RGB格式图像的内存长度
    rgbFrameBuf = (uchar *)malloc(rgbFrameLen);//为图像帧分配内存空间
    image = new QImage(rgbFrameBuf,FRAME_WIDTH,FRAME_HEIGHT,QImage::Format_RGB888);//根据内存空间创建image图像
    timer = new QTimer(this);//定时获取视频帧
    connect(timer,SIGNAL(timeout()),this,SLOT(getFrameSlot()));

    videoOutput = new QLabel();//展示视频画面
    videoOutput->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);
    videoOutput->setAlignment(Qt::AlignCenter);
    //videoOutput->setScaledContents(true);//按照label的大小放缩视频
    videoOutput->setStyleSheet("border:2px solid black;");

    QGridLayout *gridLayout = new QGridLayout(this);
    gridLayout->setMargin(1);
    gridLayout->addWidget(videoOutput,0,0,1,1);
    this->resize(1000,580);

    v4l2Capture->ioctlSetStreamSwitch(true);//开始视频采集
    timer->start(60);
}

VideoDisplayWidget::~VideoDisplayWidget()
{
}

void VideoDisplayWidget::getFrameSlot()
{
    if(this->isVisible())//仅当前界面被展示才获取界面
    {
        v4l2Capture->ioctlDequeueBuffers(rgbFrameBuf);//获取一帧RGB格式图片流
        image->loadFromData(rgbFrameBuf,rgbFrameLen);//rgb格式图片流生成QImage图片
        videoOutput->setPixmap(QPixmap::fromImage(*image));//屏幕显示
        update();//刷新显示
    }
}
