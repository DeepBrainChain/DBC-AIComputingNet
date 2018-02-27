//#include "include.h"
#include "filter.h"
#include "os_math.h"



// #define WIDTH_NUM 101
// #define FIL_ITEM  10

void anotc_filter_1(float base_hz,float gain_hz,float dT,float in,_filter_1_st *f1)
{
    LPF_1_(gain_hz,dT,(in - f1->out),f1->a); //低通后的变化量

    f1->b = my_pow(in - f1->out);

    f1->e_nr = LIMIT(safe_div(my_pow(f1->a),((f1->b) + my_pow(f1->a)),0),0,1); //变化量的有效率
    
    LPF_1_(base_hz *f1->e_nr,dT,in,f1->out); //低通跟踪
}

 void Moving_Average(float moavarray[],uint16_t len ,uint16_t *fil_cnt,float in,float *out)
{
    uint16_t width_num;
    float last;

    width_num = len ;
    
    if( ++*fil_cnt >= width_num )    
    {
        *fil_cnt = 0; //now
    }
    
    last = moavarray[ *fil_cnt ];
    
    moavarray[ *fil_cnt ] = in;
    
    *out += ( in - ( last  ) )/(float)( width_num ) ;
    *out += 0.00001f *(in - *out);  //次要修正
    
}

int32_t Moving_Median(int32_t moavarray[], uint16_t len , uint16_t *fil_p, int32_t in)
{
    uint16_t width_num;
    uint16_t now_p;
    float t;
    int8_t pn=0;
    uint16_t start_p,i;
    int32_t sum = 0;

    width_num = len ;
    
    if( ++*fil_p >= width_num )
    {
        *fil_p = 0;  //now
    }
    
    now_p = *fil_p ;    
    
    moavarray[ *fil_p ] = in;
    
    if (now_p < width_num -1)       //保证比较不越界
    {
        while(moavarray[now_p] > moavarray[now_p + 1])
        {
            t = moavarray[now_p];
            moavarray[now_p] = moavarray[now_p + 1];
            moavarray[now_p + 1] = t;
            pn = 1;
            now_p ++;
            if(now_p == (width_num-1))
            {
                break;
            }
        }
    }
    
    if(now_p > 0)             //保证比较不越界
    {
        while(moavarray[now_p] < moavarray[now_p - 1])
        {
            t = moavarray[now_p];
            moavarray[now_p] = moavarray[now_p - 1];
            moavarray[now_p - 1] = t;
            pn = -1;
            now_p--;
            if(now_p == 0)
            {
                break;
            }
        }
    
    }
    
    if(*fil_p == 0 && pn == 1)
    {
        *fil_p = width_num - 1;
    }
    else if(*fil_p == width_num - 1 && pn == -1)
    {
        *fil_p = 0;
    }
    else
    {
        *fil_p -= pn;
    }
    
    start_p = (uint16_t)(0.25f * width_num );
    for(i = 0; i < width_num/2;i++)
    {
        sum += moavarray[start_p + i];
    }
    
    return (sum/(width_num/2));
}



void simple_3d_trans(_xyz_f_t *ref, _xyz_f_t *in, _xyz_f_t *out) //小范围内正确。
{
    static int8_t pn;
    static float h_tmp_x,h_tmp_y;
    
    h_tmp_x = my_sqrt(my_pow(ref->z) + my_pow(ref->y));
    h_tmp_y = my_sqrt(my_pow(ref->z) + my_pow(ref->x));
    
    pn = ref->z < 0? -1 : 1;
    
      out->x = ( h_tmp_x *in->x - pn *ref->x *in->z ) ;
        out->y = ( pn *h_tmp_y *in->y - ref->y *in->z ) ;
    
//      out->x = h_tmp_x *in->x - ref->x *in->z;
//      out->y = ref->z *in->y - ref->y *in->z;
    
    out->z = ref->x *in->x + ref->y *in->y + ref->z *in->z ;

}

