/**
  ******************************************************************************
  * @file   drv_ns4150b.c
  * @author Sifli software development team
  * @brief   Audio Process driver adaption layer
 *
  ******************************************************************************
*/
/**
 * @attention
 * Copyright (c) 2019 - 2022,  Sifli Technology
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Sifli integrated circuit
 *    in a product or a software update for such product, must reproduce the above
 *    copyright notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of Sifli nor the names of its contributors may be used to endorse
 *    or promote products derived from this software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Sifli integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY SIFLI TECHNOLOGY "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL SIFLI TECHNOLOGY OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#include <rtthread.h>
#include <stdlib.h>
#include <string.h>
#include "board.h"
#include "drv_gpio.h"
#if defined(PMIC_CTRL_ENABLE)
    #include "pmic_controller.h"
#endif
#define DBG_TAG           "ns4150b"
#include "rtdbg.h"


#define  NS4150B_WORK_MODE   1   // only 1 2 3 4

static GPIO_TypeDef *gpio_inst = GET_GPIO_INSTANCE(NS4150B_GPIO_PIN);
static uint16_t gpio_pin = GET_GPIOx_PIN(NS4150B_GPIO_PIN);

void ns4150b_gpio_write(bool State)
{
    GPIO_PinState PinState = (GPIO_PinState)State;
    HAL_GPIO_WritePin(gpio_inst, gpio_pin, PinState);
    HAL_Delay_us(5);
}

static int ns4150b_mode = NS4150B_WORK_MODE;
void sifli_ns4150b_start()
{
    int i = ns4150b_mode;
    GPIO_InitTypeDef GPIO_InitStruct;
    /* @todo power handle */
    rt_kprintf("sifli_ns4150b to do power handle \n");
    // set sensor pin to output mode
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT;
    GPIO_InitStruct.Pin  = gpio_pin;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(gpio_inst, &GPIO_InitStruct);
    HAL_Delay_us(550);

    while (1)
    {
        ns4150b_gpio_write(1);
        i--;
        if (i > 0)
        {
            ns4150b_gpio_write(0);
        }
        else
        {
            break;
        }
    }
    rt_kprintf("sifli_ns4150b_start,mode:%d %d\n", ns4150b_mode, NS4150B_GPIO_PIN);
}

void sifli_ns4150b_stop()
{
    ns4150b_gpio_write(0);
    HAL_Delay_us(550);
    rt_kprintf("sifli_ns4150b_stop \n");
}

int apa_set_mode(int argc, char *argv[])
{
    rt_thread_t thread;
    int mode;

    if (argc != 2)
    {
        rt_kprintf("arg para num error\n");
        return -1;
    }
    mode = strtol(argv[1], NULL, 10);
    rt_kprintf("analog PA 8155 work mode=%d\n", mode);

    if ((mode >= 1) && (mode <= 4))
    {
        ns4150b_mode = mode;
    }
    else
    {
        rt_kprintf("analog PA mode error \n");
    }


    return 0;
}

MSH_CMD_EXPORT(apa_set_mode, set analog pa mode);



