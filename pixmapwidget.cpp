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
#include <QPainter>

PixmapWidget::PixmapWidget(QWidget *parent) : PARENT_WIDGET(parent)
{

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

