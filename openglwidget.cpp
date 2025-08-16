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
#include "openglwidget.h"
#include <QTimer>
#include <QFile>

/*
 *@brief:  构造函数
 *@date:   2024.05.17
 *@param:  pixel_format:帧格式(使用v4l2的宏)
 *@param:  pixel_width:像素宽度(需要确保为偶数)  pixel_height:像素高度
 */
OpenGLWidget::OpenGLWidget(uint pixel_format, uint pixel_width, uint pixel_height, bool is_tv_range, QWidget *parent)
    : QOpenGLWidget(parent),v4l2Rendering(new V4l2Rendering(pixel_format,pixel_width,pixel_height,is_tv_range))
{

}

OpenGLWidget::~OpenGLWidget()
{
    makeCurrent();
    if(v4l2Rendering)
    {
        delete v4l2Rendering;
    }
}
/*
 *@brief:  设置镜像参数
 *@date:   2025.08.14
 *@param:  hMirror:true=水平镜像
 *@param:  vMirror:true=垂直镜像
 */
void OpenGLWidget::setMirrorParam(const bool &hMirror, const bool &vMirror)
{
    v4l2Rendering->setMirrorParam(hMirror,vMirror);
}
/*
 *@brief:  设置颜色调整参数
 *@date:   2025.08.14
 *@param:  enableColorAdjust:是否使能基础颜色调整
 *@param:  brightness:亮度调整  典型范围[0.5,1.5]
 *@param:  contrast:对比度调整  典型范围[0.5,1.5]
 *@param:  saturation:饱和度调整  典型范围[0.0,2.0]
 */
void OpenGLWidget::setColorAdjustParam(const bool &enableColorAdjust, const float &brightness,
                                       const float &contrast, const float &saturation)
{
    v4l2Rendering->setColorAdjustParam(enableColorAdjust,brightness,contrast,saturation);
}
/*
 *@brief:  该接口仅用于功能测试，通过读取yuv文件测试该类的渲染功能
 *注:可使用FFmpeg工具将mp4格式文件转换成yuv文件进行测试，例如“ffmpeg -i test.mp4 -an -pix_fmt nv12 -s 1024x576 nv12.yuv”
 *FFmpeg支持的格式可通过“ffmpeg -pix_fmts”列出。
 *@date:   2024.05.17
 *@param:  file:需要读取的yuv文件
 *@param:  pixelFormat:yuv的帧格式
 *@param:  pixelWidth,pixelHeight:帧宽度和高度
 */
void OpenGLWidget::readYuvFileTest(QString file, uint pixelFormat, uint pixelWidth, uint pixelHeight)
{
    QFile *yuvFile = new QFile(file,this);
    if(yuvFile->open(QIODevice::ReadOnly))
    {
        QTimer *readYuvFileTimer = new QTimer(this);
        readYuvFileTimer->setInterval(40);
        connect(readYuvFileTimer,&QTimer::timeout,this,[this,yuvFile,pixelFormat,pixelWidth,pixelHeight](){
            if(yuvFile->atEnd())
            {
                yuvFile->seek(0);
            }
            if(pixelFormat == V4L2_PIX_FMT_YUYV ||
                    pixelFormat == V4L2_PIX_FMT_YVYU)
            {
                QByteArray array = yuvFile->read(pixelWidth*pixelHeight*2);
                uchar *yuvFrame[1];
                yuvFrame[0] = (uchar *)array.data();
                updateV4l2FrameSlot(yuvFrame);
            }
            else if(pixelFormat == V4L2_PIX_FMT_NV12 ||
                    pixelFormat == V4L2_PIX_FMT_NV21)
            {
                QByteArray array = yuvFile->read(pixelWidth*pixelHeight*3/2);
                uchar *yuvFrame[1];
                yuvFrame[0] = (uchar *)array.data();
                updateV4l2FrameSlot(yuvFrame);
            }
            else if(pixelFormat == V4L2_PIX_FMT_YUV420 ||
                    pixelFormat == V4L2_PIX_FMT_YVU420)
            {
                QByteArray array = yuvFile->read(pixelWidth*pixelHeight*3/2);
                uchar *yuvFrame[1];
                yuvFrame[0] = (uchar *)array.data();
                updateV4l2FrameSlot(yuvFrame);
            }
        });
        readYuvFileTimer->start();
    }
    else
    {
        qDebug()<<QString("open yuv file %1 failed!").arg(file);
    }
}
/*
 *@brief:  建立OpenGL的资源和状态
 *@date:   2024.05.17
 */
void OpenGLWidget::initializeGL()
{
    v4l2Rendering->initializeGL();
}
/*
 *@brief:  设置OpenGL的视口、投影
 *widget大小改变时会被调用
 *@date:   2024.05.17
 *@param:  w:宽  h:高
 */
void OpenGLWidget::resizeGL(int w, int h)
{
    v4l2Rendering->resizeGL(w,h);
}
/*
 *@brief:  渲染OpenGL场景
 *widget更新时会被调用
 *@date:   2024.05.17
 */
void OpenGLWidget::paintGL()
{
    v4l2Rendering->paintGL();
}
/*
 *@brief:  更新(渲染)V4l2帧数据
 *@date:   2024.05.17
 *@param:  v4l2Frame:v4l2帧二维指针(指针数组[planes])，planes根据pixelFormat格式在内部自动确定
 *对于多平面planes每一个元素对应着一个平面(不连续)，对于单平面v4l2Frame[0]即完整的v4l2数据
 */
void OpenGLWidget::updateV4l2FrameSlot(uchar **v4l2Frame)
{
    if(!this->isVisible())
    {
        return;
    }

    v4l2Rendering->updateV4l2Frame(v4l2Frame);
    update();
}
