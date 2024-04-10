/****************************************************************************
*
* Copyright (C) 2019-2024 MiaoQingrui. All rights reserved.
* Author: 缪庆瑞 <justdoit_mqr@163.com>
*
****************************************************************************/
/*
 *@author:  缪庆瑞
 *@date:    2022.08.19
 *@brief:   用来动态刷新显示Pixmap的控件
 *
 *注1:QLabel的setPixmap()函数适用于静态图像的显示，而在动态频繁调用时发现，该方法导致cpu占用率非常高，如果
 *图片较大的话界面在update()刷新显示时，也会耗费较多的时间，导致GUI线程阻塞来不及处理一些事件，比如定时器超时
 *事件无法及时响应等。
 *注2:采用继承自QWidget，在paintEvent()函数内使用QPainter调用drawPixmap或者drawImage绘图，在Arm Linux
 *设备上测试，cpu占用率跟QLabel的setPixmap基本一致，刷新绘图时间也没有明显降低，性能并没有提高。
 *注3:采用继承自QOpenGLWidget,在paintEvent()函数内使用QPainter调用drawPixmap或者drawImage绘图，在
 *Arm Linux设备(有GPU，支持opengl)上测试，cpu占用率相较与前面两种方式下降了10%左右，刷新绘图时间也有显著的
 *降低。
 */
#ifndef PIXMAPWIDGET_H
#define PIXMAPWIDGET_H

//是否使用QOpenGLWidget绘制pixmap
#define USE_OPENGL_WIDGET

#ifdef USE_OPENGL_WIDGET
#include <QOpenGLWidget>
#define PARENT_WIDGET QOpenGLWidget
#else
#include <QWidget>
#define PARENT_WIDGET QWidget
#endif
#include <QPixmap>

class PixmapWidget : public PARENT_WIDGET
{
    Q_OBJECT
public:
    explicit PixmapWidget(QWidget *parent = nullptr);

    void setPixmap(const QPixmap &pixmap);

protected:
    virtual void paintEvent(QPaintEvent  *event) override;

signals:

private:
    QPixmap pixmap;

};

#endif // PIXMAPWIDGET_H
