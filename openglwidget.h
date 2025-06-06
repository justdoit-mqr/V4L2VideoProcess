/****************************************************************************
*
* Copyright (C) 2019-2025 MiaoQingrui. All rights reserved.
* Author: 缪庆瑞 <justdoit_mqr@163.com>
*
****************************************************************************/
/*
 *@author:  缪庆瑞
 *@date:    2024.05.17
 *@brief:   继承自QOpenGLWidget，使用openglapi渲染显示
 */
#ifndef OPENGLWIDGET_H
#define OPENGLWIDGET_H

#include "v4l2rendering.h"
#include <QOpenGLWidget>

class OpenGLWidget : public QOpenGLWidget
{
    Q_OBJECT
public:
    explicit OpenGLWidget(uint pixel_format,uint pixel_width,
                                uint pixel_height,QWidget *parent = nullptr);
    ~OpenGLWidget();

    //该接口仅用于功能测试，通过读取yuv文件测试该类的渲染功能
    void readYuvFileTest(QString file,uint pixelFormat,
                         uint pixelWidth,uint pixelHeight);

protected:
    virtual void initializeGL();
    virtual void resizeGL(int w,int h);
    virtual void paintGL();

private:
    //负责渲染处理v4l2帧数据
    V4l2Rendering *v4l2Rendering = nullptr;

signals:

public slots:
    void updateV4l2FrameSlot(uchar **v4l2Frame);

};

#endif // OPENGLWIDGET_H
