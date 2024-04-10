/****************************************************************************
*
* Copyright (C) 2019-2024 MiaoQingrui. All rights reserved.
* Author: 缪庆瑞 <justdoit_mqr@163.com>
*
****************************************************************************/
/*
 *@author:  缪庆瑞
 *@date:    2024.04.05
 *@brief:   直接渲染显示yuv数据的部件(基于opengl的api)
 */
#ifndef YUVRENDERINGWIDGET_H
#define YUVRENDERINGWIDGET_H

#include <linux/videodev2.h>//v4l2的头文件
#include <QOpenGLWidget>
#include <QOpenGLExtraFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QOpenGLTexture>
#include <QOpenGLDebugLogger>

class YuvRenderingWidget : public QOpenGLWidget,protected QOpenGLExtraFunctions
{
    Q_OBJECT
public:
    explicit YuvRenderingWidget(uint pixel_format,uint pixel_width,
                                uint pixel_height,QWidget *parent = nullptr);
    ~YuvRenderingWidget();

    //该接口用于功能测试，通过读取yuv文件测试该类的渲染功能
    void readYuvFileTest(QString file);

protected:
    virtual void initializeGL();
    virtual void resizeGL(int w,int h);
    virtual void paintGL();

private:
    void initVertexShader();
    void initFragmentShader();
    void initTexture();
    void drawTexture();

    uint pixelFormat = 0;//采集帧格式
    uint pixelWidth = 0;//像素宽度
    uint pixelHeight = 0;//像素高度

    QOpenGLVertexArrayObject VAO;//存储顶点数据的来源与解析方式(管理VBO的状态数据)
    QOpenGLBuffer VBO;//缓存顶点数据(在显存中)
    QOpenGLTexture texture1;//纹理对象1
    QOpenGLTexture texture2;//纹理对象2
    QOpenGLTexture texture3;//纹理对象3

    QString vertexShader;//顶点着色器
    QString fragmentShader;//片段着色器
    QOpenGLShaderProgram shaderProgram;//着色器程序

signals:

public slots:
    void updateYuvFrameSlot(uchar *yuvFrame[]);

};

#endif // YUVRENDERINGWIDGET_H
