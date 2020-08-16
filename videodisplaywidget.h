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
#ifndef VIDEODISPLAYWIDGET_H
#define VIDEODISPLAYWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QTimer>
#include "v4l2capture.h"

class VideoDisplayWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoDisplayWidget(QWidget *parent = 0);
    ~VideoDisplayWidget();

protected:

signals:

public slots:
    void getFrameSlot();//获取采集的视频帧

private:
    QLabel *videoOutput;//展示视频画面

    V4L2Capture *v4l2Capture;//视频采集对象
    uchar *rgbFrameBuf;//RGB格式图像的内存起始地址
    int rgbFrameLen;//RGB格式图像的内存长度
    QImage *image;//QImage图像
    QTimer *timer;//定时获取视频帧

};

#endif // VIDEODISPLAYWIDGET_H
