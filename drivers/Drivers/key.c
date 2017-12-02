/******************************************************************************
 * Copyright (C), 1997-2011, SUNGROW POWER SUPPLY CO., LTD.
 * File name      :key.c
 * Author         :Xu Shun'an
 * Date           :2011.06.02
 * Description    :STM32F10x系列处理器的按键模块函数，包括打开按键、关闭按键、
 *                 读按键状态、清除按键状态标识
 * Others         :None
 *******************************************************************************
 *------------------------------------------------------------------------------
 * 2011-06-02 : 1.0.1 : xusa
 * Modification   :  整理代码格式
 *------------------------------------------------------------------------------
 * 2010-08-03 : 1.0.0 : liulei
 * Modification   : 初始代码编写。
 *******************************************************************************/
 
#define KEY_CONFIG    /* 按键端口信息*/
#include "string.h"
#include "key.h"
#include "fcntl.h"

#define  KEY_MAX_NUM  8

/* 开关所在的端口和引脚列表，开关的编号和其端口的下标相同，
 * 在查询开关状态的宏定义KEY_PRESS(i)KEY_LONG_PRESS(i),
 * KEY_RELEASE(i)中的i即为此处的下标 */

GPIO_CTL key_list[KEY_MAX_NUM];
GPIO_CTL key_buzzer;
uint8_t  key_num = 0U;

volatile uint16_t counter[KEY_MAX_NUM];    /*该数组用于判断按键是否按下以及是否长按*/

bool key_opened = false;    /* 按键端口打开标志 */

/* 用一个32位数表示按键状态，最多可表示8个按键，*
 * 每个按键状态用掉4位中的3位，第四位表示是否长按超过限值，表示 *
 * 按键是按下、长按、弹起三种状态****************/
volatile uint32_t key_state = 0U;
volatile uint8_t  key_beep = 0U;    /*指示按键是否按下的LED灯或蜂鸣器*/

uint32_t buzzer_flag = 0U;        //wujing 2012.10.23 add to control buzzer. buzzer_flag == 0: buzzer close; buzzer_flag == 1: buzzer open.




/********************************************************************************
 * Function       : key_timer
 * Author         : Xu Shun'an
 * Date           : 2011.06.02
 * Description    : 判断按键状态。需在系统毫秒定时器中调用，以实时扫描按键当前状态
 *                  在系统时钟为72MHz时，函数调用时间为 94 us，堆栈使用了 40 bytes
 * Calls          : None
 * Input          : None
 * Output         : None
 * Return         : None
 *********************************************************************************/
void key_timer(void)
{
    uint32_t ret; /*for MISRA-2004*/
    uint32_t tmp;
	uint32_t tmp1;
    /*按键状态用32位数据中每4位代表一个按键状态，bt用于选取这4位中到底是哪一位*/
    uint8_t bt;

    if(key_opened == true)
    {
        /*检测每一个按键，这里i代表第i个按键，总共有key_num个按键*/
        for (uint8_t i = 0U; i < key_num; i++)
        {
            /*此时btf分别被移位到第0、4、8、12，代表0、1、2、3号按键*/
            bt = (uint8_t)(i << 2);

            /*判断按键是否弹起状态*/
            if ((key_state & (4U << bt)) == 0U)
            {
                /*如果按键按下了*/
                if(GPIO_ReadInputDataBit(key_list[i].port, key_list[i].pin) != KEY_PULL_UP)
                {
                    /*如果之前没有按下的动作*/
                    if ((key_state & (1U << bt)) == 0U)
                    {
                        /*counter[i]为0,说明还未启动去抖动计时*/
                        if (counter[i] == 0U)
                        {
                            counter[i] = 1U;
                        }
                        /*说明去抖动计时时间到,确实为按下*/
                        if (counter[i] > KEY_STABLE_DELAY)
                        {
                            key_state |= 1U << bt;
                        }
                    }
                    /*如果之前已经判断为按下,且还未判断为长按*/
                    else if ((key_state & (2U << bt)) == 0U)
                    {
                        /*说明长按计时时间到,确实为长按*/
                        if ((counter[i] > LONG_PRESS_DELAY)&&(counter[i] <= LONG_PRESS_DELAY_LIMIT))
                        {
                            key_state |= 2U << bt;
                        }
                    }
                    /*如果之前已经判断为长按，还未判断为超过长按限值*/
                    else if((key_state & (8U << bt)) == 0U)	  //长按上限检测，用按键状态的第4位表示
                    {
                        if(counter[i] > LONG_PRESS_DELAY_LIMIT)	//wujing 2012.12.10 增加长按上限检测，
                        {
                            key_state |= 8U << bt; 
                        }
                    }
                    else
                    {
                        /*do nothing*/
                    }
                }

                /*若为高电平, 此时按键并未按下，但是按键状态没有更新，此处用于按键状态的更新*/
                else
                {

                    /* 如果按键状态此时并未按下但是状态标志却未弹起，则将其状态置为弹起 ????此处加了(key_state & (2<<bt)) != 0*/
                    ret = key_state & (4U << bt); /*for MISRA-2004*/
                    tmp = key_state & (1U << bt);
                    tmp1 = key_state &(8U << bt); //wujing 2012.12.10 超长按键的判断
                    if ((ret == 0U) && (tmp != 0U) && (tmp1 == 0U))
                    {
                        key_state |= 4U << bt;

                        if (key_beep == 0U)
                        {
                            key_beep = 50U;
                        }
                    }
                    else if((tmp1 != 0U) && (ret == 0U))  //检测到超长按键，且没有弹起
                    {
                         key_state &= 0U; //key_state 清0，即等同于没有操作按键
                    }

                    /* 按键弹起了，此时一次判断按键状态的循环结束，将计数值置为0。*/
                    counter[i] = 0U;
                }

                /*开始计数，计数值与按键是否按下，以及是否是长按的时间进行比较，以判断按键状态*/
                if ((counter[i] > 0U) && (counter[i] < (LONG_PRESS_DELAY_LIMIT + 5U))) //wujing 2012.12.10 增加长按上限检测，超过6S认为等于未按
                {
                    ++counter[i];
                }            
            }     
        }

        /*按键按下，LED闪烁*/
        if (key_beep != 0U)
        {
            if (key_beep > 25U)
            {
                if(buzzer_flag == 1) //wujing 2012.10.23 add to control buzzer. buzzer_flag == 0: buzzer close; buzzer_flag ==1: buzzer open.
                {
                    GPIO_ResetBits(key_buzzer.port,key_buzzer.pin);
                }
            }
            else
            {
                GPIO_SetBits(key_buzzer.port,key_buzzer.pin);
            }
            --key_beep;
        }
    }
}

