#ifndef _INCLUDE_H_
#define _INCLUDE_H_


#include "stm32f4xx.h"


enum
{
    A_X = 0,
    A_Y ,
    A_Z ,
    G_Y ,
    G_X ,
    G_Z ,
    TEM ,
    ITEMS ,
};


//================传感器===================
#define ACC_ADJ_EN                              //是否允许校准加速度计,(定义则允许)

#define OFFSET_AV_NUM    50          //校准偏移量时的平均次数。
#define FILTER_NUM              10         //滑动平均滤波数值个数

#define TO_ANGLE                  0.06103f    //0.061036 //   4000/65536  +-2000   ???

#define FIX_GYRO_Y              1.02f       //陀螺仪Y轴固有补偿
#define FIX_GYRO_X              1.02f       //陀螺仪X轴固有补偿

#define TO_M_S2                    0.23926f             //   980cm/s2    +-8g   980/4096
#define ANGLE_TO_RADIAN 0.01745329f //*0.01745 = /57.3      角度转弧度

#define MAX_ACC                  4096.0f        //+-8G		加速度计量程
#define TO_DEG_S                 500.0f         //T = 2ms  默认为2ms ，数值等于1/T



//================绯荤粺===================

#define USE_US100           //浣跨敤us100鍨嫔佛瓒呭０娉?

#define MAXMOTORS 		(4)		//鐢垫満鏁伴噺
#define GET_TIME_NUM 	(5)		//璁剧疆銮峰彇镞堕棿镄勬暟缁勬暟閲?
#define CH_NUM 				(8) 	//鎺ユ敹链洪€氶亾鏁伴噺

#define USE_TOE_IN_UNLOCK 0 // 0锛氶粯璁よВ阌佹柟寮忥紝1锛氩鍏В阌佹柟寮?
#define ANO_DT_USE_USART2 	//寮€鍚覆鍙?鏁颁紶锷熻兘
#define ANO_DT_USE_USB_HID	//寮€鍚鎺SBHID杩炴帴涓娄綅链哄姛鑳?

//=======================================
/***************涓柇浼桦厛绾?*****************/
#define NVIC_GROUP NVIC_PriorityGroup_3		//涓柇鍒嗙粍阃夋嫨
#define NVIC_PWMIN_P			1		//鎺ユ敹链洪噰板?
#define NVIC_PWMIN_S			1
#define NVIC_TIME_P       2		//鏆傛湭浣跨敤
#define NVIC_TIME_S       0
#define NVIC_UART_P				5		//鏆傛湭浣跨敤
#define NVIC_UART_S				1
#define NVIC_UART2_P			3		//涓插彛2涓柇
#define NVIC_UART2_S			1
/***********************************************/

//================浼犳劅鍣?==================
#define ACC_ADJ_EN 									//鏄惁鍏佽镙″嗳锷犻€熷害璁?(瀹氢箟鍒椤厑璁?

#define OFFSET_AV_NUM       50					//镙″嗳锅忕Щ閲忔椂镄勫钩鍧囨鏁般€?
#define FILTER_NUM 			10					//婊戝姩骞冲潎婊ゆ尝鏁板€间釜鏁?

#define TO_ANGLE 			0.06103f 		//0.061036 //   4000/65536  +-2000   ???

#define FIX_GYRO_Y 			1.02f				//闄€铻轰华Y杞村浐链夎ˉ锅?
#define FIX_GYRO_X 			1.02f				//闄€铻轰华X杞村浐链夎ˉ锅?

#define TO_M_S2 				0.23926f   	//   980cm/s2    +-8g   980/4096
#define ANGLE_TO_RADIAN 0.01745329f         //*0.01745 = /57.3	瑙掑害杞姬搴?

#define MAX_ACC  4096.0f						//+-8G		锷犻€熷害璁￠噺绋?
#define TO_DEG_S 500.0f      				//T = 2ms  榛樿涓?ms 锛屾暟链肩瓑浜?/T


// CH_filter[],0妯粴锛?淇话锛?娌归棬锛?鑸悜		
enum
{
 ROL= 0,
 PIT ,
 THR ,
 YAW ,
 AUX1 ,
 AUX2 ,
 AUX3 ,
 AUX4 ,
};
//=========================================

//================鎺у埗=====================
#define MAX_VERTICAL_SPEED_UP  5000										//链€澶т笂鍗囬€熷害mm/s
#define MAX_VERTICAL_SPEED_DW  3000										//链€澶т笅闄嶉€熷害mm/s

#define MAX_CTRL_ANGLE			30.0f										//阆ユ带鑳借揪鍒扮殑链€澶ц搴?
#define ANGLE_TO_MAX_AS 		30.0f										//瑙掑害璇樊N镞讹紝链熸湜瑙掗€熷害杈惧埌链€澶э纸鍙互阃氲绷璋冩暣CTRL_2镄凯链艰皟鏁达级
#define CTRL_2_INT_LIMIT 		0.5f * MAX_CTRL_ANGLE		//澶栫幆绉垎骞呭害

#define MAX_CTRL_ASPEED 	 	300.0f									//ROL,PIT鍏佽镄勬渶澶ф带鍒惰阃熷害
#define MAX_CTRL_YAW_SPEED 	150.0f									//YAW鍏佽镄勬渶澶ф带鍒惰阃熷害
#define CTRL_1_INT_LIMIT 		0.5f *MAX_CTRL_ASPEED		//鍐呯幆绉垎骞呭害


#define MAX_PWM		  100			///%	链€澶WM杈揿嚭涓?00%娌归棬
#define MAX_THR       80 			///%	娌归棬阃氶亾链€澶у崰姣?0%锛岀暀20%缁欐带鍒堕噺
#define READY_SPEED   20			///%	瑙ｉ挛鍚庣数链鸿浆阃?0%娌归棬
//=========================================






#endif

