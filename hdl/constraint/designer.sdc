################################################################################
#  DESIGN "TOPLEVEL";
#  VENDOR "Actel";
################################################################################


set sdc_version 1.7


########  Clock Constraints  ########

create_clock  -name { MAINXIN } -period 83.3333 -waveform { 0.000 41.6667  }  { MSS_CORE_0/MSS_CCC_0/I_XTLOSC:CLKOUT }

create_clock  -name { RMII_REF_CLK } -period 20.000 -waveform { 0.000 10.000  }  { RMII_REF_CLK }



########  Generated Clock Constraints  ########

# MSS FCLK

create_generated_clock  -name { mss_ccc_gla0 } -divide_by 3  -multiply_by 20  -source { MSS_CORE_0/MSS_CCC_0/I_MSSCCC/U_MSSCCC:CLKA } { MSS_CORE_0/MSS_CCC_0/I_MSSCCC/U_MSSCCC:GLAMSS  } 

# FAB_CLK

create_generated_clock  -name { mss_ccc_glb } -divide_by 6  -multiply_by 20  -source { MSS_CORE_0/MSS_CCC_0/I_MSSCCC/U_MSSCCC:CLKA } { MSS_CORE_0/MSS_CCC_0/I_MSSCCC/U_MSSCCC:GLB  }
 
# MSS generated clocks

create_generated_clock  -name { mss_fabric_interface_clock } -divide_by 2 -multiply_by 1 -source { MSS_CORE_0/MSS_ADLIB_INST/U_CORE:FCLK } { MSS_CORE_0/MSS_ADLIB_INST/U_CORE:GLB  } 

create_generated_clock  -name { mss_aclk } -divide_by 2 -multiply_by 1  -source { MSS_CORE_0/MSS_ADLIB_INST/U_CORE:FCLK } { MSS_CORE_0/MSS_ADLIB_INST/U_CORE:ACLK  } 

create_generated_clock  -name { mss_pclk1 } -divide_by 4 -multiply_by 1  -source { MSS_CORE_0/MSS_ADLIB_INST/U_CORE:FCLK } { MSS_CORE_0/MSS_ADLIB_INST/U_CORE:PCLK1 } 



########  Clock Source Latency Constraints #########



########  Input Delay Constraints  ########



########  Output Delay Constraints  ########



########   Delay Constraints  ########



########   Delay Constraints  ########


########   Multicycle Constraints  ########




########   False Path Constraints  ########



########   Output load Constraints  ########



########  Disable Timing Constraints #########



########  Clock Uncertainty Constraints #########