/********************************************************************************
 * Function       : key_open
 * Author         : Xu Shun'an
 * Date           : 2011.06.02
 * Description    : 打开按键。主要将按键端口打开标志置“1”，按键端口初始化在函数
 *                  io_interface.c中; 在系统时钟为72MHz时，函数调用时间为 20 us，堆栈
 *                  使用了 0 bytes
 * Calls          : None
 * Input          : None
 * Output         : None
 * Return         : DSUCCESS: 按键端口打开
 *********************************************************************************/
int32_t key_open(int32_t flag, int32_t mode)
{
    uint8_t i;
    if(key_opened == false)
    {
        device_gpio_config(DEVICE_ID_KEY, TRUE);
        device_gpio_config(DEVICE_ID_BUZZER, TRUE);
        /* 从GPIO配置表中检索KEY信息 */
        for(i = 0; i < KEY_MAX_NUM; i++)
        {
            key_list[i] = get_gpio_ctl_attr(DEVICE_ID_KEY, (PIN_TYPE)(PIN_GPIO_KEY1 + i));
            if(key_list[i].port != 0)
            {
                key_num++;
                key_opened = true;
            }
            else
            {
                break;
            }
        }
        key_buzzer = get_gpio_ctl_attr(DEVICE_ID_BUZZER, PIN_GPIO_BUZZER);
    }
    return DSUCCESS;
}


/********************************************************************************
 * Function       : key_close
 * Author         : Xu Shun'an
 * Date           : 2011.06.02
 * Description    : 关闭按键，将按键所占用的IO口关闭； 在系统时钟为72MHz时，函数调
 *                  用时间为 54 us，堆栈使用了 16 bytes
 * Calls          : GPIO_DeInit
 * Input          : None
 * Output         : None
 * Return         : DSUCCESS：按键端口关闭
 *********************************************************************************/
int32_t key_close(void)
{
    int32_t ret = DSUCCESS;
    if(true == key_opened)
    {
        key_beep = 0;
        key_num = 0;
        device_gpio_config(DEVICE_ID_KEY, FALSE);
        device_gpio_config(DEVICE_ID_BUZZER, FALSE);
        ret = DSUCCESS;
		key_opened = false;
    }
    return ret;
}


/********************************************************************************
 * Function       : key_read
 * Author         : Xu Shun'an
 * Date           : 2011.06.02
 * Description    : 读取按键状态信息，该信息为按键状态的历史信息，按键状态字是一个
 *                  全局变量，由函数key_timer（）在毫秒定时器中实时更新关闭按键，
 *                  在系统时钟为72MHz时，函数调用时间为 168us，堆栈使用了 4 bytes
 * Calls          : memcpy
 * Input          : buf：存储按键状态信息的数据指针；count：要读取表示状态信息的数
 *                  据的字节数，至少为4
 * Output         : None
 * Return         : OPFAULT：读取按键状态信息失败；count：读取到的状态信息字节数
 *********************************************************************************/
int32_t key_read(uint8_t buf[], uint32_t count)
{
    int32_t ret;
    if((!key_opened) || (count < 4U))
    {
        ret = OPFAULT;
    }
    else
    {
        uint32_t tmp = key_state;
        memcpy(&buf[0], &tmp, 4U);
        ret = (int32_t)count;
    }
    return ret;
}


/**********************************************************************************
 * Function       : key_ioctl
 * Author         : Xu Shun'an
 * Date           : 2011.06.02
 * Description    : 清除按键状态信息，将按键状态信息标识为 按键弹起；在系统时钟为
 *                  72MHz时，函数调用时间为 40 us，堆栈使用了 0 bytes
 * Calls          : None
 * Input          : op：按键操作的种类与按键编号 相或 运算 ；arg： 未定义，可为NULL
 * Output         : None
 * Return         : OPFAULT：清除按键状态信息失败；DSUCCESS：清除按键状态信息成功
 ***********************************************************************************/
int32_t key_ioctl(uint32_t op, void *arg)
{
    arg = arg;
    int32_t ret = DSUCCESS;
    uint32_t cmd    = op & 0xffffff00U;
    uint32_t key_id = op & 0x000000ffU;

    if (key_id >= key_num)
    {
        ret =  OPFAULT;
    }
    else
    {
        switch(cmd)
        {
        case KEY_CLR_STATE:
            key_state &= ~((uint32_t)((uint32_t)0x0f << (key_id * 4U)));
            break;
        case BUZZER_OPEN:
            buzzer_flag = 1U;//wujing 2012.10.23 open buzzer;
            break;
        case BUZZER_CLOSE:
            buzzer_flag = 0U; //wujing 2012.10.23 close buzzer;
            break;
        default:
            ret = OPFAULT;
            break;
        }
    }
    return ret;
}



