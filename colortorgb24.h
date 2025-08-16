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
 *该模块目前集成了软解码(包含基础的颜色调整)相关的接口：
 *软解码包括V4L2_PIX_FMT_YUYV、V4L2_PIX_FMT_NV12、V4L2_PIX_FMT_NV21三种yuv格式到rgb的转换处理，均是使用CCIR 601的转换公
 *式(整形移位)，为了提高处理性能，转换函数已经尽最大可能的进行了优化。
 *根据具体需求，通过宏定义(减少因软件标志判断的性能损失)控制是否启用颜色调整处理，目前只针对亮度、对比度、饱和度三项基础参数进行调整。
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

/*表示是否启用颜色调整(亮度、对比度、饱和度)处理算法
 *这里通过宏定义控制是否启用颜色调整处理算法，之所以不使用内部的软标志，是为了减少因代码标志判断造成的性能损失，实现软解码性能的最优化。
 *另外颜色调整算法会使cpu处理占用率提升，非必要情况不建议使用。
 */
#define ENABLE_COLOR_ADJUST

class ColorToRgb24
{
public:
    ColorToRgb24();

    /* 软解码
     * YUV<---->RGB格式转换常用公式(CCIR BT601，主要针对标清图像，高清图像不建议软解码)如下：
     *
     * 注:下述公式yuv和rgb值区间均为全范围[0,255],但很多摄像头采集的YUV数据是TV范围：Y[16,235],UV[16,240]
     * 这也导致了基于该算法处理的图像失真，纯黑色(16,16,16)会误被认为带亮度的灰色
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

    /*颜色调整参数设置*/
    static void setColorAdjustParam(const double &brightness,const double &contrast,const double &saturation);


private:
    static inline void rgbColorAdjust(int &r,int &g,int &b);

    //颜色调整参数结构声明(目前主要针对亮度、对比度、饱和度进行调整)
    struct ColorAdjustmentParam
    {
        //亮度调节，在rgb元素上直接乘以调节比例，1.0对应原图，典型范围[0.5,1.5],避免过曝或欠曝
        double brightness = 1.0;
        //对比度调节，将rgb元素与中间灰度(128)差值与调节比例相乘再加上中间灰度值，拉伸亮度差异，典型范围[0.5,1.5]，1.0对应原图
        double contrast = 1.0;
        /*饱和度调节，典型范围[0.0,2.0]，1.0表示原图，越大色彩越鲜艳，反之色彩越单调
         *此处基于RGB权重近似算法调整饱和度(HSV虽然效果好、方便(QColor自带转换函数接口)，但算法太复杂，影响性能)
         *
         *原理：通过调整RGB三通道的权重，模拟饱和度变化:去饱和​​即向灰度值靠拢(R=G=B=亮度)，增饱和​​即增强颜色分量差异。
         *
         *R′=L+(R−L)×S
         *G′=L+(G−L)×S
         *B′=L+(B−L)×S
         *
         *L(亮度，灰度值)=0.299R+0.587G+0.114B  对应BT601 转换成整形移位公式 L=(306*R+601*G+117*B)>>10
         *S:饱和度系数(<1.0去饱和，1.0原图，>1.0增饱和)
         */
        double saturation = 1.0;
    };
    static ColorAdjustmentParam colorAdjustParam;
    //亮度调节查表法，只有加法和范围校验，运算并不复杂，这里纯粹是以空间换时间，追求极限性能
    static int brightnessLUT[256];
    //对比度调节查表法,减少浮点计算的性能损失(实测对于一些浮点运算能力低的硬件会导致图像闪烁)
    static int contrastLUT[256];
    /*饱和度调节查表法，同样是为了减少浮点计算的性能损失
     *数组元素对应上述(R(GB)-L)的变量区间，这里不使用两个变量的二维数组是因为[256][256]有64Kb，可能会超出cpu
     *一级缓存大小，造成缓存命中率降低。*/
    static int saturationLUT[511];

};

#endif // COLORTORGB24_H
