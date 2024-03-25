/*
 *@author:  缪庆瑞
 *@date:    2024.03.21
 *@brief:   将指定颜色空间数据转换为rgb24格式(软解码)
 */
#include "colortorgb24.h"
#include <QDebug>
#include <QTime>

ColorToRgb24::ColorToRgb24()
{

}
/*
 *@brief:   将yuyv帧格式数据转换成rgb24格式数据，这里采用的是基于整形移位的yuv--rgb转换公式
 *注:YUYV是YUV422采样方式(数据存储分为packed(打包)和planar(平面))中的一种，基于packed方式的转换。
 *@date:    2019.8.7
 *@param:   yuyv:yuyv帧格式数据地址，该地址通常是对设备的内存映射空间
 *@param:   rgb24:rgb888帧格式数据地址，该地址内存空间必须在方法外申请
 *@param:   width:宽度  height:高度
 */
void ColorToRgb24::yuyv_to_rgb24_shift(uchar *yuyv, uchar *rgb24,
                                       const uint &width, const uint &height)
{
    //qDebug()<<"yuyv_to_rgb24_shift-start:"<<QTime::currentTime().toString("hh:mm:ss:zzz");
    int yvyuLen = width*height*2;//yuyv用四字节表示两个像素
    int y0,u,y1,v;
    int r_uv,g_uv,b_uv;
    int r,g,b;
    int rgbIndex = 0;
    /*每次循环转换出两个rgb像素*/
    for(int i = 0;i<yvyuLen;i += 4)
    {
        //按顺序提取yuyv数据
        y0 = yuyv[i+0];
        u  = yuyv[i+1] - 128;
        y1 = yuyv[i+2];
        v  = yuyv[i+3] - 128;
        //移位法  计算RGB公式内不包含Y的部分，结果可以供两个rgb像素使用
        r_uv = v+((103*v)>>8);
        g_uv = ((88*u)>>8)+((183*v)>>8);
        b_uv = u+((197*u)>>8);
        //像素1的rgb数据
        r = y0 + r_uv;
        g = y0 - g_uv;
        b = y0 + b_uv;
        rgb24[rgbIndex++] = (r > 255)?255:(r < 0)?0:r;
        rgb24[rgbIndex++] = (g > 255)?255:(g < 0)?0:g;
        rgb24[rgbIndex++] = (b > 255)?255:(b < 0)?0:b;
        //像素2的rgb数据
        r = y1 + r_uv;
        g = y1 - g_uv;
        b = y1 + b_uv;
        rgb24[rgbIndex++] = (r > 255)?255:(r < 0)?0:r;
        rgb24[rgbIndex++] = (g > 255)?255:(g < 0)?0:g;
        rgb24[rgbIndex++] = (b > 255)?255:(b < 0)?0:b;
    }
    //qDebug()<<"yuyv_to_rgb24_shift-end:"<<QTime::currentTime().toString("hh:mm:ss:zzz");
}
/*
 *@brief:   将NV12/NV21帧格式数据转换成rgb24格式数据，这里采用的是基于整形移位的yuv--rgb转换公式
 *注：NV12/NV21是YUV420SP格式的一种，two-plane模式(连续缓存)，即Y和UV分为两个plane，Y按照和planar存储，
 *UV(CbCr)则为packed交错存储,两种格式仅仅UV的先后顺序相反。
 *@date:    2024.3.22
 *@param:   is_nv12:true=NV12  false=NV21
 *@param:   nv12_21:NV12/NV21(YUV420SP的一种)帧格式数据地址，该地址通常是对设备的内存映射空间
 *@param:   rgb24:rgb888帧格式数据地址，该地址内存空间必须在方法外申请
 *@param:   width:宽度  height:高度
 */
