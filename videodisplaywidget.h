/****************************************************************************
*
* Copyright (C) 2019-2023 MiaoQingrui. All rights reserved.
* Author: 缪庆瑞 <justdoit_mqr@163.com>
*
****************************************************************************/
/*
 *@author:  缪庆瑞
 *@date:    2019.08.07
 *@brief:   视频预览界面
 */
#ifndef VIDEODISPLAYWIDGET_H
#define VIDEODISPLAYWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include "pixmapwidget.h"
#include "v4l2capture.h"

//当平台支持opengl时，定义该宏，使用硬件加速绘图
#define USE_OPENGL_DISPLAY

class VideoDisplayWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoDisplayWidget(QWidget *parent = 0);
    ~VideoDisplayWidget();

    void initTimerCapture();
    void initSelectCapture();

protected:

signals:

public slots:
    void captureBtnClickedSlot();//采集按钮响应槽
    void saveImageBtnClickedSlot();//保存图片按钮响应槽

    //定时采集视频
    void timerCaptureFrameSlot();
    //select自动采集，信号传递
    void captrueRgb24FrameSlot(uchar *rgb24Frame);
    void captureRgb24ImageSlot(const QImage &rgb24Image);


private:
    bool initV4l2CaptureDevice();//初始化采集设备

#ifdef USE_OPENGL_DISPLAY
    PixmapWidget *videoOutput;//展示视频画面
#else
    QLabel *videoOutput;//展示视频画面
#endif

    QPushButton *captureBtn;//控制采集开始/结束的按钮
    QPushButton *saveImageBtn;//保存当前图片
    QPushButton *quitBtn;//退出程序

    V4L2Capture *v4l2Capture = NULL;//视频采集对象
    bool useSelectCapture = false;//使用定时采集还是select自动采集
    bool isSaveImage = false;//是否保存当前图片

    //定时获取视频帧
    QTimer *timer = NULL;
    uchar *timerRgbFrameBuf = NULL;//RGB格式图像的内存起始地址
    QImage timerImage;//RGB数据的QImage封装


};

#endif // VIDEODISPLAYWIDGET_H
