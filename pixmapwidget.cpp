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
 */
#include "pixmapwidget.h"
#include "colortorgb24.h"
#include <linux/videodev2.h>//v4l2的头文件
#include <QPainter>
#include <QFile>
#include <QTimer>
#include <QDebug>

PixmapWidget::PixmapWidget(QWidget *parent) : PARENT_WIDGET(parent)
{

}
/*
 *@brief:  该接口仅用于功能测试，通过读取yuv文件测试该类的渲染性能(包含yuv转rgb软解码处理)
 *注:可使用FFmpeg工具将mp4格式文件转换成yuv文件进行测试，例如“ffmpeg -i test.mp4 -an -pix_fmt nv12 -s 1024x576 nv12.yuv”
 *FFmpeg支持的格式可通过“ffmpeg -pix_fmts”列出。
 *@date:   2024.04.26
 *@param:  file:需要读取的yuv文件
 *@param:  pixelFormat:yuv的帧格式
 *@param:  pixelWidth,pixelHeight:帧宽度和高度
 */
void PixmapWidget::readYuvFileTest(QString file, uint pixelFormat, uint pixelWidth, uint pixelHeight)
{
    uchar * selectRgbFrameBuf = (uchar *)malloc(pixelWidth*pixelHeight*3);
    QFile *yuvFile = new QFile(file,this);
    if(yuvFile->open(QIODevice::ReadOnly))
    {
        QTimer *readYuvFileTimer = new QTimer(this);
        readYuvFileTimer->setInterval(40);
        connect(readYuvFileTimer,&QTimer::timeout,this,[this,selectRgbFrameBuf,yuvFile,
                pixelFormat,pixelWidth,pixelHeight](){
            if(yuvFile->atEnd())
            {
                yuvFile->seek(0);
            }
            if(pixelFormat == V4L2_PIX_FMT_YUYV)
            {
                QByteArray array = yuvFile->read(pixelWidth*pixelHeight*2);
                uchar *yuvFrame[1];
                yuvFrame[0] = (uchar *)array.data();
                ColorToRgb24::yuyv_to_rgb24_shift(yuvFrame[0],selectRgbFrameBuf,pixelWidth,pixelHeight);
                //此处注意QImage的字节对齐，默认是32bit，如果帧宽度不是4的倍数，则需要特别指定bytesPerLine，否则显示会异常
                QImage selectImage(selectRgbFrameBuf,pixelWidth,pixelHeight,pixelWidth*3,QImage::Format_RGB888);
                this->setPixmap(QPixmap::fromImage(selectImage));//屏幕显示
                this->update();//刷新显示
            }
            else if(pixelFormat == V4L2_PIX_FMT_NV12 ||
                    pixelFormat == V4L2_PIX_FMT_NV21)
            {
                QByteArray array = yuvFile->read(pixelWidth*pixelHeight*3/2);
                uchar *yuvFrame[1];
                yuvFrame[0] = (uchar *)array.data();
                ColorToRgb24::nv12_21_to_rgb24_shift((pixelFormat==V4L2_PIX_FMT_NV12),yuvFrame[0],selectRgbFrameBuf,pixelWidth,pixelHeight);
                //此处注意QImage的字节对齐，默认是32bit，如果帧宽度不是4的倍数，则需要特别指定bytesPerLine，否则显示会异常
                QImage selectImage(selectRgbFrameBuf,pixelWidth,pixelHeight,pixelWidth*3,QImage::Format_RGB888);
                this->setPixmap(QPixmap::fromImage(selectImage));//屏幕显示
                this->update();//刷新显示
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
 *@brief:  设置要绘制的pixmap
 *注:QPixmap是隐式共享类，在数据赋值的过程中可以自动管理内存
 *@author: 缪庆瑞
 *@date:   2022.08.19
 *@param:  pixmap
 */
void PixmapWidget::setPixmap(const QPixmap &pixmap)
{
    this->pixmap = pixmap;
}
/*
 *@brief:  重写绘图事件
 *注:根据Qt提供的例程(2D Painting example)描述可知,QOpenGlWidget因为继承自QWidget，所以可以像QWidget一样
 *在重写的paintEvent()中使用QPainter进行2D绘图，而唯一的区别就是如果系统的opengl驱动支持，该函数内部的绘制操作
 *将在硬件(GPU)中加速，这样就可以减少cpu的占用与绘图处理时间。
 *@author: 缪庆瑞
 *@date:   2022.08.19
 *@param:  event
 */
void PixmapWidget::paintEvent(QPaintEvent *event)
{
    PARENT_WIDGET::paintEvent(event);

    QPainter painter(this);
    /* 对pixmap使用平滑转换算法放缩渲染
     * 如果直接用QImage或者QPixmap的scaled()方法，参数设置Qt::SmoothTransformation进行平滑放缩会
     * 非常耗时间。对于缩小图，如果该方式画面有波纹，可以在外面先用scaled的快速转换将原图等比例缩小一定倍数后
     * 再传递进来进行放缩渲染*/
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawPixmap(this->rect(),pixmap,pixmap.rect());
}

