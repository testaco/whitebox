module whitebox_toplevel(
    resetn,
    pclk,
    paddr,
    psel,
    penable,
    pwrite,
    pwdata,
    pready,
    prdata,
    dac_clock,
    dac2x_clock,
    dac_en,
    dac_data,
    adc_idata,
    adc_qdata,
    tx_status_led,
    tx_dmaready,
    rx_status_led,
    rx_dmaready
);
    input resetn;
    input dac2x_clock;
    input dac_clock;
    input pclk;
    input [31:0] paddr;
    input psel;
    input penable;
    input pwrite;
    input [31:0] pwdata;
    input [9:0] adc_idata;
    input [9:0] adc_qdata;
    output pready;
    output [31:0] prdata;
    output [9:0] dac_data;
    output dac_en;
    output tx_status_led;
    output tx_dmaready;
    output rx_status_led;
    output rx_dmaready;

    wire clear_enable;
    wire clearn;
    wire tx_fifo_re;
    wire [31:0] tx_fifo_rdata;
    wire tx_fifo_we;
    wire [31:0] tx_fifo_wdata;
    wire tx_fifo_full;
    wire tx_fifo_empty;
    wire tx_fifo_afull;
    wire tx_fifo_aempty;
    wire [9:0] tx_fifo_afval;
    wire [9:0] tx_fifo_aeval;
    wire tx_fifo_wack;
    wire tx_fifo_dvld;
    wire tx_fifo_overflow;
    wire tx_fifo_underflow;
    wire [10:0] tx_fifo_rdcnt;
    wire [10:0] tx_fifo_wrcnt;

    wire rx_fifo_re;
    wire [31:0] rx_fifo_rdata;
    wire rx_fifo_we;
    wire [31:0] rx_fifo_wdata;
    wire rx_fifo_full;
    wire rx_fifo_empty;
    wire rx_fifo_afull;
    wire rx_fifo_aempty;
    wire [9:0] rx_fifo_afval;
    wire [9:0] rx_fifo_aeval;
    wire rx_fifo_wack;
    wire rx_fifo_dvld;
    wire rx_fifo_overflow;
    wire rx_fifo_underflow;
    wire [10:0] rx_fifo_rdcnt;
    wire [10:0] rx_fifo_wrcnt;

    wire [8:0] fir_coeff_ram_addr;
    wire [8:0] fir_coeff_ram_din;
    wire fir_coeff_ram_width0 = 1;
    wire fir_coeff_ram_width1 = 1;
    wire fir_coeff_ram_pipe = 1;
    wire fir_coeff_ram_wmode = 0;
    wire fir_coeff_ram_blk;
    wire fir_coeff_ram_wen;
    wire fir_coeff_ram_clk = dac_clock;
    wire [8:0] fir_coeff_ram_dout;

    wire [8:0] fir_load_coeff_ram_addr;
    wire [8:0] fir_load_coeff_ram_din;
    wire fir_load_coeff_ram_width0 = 1;
    wire fir_load_coeff_ram_width1 = 1;
    wire fir_load_coeff_ram_pipe = 1;
    wire fir_load_coeff_ram_wmode = 0;
    wire fir_load_coeff_ram_blk;
    wire fir_load_coeff_ram_wen;
    wire fir_load_coeff_ram_clk = pclk;
    wire [8:0] fir_load_coeff_ram_dout;

    wire [8:0] fir_delay_line_i_ram_addr;
    wire [8:0] fir_delay_line_i_ram_din;
    wire fir_delay_line_i_ram_width0 = 1;
    wire fir_delay_line_i_ram_width1 = 1;
    wire fir_delay_line_i_ram_pipe = 1;
    wire fir_delay_line_i_ram_wmode = 0;
    wire fir_delay_line_i_ram_blk;
    wire fir_delay_line_i_ram_wen;
    wire fir_delay_line_i_ram_clk = dac_clock;
    wire [8:0] fir_delay_line_i_ram_dout;
    wire [8:0] fir_delay_line_i_ram_dout_b;

    wire [8:0] fir_delay_line_q_ram_addr;
    wire [8:0] fir_delay_line_q_ram_din;
    wire fir_delay_line_q_ram_width0 = 1;
    wire fir_delay_line_q_ram_width1 = 1;
    wire fir_delay_line_q_ram_pipe = 1;
    wire fir_delay_line_q_ram_wmode = 0;
    wire fir_delay_line_q_ram_blk;
    wire fir_delay_line_q_ram_wen;
    wire fir_delay_line_q_ram_clk = dac_clock;
    wire [8:0] fir_delay_line_q_ram_dout;
    wire [8:0] fir_delay_line_q_ram_dout_b;

    // Whitebox Reset
    whitebox_reset whitebox_reset_0(
        .resetn(resetn),
        .dac_clock(dac_clock),
        .clear_enable(clear_enable),
        .clearn(clearn)
    );
    
    // tx fifo
    actel_soft_fifo_32_1024 tx_fifo(
        .DATA(tx_fifo_wdata),
        .Q(tx_fifo_rdata),
        .WE(tx_fifo_we),
        .RE(tx_fifo_re),
        .WCLOCK(pclk),
        .RCLOCK(dac_clock),
        .FULL(tx_fifo_full),
        .EMPTY(tx_fifo_empty),
        .RESET(clearn),
        .AEMPTY(tx_fifo_aempty),
        .AFULL(tx_fifo_afull),
        .AFVAL(tx_fifo_afval),
        .AEVAL(tx_fifo_aeval),
        .WACK(tx_fifo_wack),
        .DVLD(tx_fifo_dvld),
        .OVERFLOW(tx_fifo_overflow),
        .UNDERFLOW(tx_fifo_underflow),
        .RDCNT(tx_fifo_rdcnt),
        .WRCNT(tx_fifo_wrcnt)
    );

    // rx fifo
    actel_soft_fifo_32_1024 rx_fifo(
        .DATA(rx_fifo_wdata),
        .Q(rx_fifo_rdata),
        .WE(rx_fifo_we),
        .RE(rx_fifo_re),
        .WCLOCK(dac_clock),
        .RCLOCK(pclk),
        .FULL(rx_fifo_full),
        .EMPTY(rx_fifo_empty),
        .RESET(clearn),
        .AEMPTY(rx_fifo_aempty),
        .AFULL(rx_fifo_afull),
        .AFVAL(rx_fifo_afval),
        .AEVAL(rx_fifo_aeval),
        .WACK(rx_fifo_wack),
        .DVLD(rx_fifo_dvld),
        .OVERFLOW(rx_fifo_overflow),
        .UNDERFLOW(rx_fifo_underflow),
        .RDCNT(rx_fifo_rdcnt),
        .WRCNT(rx_fifo_wrcnt)
    );
    
    // FIR COEFFICIENT RAM
    RAM4K9 fir_coeff_ram(
        .RESET(clearn),
        .ADDRA8(fir_coeff_ram_addr[8]), .ADDRA7(fir_coeff_ram_addr[7]),
        .ADDRA6(fir_coeff_ram_addr[6]), .ADDRA5(fir_coeff_ram_addr[5]),
        .ADDRA4(fir_coeff_ram_addr[4]), .ADDRA3(fir_coeff_ram_addr[3]),
        .ADDRA2(fir_coeff_ram_addr[2]), .ADDRA1(fir_coeff_ram_addr[1]),
        .ADDRA0(fir_coeff_ram_addr[0]),
        .DINA8(fir_coeff_ram_din[8]), .DINA7(fir_coeff_ram_din[7]),
        .DINA6(fir_coeff_ram_din[6]), .DINA5(fir_coeff_ram_din[5]),
        .DINA4(fir_coeff_ram_din[4]), .DINA3(fir_coeff_ram_din[3]),
        .DINA2(fir_coeff_ram_din[2]), .DINA1(fir_coeff_ram_din[1]),
        .DINA0(fir_coeff_ram_din[0]),
        .WIDTHA0(fir_coeff_ram_width0),
        .WIDTHA1(fir_coeff_ram_width1),
        .PIPEA(fir_coeff_ram_pipe),
        .WMODEA(fir_coeff_ram_wmode),
        .BLKA(fir_coeff_ram_blk),
        .WENA(fir_coeff_ram_wen),
        .CLKA(fir_coeff_ram_clk),
        .DOUTA8(fir_coeff_ram_dout[8]), .DOUTA7(fir_coeff_ram_dout[7]),
        .DOUTA6(fir_coeff_ram_dout[6]), .DOUTA5(fir_coeff_ram_dout[5]),
        .DOUTA4(fir_coeff_ram_dout[4]), .DOUTA3(fir_coeff_ram_dout[3]),
        .DOUTA2(fir_coeff_ram_dout[2]), .DOUTA1(fir_coeff_ram_dout[1]),
        .DOUTA0(fir_coeff_ram_dout[0]),
        .ADDRB8(fir_load_coeff_ram_addr[8]), .ADDRB7(fir_load_coeff_ram_addr[7]),
        .ADDRB6(fir_load_coeff_ram_addr[6]), .ADDRB5(fir_load_coeff_ram_addr[5]),
        .ADDRB4(fir_load_coeff_ram_addr[4]), .ADDRB3(fir_load_coeff_ram_addr[3]),
        .ADDRB2(fir_load_coeff_ram_addr[2]), .ADDRB1(fir_load_coeff_ram_addr[1]),
        .ADDRB0(fir_load_coeff_ram_addr[0]),
        .DINB8(fir_load_coeff_ram_din[8]), .DINB7(fir_load_coeff_ram_din[7]),
        .DINB6(fir_load_coeff_ram_din[6]), .DINB5(fir_load_coeff_ram_din[5]),
        .DINB4(fir_load_coeff_ram_din[4]), .DINB3(fir_load_coeff_ram_din[3]),
        .DINB2(fir_load_coeff_ram_din[2]), .DINB1(fir_load_coeff_ram_din[1]),
        .DINB0(fir_load_coeff_ram_din[0]),
        .WIDTHB0(fir_load_coeff_ram_width0),
        .WIDTHB1(fir_load_coeff_ram_width1),
        .PIPEB(fir_load_coeff_ram_pipe),
        .WMODEB(fir_load_coeff_ram_wmode),
        .BLKB(fir_load_coeff_ram_blk),
        .WENB(fir_load_coeff_ram_wen),
        .CLKB(fir_load_coeff_ram_clk),
        .DOUTB8(fir_load_coeff_ram_dout[8]), .DOUTB7(fir_load_coeff_ram_dout[7]),
        .DOUTB6(fir_load_coeff_ram_dout[6]), .DOUTB5(fir_load_coeff_ram_dout[5]),
        .DOUTB4(fir_load_coeff_ram_dout[4]), .DOUTB3(fir_load_coeff_ram_dout[3]),
        .DOUTB2(fir_load_coeff_ram_dout[2]), .DOUTB1(fir_load_coeff_ram_dout[1]),
        .DOUTB0(fir_load_coeff_ram_dout[0])
    );

    // FIR REAL DELAY LINE RAM
    RAM4K9 fir_delay_line_i_ram(
        .RESET(clearn),
        .ADDRA8(fir_delay_line_i_ram_addr[8]), .ADDRA7(fir_delay_line_i_ram_addr[7]),
        .ADDRA6(fir_delay_line_i_ram_addr[6]), .ADDRA5(fir_delay_line_i_ram_addr[5]),
        .ADDRA4(fir_delay_line_i_ram_addr[4]), .ADDRA3(fir_delay_line_i_ram_addr[3]),
        .ADDRA2(fir_delay_line_i_ram_addr[2]), .ADDRA1(fir_delay_line_i_ram_addr[1]),
        .ADDRA0(fir_delay_line_i_ram_addr[0]),
        .DINA8(fir_delay_line_i_ram_din[8]), .DINA7(fir_delay_line_i_ram_din[7]),
        .DINA6(fir_delay_line_i_ram_din[6]), .DINA5(fir_delay_line_i_ram_din[5]),
        .DINA4(fir_delay_line_i_ram_din[4]), .DINA3(fir_delay_line_i_ram_din[3]),
        .DINA2(fir_delay_line_i_ram_din[2]), .DINA1(fir_delay_line_i_ram_din[1]),
        .DINA0(fir_delay_line_i_ram_din[0]),
        .WIDTHA0(fir_delay_line_i_ram_width0),
        .WIDTHA1(fir_delay_line_i_ram_width1),
        .PIPEA(fir_delay_line_i_ram_pipe),
        .WMODEA(fir_delay_line_i_ram_wmode),
        .BLKA(fir_delay_line_i_ram_blk),
        .WENA(fir_delay_line_i_ram_wen),
        .CLKA(fir_delay_line_i_ram_clk),
        .DOUTA8(fir_delay_line_i_ram_dout[8]), .DOUTA7(fir_delay_line_i_ram_dout[7]),
        .DOUTA6(fir_delay_line_i_ram_dout[6]), .DOUTA5(fir_delay_line_i_ram_dout[5]),
        .DOUTA4(fir_delay_line_i_ram_dout[4]), .DOUTA3(fir_delay_line_i_ram_dout[3]),
        .DOUTA2(fir_delay_line_i_ram_dout[2]), .DOUTA1(fir_delay_line_i_ram_dout[1]),
        .DOUTA0(fir_delay_line_i_ram_dout[0]),
        .DOUTB8(fir_delay_line_i_ram_dout_b[8]), .DOUTB7(fir_delay_line_i_ram_dout_b[7]),
        .DOUTB6(fir_delay_line_i_ram_dout_b[6]), .DOUTB5(fir_delay_line_i_ram_dout_b[5]),
        .DOUTB4(fir_delay_line_i_ram_dout_b[4]), .DOUTB3(fir_delay_line_i_ram_dout_b[3]),
        .DOUTB2(fir_delay_line_i_ram_dout_b[2]), .DOUTB1(fir_delay_line_i_ram_dout_b[1]),
        .DOUTB0(fir_delay_line_i_ram_dout_b[0])
    );

    // FIR IMAGINARY DELAY LINE RAM
    RAM4K9 fir_delay_line_q_ram(
        .RESET(clearn),
        .ADDRA8(fir_delay_line_q_ram_addr[8]), .ADDRA7(fir_delay_line_q_ram_addr[7]),
        .ADDRA6(fir_delay_line_q_ram_addr[6]), .ADDRA5(fir_delay_line_q_ram_addr[5]),
        .ADDRA4(fir_delay_line_q_ram_addr[4]), .ADDRA3(fir_delay_line_q_ram_addr[3]),
        .ADDRA2(fir_delay_line_q_ram_addr[2]), .ADDRA1(fir_delay_line_q_ram_addr[1]),
        .ADDRA0(fir_delay_line_q_ram_addr[0]),
        .DINA8(fir_delay_line_q_ram_din[8]), .DINA7(fir_delay_line_q_ram_din[7]),
        .DINA6(fir_delay_line_q_ram_din[6]), .DINA5(fir_delay_line_q_ram_din[5]),
        .DINA4(fir_delay_line_q_ram_din[4]), .DINA3(fir_delay_line_q_ram_din[3]),
        .DINA2(fir_delay_line_q_ram_din[2]), .DINA1(fir_delay_line_q_ram_din[1]),
        .DINA0(fir_delay_line_q_ram_din[0]),
        .WIDTHA0(fir_delay_line_q_ram_width0),
        .WIDTHA1(fir_delay_line_q_ram_width1),
        .PIPEA(fir_delay_line_q_ram_pipe),
        .WMODEA(fir_delay_line_q_ram_wmode),
        .BLKA(fir_delay_line_q_ram_blk),
        .WENA(fir_delay_line_q_ram_wen),
        .CLKA(fir_delay_line_q_ram_clk),
        .DOUTA8(fir_delay_line_q_ram_dout[8]), .DOUTA7(fir_delay_line_q_ram_dout[7]),
        .DOUTA6(fir_delay_line_q_ram_dout[6]), .DOUTA5(fir_delay_line_q_ram_dout[5]),
        .DOUTA4(fir_delay_line_q_ram_dout[4]), .DOUTA3(fir_delay_line_q_ram_dout[3]),
        .DOUTA2(fir_delay_line_q_ram_dout[2]), .DOUTA1(fir_delay_line_q_ram_dout[1]),
        .DOUTA0(fir_delay_line_q_ram_dout[0]),
        .DOUTB8(fir_delay_line_q_ram_dout_b[8]), .DOUTB7(fir_delay_line_q_ram_dout_b[7]),
        .DOUTB6(fir_delay_line_q_ram_dout_b[6]), .DOUTB5(fir_delay_line_q_ram_dout_b[5]),
        .DOUTB4(fir_delay_line_q_ram_dout_b[4]), .DOUTB3(fir_delay_line_q_ram_dout_b[3]),
        .DOUTB2(fir_delay_line_q_ram_dout_b[2]), .DOUTB1(fir_delay_line_q_ram_dout_b[1]),
        .DOUTB0(fir_delay_line_q_ram_dout_b[0])
    );

    // APB3 WRAPPER, DUC, DDC
    whitebox whitebox_0(
        .resetn(resetn),
        .pclk(pclk),
        .paddr(paddr),
        .psel(psel),
        .penable(penable),
        .pwrite(pwrite),
        .pwdata(pwdata),
        .pready(pready),
        .prdata(prdata),
        .clearn(clearn),
        .clear_enable(clear_enable),
        .dac_clock(dac_clock),
        .dac2x_clock(dac2x_clock),
        .dac_en(dac_en),
        .dac_data(dac_data),
        .adc_idata(adc_idata),
        .adc_qdata(adc_qdata),
        .tx_status_led(tx_status_led),
        .tx_dmaready(tx_dmaready),
        .rx_status_led(rx_status_led),
        .rx_dmaready(rx_dmaready),
        .tx_fifo_re(tx_fifo_re),
        .tx_fifo_rdata(tx_fifo_rdata),
        .tx_fifo_we(tx_fifo_we),
        .tx_fifo_wdata(tx_fifo_wdata),
        .tx_fifo_full(tx_fifo_full),
        .tx_fifo_empty(tx_fifo_empty),
        .tx_fifo_afull(tx_fifo_afull),
        .tx_fifo_aempty(tx_fifo_aempty),
        .tx_fifo_afval(tx_fifo_afval),
        .tx_fifo_aeval(tx_fifo_aeval),
        .tx_fifo_wack(tx_fifo_wack),
        .tx_fifo_dvld(tx_fifo_dvld),
        .tx_fifo_overflow(tx_fifo_overflow),
        .tx_fifo_underflow(tx_fifo_underflow),
        .tx_fifo_rdcnt(tx_fifo_rdcnt),
        .tx_fifo_wrcnt(tx_fifo_wrcnt),
        .rx_fifo_re(rx_fifo_re),
        .rx_fifo_rdata(rx_fifo_rdata),
        .rx_fifo_we(rx_fifo_we),
        .rx_fifo_wdata(rx_fifo_wdata),
        .rx_fifo_full(rx_fifo_full),
        .rx_fifo_empty(rx_fifo_empty),
        .rx_fifo_afull(rx_fifo_afull),
        .rx_fifo_aempty(rx_fifo_aempty),
        .rx_fifo_afval(rx_fifo_afval),
        .rx_fifo_aeval(rx_fifo_aeval),
        .rx_fifo_wack(rx_fifo_wack),
        .rx_fifo_dvld(rx_fifo_dvld),
        .rx_fifo_overflow(rx_fifo_overflow),
        .rx_fifo_underflow(rx_fifo_underflow),
        .rx_fifo_rdcnt(rx_fifo_rdcnt),
        .rx_fifo_wrcnt(rx_fifo_wrcnt),
        .fir_coeff_ram_addr(fir_coeff_ram_addr),
        .fir_coeff_ram_din(fir_coeff_ram_din),
        .fir_coeff_ram_blk(fir_coeff_ram_blk),
        .fir_coeff_ram_wen(fir_coeff_ram_wen),
        .fir_coeff_ram_dout(fir_coeff_ram_dout),
        .fir_load_coeff_ram_addr(fir_load_coeff_ram_addr),
        .fir_load_coeff_ram_din(fir_load_coeff_ram_din),
        .fir_load_coeff_ram_blk(fir_load_coeff_ram_blk),
        .fir_load_coeff_ram_wen(fir_load_coeff_ram_wen),
        .fir_load_coeff_ram_dout(fir_load_coeff_ram_dout),
        .fir_delay_line_i_ram_addr(fir_delay_line_i_ram_addr),
        .fir_delay_line_i_ram_din(fir_delay_line_i_ram_din),
        .fir_delay_line_i_ram_blk(fir_delay_line_i_ram_blk),
        .fir_delay_line_i_ram_wen(fir_delay_line_i_ram_wen),
        .fir_delay_line_i_ram_dout(fir_delay_line_i_ram_dout),
        .fir_delay_line_q_ram_addr(fir_delay_line_q_ram_addr),
        .fir_delay_line_q_ram_din(fir_delay_line_q_ram_din),
        .fir_delay_line_q_ram_blk(fir_delay_line_q_ram_blk),
        .fir_delay_line_q_ram_wen(fir_delay_line_q_ram_wen),
        .fir_delay_line_q_ram_dout(fir_delay_line_q_ram_dout)
    );
endmodule
