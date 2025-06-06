/****************************************************************************
*
* Copyright (C) 2019-2025 MiaoQingrui. All rights reserved.
* Author: 缪庆瑞 <justdoit_mqr@163.com>
*
****************************************************************************/
/*
 *@author:  缪庆瑞
 *@date:    2024.03.21
 *@brief:   将指定颜色空间数据转换为rgb24格式(软解码)
 *
 *该模块目前集成了软解码相关的接口：
 *其中包括V4L2_PIX_FMT_YUYV、V4L2_PIX_FMT_NV12、V4L2_PIX_FMT_NV21三种yuv格式到rgb的转换处理，均是使用CCIR 601的转换公
 *式(整形移位)，为了提高处理性能，转换函数已经尽最大可能的进行了优化。
 *
 *注:关于软解码初期尝试过使用完全查表法(提前基于转换公式将r、g、b的所有可能性计算出来存到表里，通过yuv值索引获取)实现yuv到rgb的转换，
 *但该方式会涉及多维数据（r_yv_table[256][256]、g_yuv_table[256][256][256]、b_yu_table[256][256])访问,初始化运算量较大(进行
 *一千六百多万次的计算，需要考虑cpu主频性能，否则初始化时间可能难以接受)，占用内存较多(约16M)，更严重的是完全查表法有一个无法解决的性能
 *瓶颈，便是多维数组数据量太大造成cpu缓存命中率降低，导致频繁读内存查表效率反而更低。为了避开该瓶颈，采用部分查表法(提前基于整形移位将uv
 *分量跟rgb分量的关系计算存到表里)省却了一部分cpu计算，但后续还是要关联Y分量，进行rgb阈值判断，相较与对每个像素都进行整形移位计算的方式，
 *部分查表法在性能上并没有提高多少。
 */
#ifndef COLORTORGB24_H
#define COLORTORGB24_H

#include "qglobal.h"

class ColorToRgb24
{
public:
    ColorToRgb24();

    /* 软解码
     * YUV<---->RGB格式转换常用公式(CCIR BT601)如下：
     *
     * 注:下述公式yuv和rgb值区间均为[0,255]
     * 浮点计算(效率低)                                  整形移位:(效率较高)
     *                                                 v=V-128; u=U-128;
     * R=Y+1.403*(V−128)                               R=Y+v+((103*v)>>8)
     * G=Y–0.343*(U–128)–0.714*(V–128)                 G=Y-((88*u)>>8)-((183*v)>>8)
     * B=Y+1.770*(U–128)                               B=Y+u+((197*u)>>8)
     *
     * Y=0.299R+0.587G+0.114B
     * U(Cb)=−0.169R−0.331G+0.500B+128
     * V(Cr)=0.500R−0.419G−0.081B+128
     */
    static void yuyv_to_rgb24_shift(uchar *yuyv,uchar *rgb24,
                                    const uint &width,const uint &height);
    static void nv12_21_to_rgb24_shift(bool is_nv12,uchar *nv12_21,uchar *rgb24,
                                    const uint &width,const uint &height);
    static void rgb4_to_rgb24(uchar *rgb32,uchar *rgb24,const uint &width,const uint &height);


private:

};

#endif // COLORTORGB24_H
