//  Catapult University Version 2011a.126 (Production Release) Wed Aug  8 00:52:07 PDT 2012
//  
//  Copyright (c) Calypto Design Systems, Inc., 1996-2012, All Rights Reserved.
//                       UNPUBLISHED, LICENSED SOFTWARE.
//            CONFIDENTIAL AND PROPRIETARY INFORMATION WHICH IS THE
//          PROPERTY OF CALYPTO DESIGN SYSTEMS OR ITS LICENSORS
//  
//  Running on Windows XP student@FITKIT-PROJEKT Service Pack 3 5.01.2600 i686
//  
//  Package information: SIFLIBS v17.0_1.1, HLS_PKGS v17.0_1.1, 
//                       DesignPad v2.78_0.0
//  
//  This version may only be used for academic purposes.  Some optimizations 
//  are disabled, so results obtained from this version may be sub-optimal.
//  
project new
solution file add {./src_filter/filter.h} -type CHEADER
solution file add {./src_filter/filter.cpp} -type C++
directive set -REGISTER_IDLE_SIGNAL false
directive set -IDLE_SIGNAL {}
directive set -DONE_FLAG {}
directive set -START_FLAG {}
directive set -FSM_ENCODING none
directive set -REG_MAX_FANOUT 0
directive set -NO_X_ASSIGNMENTS true
directive set -SAFE_FSM false
directive set -RESET_CLEARS_ALL_REGS true
directive set -ASSIGN_OVERHEAD 0
directive set -DESIGN_GOAL area
directive set -OLD_SCHED false
directive set -PIPELINE_RAMP_UP true
directive set -COMPGRADE fast
directive set -SPECULATE true
directive set -MERGEABLE true
directive set -REGISTER_THRESHOLD 256
directive set -MEM_MAP_THRESHOLD 32
directive set -UNROLL no
directive set -CLOCK_OVERHEAD 20.000000
directive set -OPT_CONST_MULTS -1
go analyze
directive set -CLOCK_NAME clk
directive set -CLOCKS {clk {-CLOCK_PERIOD 40.0 -CLOCK_EDGE rising -CLOCK_UNCERTAINTY 0.0 -CLOCK_HIGH_TIME 20.0 -RESET_SYNC_NAME rst -RESET_ASYNC_NAME arst_n -RESET_KIND sync -RESET_SYNC_ACTIVE high -RESET_ASYNC_ACTIVE low -ENABLE_NAME {} -ENABLE_ACTIVE high}}
directive set -CLOCK_NAME clk
directive set -TECHLIBS {{mgc_Xilinx-SPARTAN3-4_beh_psr.lib {{mgc_Xilinx-SPARTAN3-4_beh_psr part 3s50tq144}}} {ram_Xilinx-SPARTAN3-4_RAMDB.lib ram_Xilinx-SPARTAN3-4_RAMDB} {ram_Xilinx-SPARTAN3-4_RAMSB.lib ram_Xilinx-SPARTAN3-4_RAMSB}}
directive set -DESIGN_HIERARCHY filter
directive set -TRANSACTION_DONE_SIGNAL true
go compile
directive set /filter/core/window:rsc -MAP_TO_MODULE {[Register]}
directive set /filter/core/system_input:window:rsc -MAP_TO_MODULE {[Register]}
directive set /filter/core/median:max:rsc -MAP_TO_MODULE {[Register]}
directive set /filter/core/median:max2:rsc -MAP_TO_MODULE {[Register]}
directive set /filter/core/buffer:buf:rsc -MAP_TO_MODULE ram_Xilinx-SPARTAN3-4_RAMSB.singleport
directive set /filter/core/histogram:rsc -MAP_TO_MODULE ram_Xilinx-SPARTAN3-4_RAMSB.singleport
directive set /filter -EFFORT_LEVEL high
directive set /filter/in_data:rsc -MAP_TO_MODULE mgc_ioport.mgc_in_wire_en
directive set /filter/out_data:rsc -MAP_TO_MODULE mgc_ioport.mgc_out_stdreg_en
directive set /filter/core/L1b -UNROLL yes
directive set /filter/core/L2 -UNROLL yes
directive set /filter/core/Linit -UNROLL yes
directive set /filter/core/L1a -UNROLL yes
directive set /filter/core/main -PIPELINE_INIT_INTERVAL 4
directive set /filter/mcu_data:rsc -MAP_TO_MODULE ram_Xilinx-SPARTAN3-4_RAMSB.singleport
go architect
go extract
options set Flows/Precision addio false
flow run /Precision/generate
flow run /Precision/precision -shell -file ./rtl.vhdl.psr -run_state mapped
