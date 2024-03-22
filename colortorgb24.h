/*
 *@author:  缪庆瑞
 *@date:    2024.03.21
 *@brief:   将指定颜色空间数据转换为rgb24格式(软解码)
 *
 *该模块目前集成了V4L2_PIX_FMT_YUYV、V4L2_PIX_FMT_NV12、V4L2_PIX_FMT_NV21三种yuv格式到rgb的转换
 *处理(软解码)，均是使用CCIR 601的转换公式(整形移位)，为了提高处理性能，转换函数已经尽最大可能的进行了优化。
 */
#ifndef COLORTORGB24_H
#define COLORTORGB24_H

//该宏定义用来启用查表法(yuv转rgb)，不启用则对每一个像素都默认使用整形移位法
#define USE_RGB_YUV_TABLE

#include "qglobal.h"

class ColorToRgb24
{
public:
    ColorToRgb24();

    /* YUV<---->RGB格式转换常用公式(CCIR 601)如下：
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
#ifdef USE_RGB_YUV_TABLE
    static void initRgbTableFromYuv();
    static void yuyv_to_rgb24_table(uchar *yuyv,uchar *rgb24,
                                    const uint &width,const uint &height);
    static void nv12_21_to_rgb24_table(bool is_nv12,uchar *nv12_21,uchar *rgb24,
                                    const uint &width,const uint &height);
#endif

    static void yuyv_to_rgb24_shift(uchar *yuyv,uchar *rgb24,
                                    const uint &width,const uint &height);
    static void nv12_21_to_rgb24_shift(bool is_nv12,uchar *nv12_21,uchar *rgb24,
                                    const uint &width,const uint &height);
    static void rgb4_to_rgb24(uchar *rgb32,uchar *rgb24,const uint &width,const uint &height);

private:
#ifdef USE_RGB_YUV_TABLE
    /*查表法实现yuv到rgb的转换
     *提前基于整形移位公式，将r、g、b的所有可能性计算出来，存到对应的多维数组(表)里，每一维数据索引对应着
     *y、u、v的值，这样在后续转换视频帧时只需要把yuv值带入索引直接取rgb值即可，减少运算处理，降低cpu占用。
     *该方式属于以空间换时间，这几个数组加起来约占16M内存，使用时需要考虑内存的大小，此外在初始化计算下述
     *多维数组值时，运算量比较大(进行1千六百多万次的计算)，还需要考虑cpu主频性能，否则初始化时间可能难以接受。
     */
    static uchar r_yv_table[256][256];
    static uchar g_yuv_table[256][256][256];
    static uchar b_yu_table[256][256];
#endif

};

#endif // COLORTORGB24_H