void ColorToRgb24::nv12_21_to_rgb24_shift(bool is_nv12, uchar *nv12_21, uchar *rgb24,
                                          const uint &width, const uint &height)
{
    //qDebug()<<"nv12_21_to_rgb24_shift-start:"<<QTime::currentTime().toString("hh:mm:ss:zzz");
    uint y_len,rgb_width,cur_pixel_pos,cur_row_pixel_len;
    uint cur_pixel_rgb_pos,next_pixel_rgb_pos;
    int y_odd1,y_odd2,y_even1,y_even2,u,v;
    int r_uv,g_uv,b_uv;
    int r,g,b;
    y_len = width*height;//Y分量的字节长度
    rgb_width = width*3;//一行rgb像素的字节长度
    cur_pixel_pos = 0;//当前处理的像素位置
    for(uint i=0;i<height;i+=2)//一次处理两行
    {
        cur_row_pixel_len = cur_pixel_pos;//当前行首的像素距离首个像素的长度
        for(uint j=0;j<width;j+=2)//一次处理两列
        {
            //当前的rgb像素位置 = 当前处理像素位置*3
            cur_pixel_rgb_pos = cur_pixel_pos*3;
            next_pixel_rgb_pos = cur_pixel_rgb_pos + rgb_width;
            //uv分量
            if(is_nv12)
            {
                u = nv12_21[y_len-(cur_row_pixel_len>>1)+cur_pixel_pos] - 128;
                v = nv12_21[y_len-(cur_row_pixel_len>>1)+cur_pixel_pos+1] - 128;
            }
            else
            {
                v = nv12_21[y_len-(cur_row_pixel_len>>1)+cur_pixel_pos] - 128;
                u = nv12_21[y_len-(cur_row_pixel_len>>1)+cur_pixel_pos+1] - 128;
            }

            //移位法  计算RGB公式内不包含Y的部分，结果可以供4个rgb像素使用
            r_uv = v+((103*v)>>8);
            g_uv = ((88*u)>>8)+((183*v)>>8);
            b_uv = u+((197*u)>>8);
            //四个Y分量，共用一组uv
            y_odd1 = nv12_21[cur_pixel_pos];
            y_odd2 = nv12_21[cur_pixel_pos+1];
            y_even1 = nv12_21[cur_pixel_pos+width];
            y_even2 = nv12_21[cur_pixel_pos+1+width];
            /*关联Y分量，计算出rgb值*/
            //奇数行
            r = y_odd1 + r_uv;
            g = y_odd1 - g_uv;
            b = y_odd1 + b_uv;
            rgb24[cur_pixel_rgb_pos] = (r > 255)?255:(r < 0)?0:r;
            rgb24[cur_pixel_rgb_pos+1] = (g > 255)?255:(g < 0)?0:g;
            rgb24[cur_pixel_rgb_pos+2] = (b > 255)?255:(b < 0)?0:b;
            r = y_odd2 + r_uv;
            g = y_odd2 - g_uv;
            b = y_odd2 + b_uv;
            rgb24[cur_pixel_rgb_pos+3] = (r > 255)?255:(r < 0)?0:r;
            rgb24[cur_pixel_rgb_pos+4] = (g > 255)?255:(g < 0)?0:g;
            rgb24[cur_pixel_rgb_pos+5] = (b > 255)?255:(b < 0)?0:b;
            //偶数行
            r = y_even1 + r_uv;
            g = y_even1 - g_uv;
            b = y_even1 + b_uv;
            rgb24[next_pixel_rgb_pos] = (r > 255)?255:(r < 0)?0:r;
            rgb24[next_pixel_rgb_pos+1] = (g > 255)?255:(g < 0)?0:g;
            rgb24[next_pixel_rgb_pos+2] = (b > 255)?255:(b < 0)?0:b;
            r = y_even2 + r_uv;
            g = y_even2 - g_uv;
            b = y_even2 + b_uv;
            rgb24[next_pixel_rgb_pos+3] = (r > 255)?255:(r < 0)?0:r;
            rgb24[next_pixel_rgb_pos+4] = (g > 255)?255:(g < 0)?0:g;
            rgb24[next_pixel_rgb_pos+5] = (b > 255)?255:(b < 0)?0:b;

            //像素位置向后移动两列
            cur_pixel_pos += 2;
        }
        //像素位置向后跳过一行
        cur_pixel_pos += width;
    }
    //qDebug()<<"nv12_21_to_rgb24_shift-end:"<<QTime::currentTime().toString("hh:mm:ss:zzz");
}
/*
 *@brief:   将rgb32(rgb8888,对应fourcc为rgb4)帧格式数据转换成rgb24格式数据
 *@date:    2024.03.07
 *@param:   rgb32:rgb8888格式帧数据地址，该地址通常是对设备的内存映射空间
 *@param:   rgb24:rgb888帧格式数据地址，该地址内存空间必须在方法外申请
 *@param:   width:宽度  height:高度
 */
void ColorToRgb24::rgb4_to_rgb24(uchar *rgb32, uchar *rgb24, const uint &width, const uint &height)
{
    int rgb32_len = width*height*4;
    int rgb24_index = 0;
    for(int i=0;i<rgb32_len;i+=4)
    {
        rgb24[rgb24_index] = rgb32[i+1];
        rgb24[++rgb24_index] = rgb32[i+2];
        rgb24[++rgb24_index] = rgb32[i+3];
    }
}
