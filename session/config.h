#ifndef CONFIG_H_
#define CONFIG_H_

#include "global.h"

void task_ui(void *p_arg); //user ui task
void task_net(void *p_arg);//network task
void task_main(void *p_arg);

#endif

