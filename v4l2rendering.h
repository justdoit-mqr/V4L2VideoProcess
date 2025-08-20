/****************************************************************************
*
* Copyright (C) 2019-2025 MiaoQingrui. All rights reserved.
* Author: 缪庆瑞 <justdoit_mqr@163.com>
*
****************************************************************************/
/*
 *@author:  缪庆瑞
 *@date:    2024.05.17
 *@brief:   负责渲染处理V4L2帧数据(基于opengl的api)
 */
#ifndef V4L2RENDERING_H
#define V4L2RENDERING_H

#include <linux/videodev2.h>//v4l2的头文件
#include <QObject>
#include <QOpenGLExtraFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QOpenGLTexture>
#include <QOpenGLPixelTransferOptions>
#include <QOpenGLFramebufferObject>

class V4l2Rendering : public QObject,protected QOpenGLExtraFunctions
{
    Q_OBJECT
public:
    explicit V4l2Rendering(uint pixel_format,uint pixel_width,
                                uint pixel_height,bool is_tv_range = true,QObject *parent=nullptr);
    ~V4l2Rendering();

    void initializeGL();
    void resizeGL(int w,int h);
    void paintGL();

    void setSingleCaptureImage(bool on){this->needCaptureImage = on;}
    void setMirrorParam(const bool &hMirror,const bool &vMirror);
    void setColorAdjustParam(const bool &enableColorAdjust,const float &brightness,
                             const float &contrast,const float &saturation);
    void updateV4l2Frame(uchar **v4l2FrameData);

signals:
    void captureImageSig(const QImage &image);

private:
    void initVertexShader();
    void initFragmentShader();
    void initTexture();
    void initShaderProgram();

    void paintGLTexture();
    void drawTexture();
    void destroyTexture();

    uint pixelFormat = 0;//采集帧格式
    uint pixelWidth = 0;//像素宽度
    uint pixelHeight = 0;//像素高度
    uint widgetWidth = 0;//渲染组件宽度
    uint widgetHeight = 0;//渲染组件高度
    bool isTVRange = true;//TV range标识(通常摄像头采集的数据为该类型)

    //初始化标识
    bool isInitGl = false;
    //顶点数据
    QOpenGLVertexArrayObject VAO;//存储顶点数据的来源与解析方式(管理VBO的状态数据)
    QOpenGLBuffer VBO;//缓存顶点数据(在显存中)
    //帧缓冲对象
    bool needCaptureImage = false;//表示当前是否需要捕获图像为Image图片
    QOpenGLFramebufferObject *FBO = nullptr;//用于离屏渲染保存图片
    //纹理对象
    QOpenGLTexture texture1;
    QOpenGLTexture texture2;
    QOpenGLTexture texture3;
    /*纹理对象更新纹理数据时，像素解包的存储方式
     *这里主要设置字节对齐,opengl默认为4字节对齐,当像素行字节数不是4的整数倍时，就要改变对齐方式(GL_UNPACK_ALIGNMENT)，否则就会
     *因为每一行尾部读取下一行头部的数据导致整个画面倾斜。注：字节对齐越小效率越低，所以根据情况适当选择*/
    QOpenGLPixelTransferOptions pixelTransferOptions1;
    QOpenGLPixelTransferOptions pixelTransferOptions2;
    QOpenGLPixelTransferOptions pixelTransferOptions3;
    //标识纹理对象是否有效(是否setData)
    bool isVaildTexture = false;

    //着色器
    QString vertexShader;//顶点着色器
    QString fragmentShader;//片段着色器
    QOpenGLShaderProgram shaderProgram;//着色器程序
    //以下参数会直接传递给顶点着色器程序，由GLSL程序对纹理坐标进行处理，实现镜像效果
    struct MirrorParam
    {
        bool hMirror = true;//水平镜像
        bool vMirror = false;//垂直镜像
    }mirrorParam;
    bool mirrorParamChanged = false;//表示镜像参数是否改变
    //以下参数会直接传递给片段着色器程序，由GLSL程序在rgb的基础上进行算法处理，实现基础的颜色调整
    struct ColorAdjustmentParam
    {
        bool enableColorAdjust = false;//是否启用颜色调整
        float brightness = 1.0;//亮度调节 1.0对应原图，典型范围[0.5,1.5],避免过曝或欠曝
        float contrast = 1.0;//对比度调节，典型范围[0.5,1.5]，1.0对应原图
        float saturation = 1.0;//饱和度调节，典型范围[0.0,2.0]，1.0表示原图，越大色彩越鲜艳，反之色彩越单调
    }colorAdjustParam;
    bool colorAdjustParamChanged = false;//表示颜色调整参数是否改变

};

#endif // V4L2RENDERING_H
