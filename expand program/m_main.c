/************************************************************************************************************************************
overview:  1、主循环
           2、时间片轮
*************************************************************************************************************************************/
#include "r_cg_macrodriver.h"
#include "r_cg_timer.h"
#include "r_cg_serial.h"
#include "r_cg_adc.h"
#include "r_cg_pclbuz.h"

#include "h_type_define.h"
#include "m_com.h"
#include "m_test.h"
#include "m_ad.h"
#include "m_e2.h"
#include "m_peripheral_control.h"
#include "m_main.h"

//函数声明
void system_init(void);  
void system_start(void);
void main_loop(void);    
void timer_op(void); 
void ad_convert_deal();
void timer_int(void);     

//变量定义
flag_type flg_time;
//----------------------------------------------------------
uint8_t   guc_5ms_timer;      //5毫秒计时器
uint8_t   guc_10ms_timer;     //10毫秒计时器
uint8_t   guc_100ms_timer;    //100ms定时器
uint8_t   guc_1s_timer;       //1s定时器
uint8_t   guc_1min_timer;     //1min定时器



/*************************************************************************************************************************************************************
函数功能：初始上电时初始化

函数位置：系统初始化主循环外----------------------------------ok
**************************************************************************************************************************************************************/
void system_init(void)   
{
    R_ADC_Set_OperationOn(); //ad转换启动
    com_init();    

    //--------------------------------------------------
    init_ram_para2();        //peak 烧写E2，新板子要先写一次；
    init_ram_para();

    eeprom2_read_deal();    //eeprom2读取处理程序
    eeprom_read_deal();     //eeprom读取处理程序
    
    
    guc_100ms_timer = 100;
    guc_1s_timer = 100;
    guc_1min_timer = 60;
}
/*************************************************************************************************************************************************************
函数功能：初始上电时初始化

函数位置：系统初始化主循环外----------------------------------ok
**************************************************************************************************************************************************************/
void system_start(void)  //系统启动程序
{
    R_TAU0_Channel7_Start();   //开时间片定时器
    R_ADC_Start();             //AD转换启动

    R_TAU0_Channel2_Start();   //pwm控制杀菌模块的输出
    //debug
   // TDR03 = 48828;
}

/*************************************************************************************************************************************************************
函数功能：需要放在主循环中的函数

函数位置：系统主循环----------------------------------ok
**************************************************************************************************************************************************************/
void main_loop(void)    
{
    timer_op();
    
    if (bflg_allow_ad_calc == 1)   //如果允许ad计算
    {
        bflg_allow_ad_calc = 0;
        ad_temp_calc();            //ad温度计算程序
    }
    
    if(bflg_test_mode == 1)   
    {
        test_mode_com();
    }
    else
    {
        if (bflg_com_allow_rx == 1)  //如果允许接收
        {
            bflg_com_allow_rx = 0;
            //------------------------------
            com_rx_init();   
            COM_RX_MODE;
            R_UART0_Start();
        }
        //----------------------------------
        if (bflg_com_rx_end == 1)    //如果接收结束
        {
            bflg_com_rx_end = 0;
            //------------------------------
            R_UART0_Stop();
            bflg_com_rx_delaytime = 1;
            gss_com_rx_delaytimer = 10;
        }
        //----------------------------------
        if (bflg_com_rx_ok == 1)     //如果接收成功
        {
            bflg_com_rx_ok = 0;
            //------------------------------
            R_UART0_Stop();          //------搞死
            com_rx_data_deal();   
        }
        //----------------------------------
        if (bflg_com_allow_tx == 1)  //如果允许发送
        {
            bflg_com_allow_tx = 0;
            //------------------------------
            R_UART0_Start();
            COM_TX_MODE;
            com_tx_init();   
        }
        if (bflg_com_tx_ok == 1)     //如果发送成功
        {
            bflg_com_tx_ok = 0;       
            //------------------------------
            R_UART0_Stop();
            bflg_com_rx_delaytime = 1;   
            gss_com_rx_delaytimer = 5;    
        }
    }
    
    if(bflg_test_mode == 1)   
    {
        test_in_out_pin();
        test_error_code_deal();
    }
    else
    {
        sterilize_deal();    //杀菌模块
        sterilize_monitor();
        lock_deal();
    }
}
/*************************************************************************************************************************************************************
函数功能：各个时间片轮

函数位置：主循环----------------------------------ok
**************************************************************************************************************************************************************/
void timer_op(void)  
{
    if (bflg_1ms_reach == 1)  //1ms
    {
        bflg_1ms_reach = 0;
        guc_100ms_timer--;
        
        ad_convert_deal();
        com_rx_delaytime();
        com_tx_delaytime();
        com_rx_end_delaytime();
    }
    if (bflg_5ms_reach == 1)  //5ms
    {
        bflg_5ms_reach = 0;

    }
    if (bflg_10ms_reach == 1)  //10ms
    {
        bflg_10ms_reach = 0;
        guc_1s_timer--;

    }
    if (guc_100ms_timer == 0) //100ms
    {
        guc_100ms_timer = 100;

    }
    if (guc_1s_timer == 0)    //1s
    {
        guc_1s_timer = 100;
        guc_1min_timer--;
        
    }
    if (guc_1min_timer == 0)  //1min
    {
        guc_1min_timer = 60;

        
    }
}
/*************************************************************************************************************************************************************
函数功能: 获取每个通道ad采集的值

名词解析: ADCR: 转换结果寄存器
          ADCR  寄存器将 A/D 转换结果保持在其高 10 位（低 6 位固定为 0）;
                因为用的是10位ad转换；
          ADS: 模拟输入通道选择寄存器   
          
函数位置: 1ms定时中---------------------------------------ok        
**************************************************************************************************************************************************************/
void ad_convert_deal(void)    //ad转换处理程序，在1ms定时程序中调用
{
    gus_ad_val = (uint16_t)(ADCR >> 6U);
    //------------------------------------------------------
    ad_val_deal();        //ad值处理程序
    //------------------------------------------------------
    if(guc_ad_index < 2) // 0、1 + 2 = 2、3
    {
        ADS = (uint8_t)(guc_ad_index + 2);   // AD2 AD3
    }
    else if((guc_ad_index >= 2) && ((guc_ad_index < 4)))   // 2、3 + 14 = 16、17
    {
        ADS = (uint8_t)(guc_ad_index + 14);  // AD16 AD17
    }
    else                // 4 + 15 = 19
    {
        ADS = (uint8_t)(guc_ad_index + 15);  // AD19
    }
    
    //ADS = (uint8_t)(guc_ad_index + 2); //ad通道选择，peak因为硬件从第2通道开始的
    //------------------------------------------------------
    R_ADC_Start();  //启动ad转换 peak 每次都要开始还是每改变一次通道都要开始；
}

/*************************************************************************************************************************************************************
函数功能：时间片计时

函数位置：1ms定时中断----------------------------------ok
**************************************************************************************************************************************************************/
void timer_int(void)     
{
    bflg_1ms_reach = 1;       //置1ms到标志位
    //----------------------------------
    guc_5ms_timer++;
    if (guc_5ms_timer >= 5)   //5ms计时到
    {
        guc_5ms_timer = 0;
        bflg_5ms_reach = 1;
    }
    //----------------------------------
    guc_10ms_timer++;
    if (guc_10ms_timer >= 10) //10ms计时到
    {
        guc_10ms_timer = 0;
        bflg_10ms_reach = 1;
    }
}




/***************************************END OF THE FILE******************************************************************************************/
