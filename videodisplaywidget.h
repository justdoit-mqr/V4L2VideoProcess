/****************************************************************************
*
* Copyright (C) 2019-2025 MiaoQingrui. All rights reserved.
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
#include "openglwidget.h"
#include "v4l2capture.h"

//是否使用yuv渲染器部件，不定义默认用软解码，通过pixmapWidget渲染
#define USE_YUV_RENDERING_WIDGET
//是否使用select自动采集，不定义默认使用定时采集
#define USE_SELECT_CAPTURE

class VideoDisplayWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoDisplayWidget(QWidget *parent = 0);
    ~VideoDisplayWidget();

private:
    void initTimerCapture();
    void initSelectCapture();
    bool initV4l2CaptureDevice();//初始化采集设备

#ifdef USE_YUV_RENDERING_WIDGET
    OpenGLWidget *videoOutput;//展示视频画面
#else
    PixmapWidget *videoOutput;//展示视频画面
#endif

    QPushButton *captureBtn;//控制采集开始/结束的按钮
    QPushButton *saveImageBtn;//保存当前图片
    QPushButton *quitBtn;//退出程序

    V4L2Capture *v4l2Capture = NULL;//视频采集对象
    bool isSaveImage = false;//是否保存当前图片

#ifdef USE_SELECT_CAPTURE
#else
    //定时获取视频帧
    QTimer *timer = NULL;
    uchar *timerRgbFrameBuf = NULL;//RGB格式图像的内存起始地址
    QImage timerImage;//RGB数据的QImage封装
#endif

signals:

public slots:
    void captureBtnClickedSlot();//采集按钮响应槽
    void saveImageBtnClickedSlot();//保存图片按钮响应槽

};

#endif // VIDEODISPLAYWIDGET_H
