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
 *下面列举部分硬件平台查看GPU的使用情况的方法，方便分析。
 *全志T3:
 *mount -t debugfs debugfs /sys/kernel/debug/
 *cat /sys/kernel/debug/mali/gpu_memory   查看GPU内存使用情况
 *cat /sys/kernel/debug/mali/utilization_gp    查看GPU核心负载率
 *cat /sys/kernel/debug/mali/utilization_gp_pp    查看GPU核心+着色器负载率
 *瑞芯微RK3568:
 *mount -t debugfs debugfs /sys/kernel/debug
 *cat /sys/devices/platform/fde60000.gpu/utilisation  查看GPU核心负载率
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
                                uint pixel_height,bool is_tv_range=true,QWidget *parent = nullptr);
    ~OpenGLWidget();

    //设置镜像参数
    void setMirrorParam(const bool &hMirror,const bool &vMirror);
    //设置颜色调整参数
    void setColorAdjustParam(const bool &enableColorAdjust,const float &brightness,
                             const float &contrast,const float &saturation);
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
