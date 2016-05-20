/**
 * Image filtering and segmentation on MSP430 and FPGA SPARTAN3
 * by Jakub Vojvoda, vojvoda@swdeveloper.sk
 * 2014
 *
 **/

#ifndef __FILTER__
#define __FILTER__

#include <ac_int.h>
#include "../../cpu/common.h"

#define MCU_SIZE        512

typedef ac_int<PIXEL_WIDTH,false> t_pixel;
typedef ac_int<32, false> t_mcu_data;

void filter(t_pixel &in_data, bool &in_data_vld, t_pixel &out_data, t_mcu_data mcu_data[MCU_SIZE]);

#endif
