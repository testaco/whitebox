module cosim_whitebox;

reg resetn;
reg dac2x_clock;
reg pclk;
reg [31:0] paddr;
reg psel;
reg penable;
reg pwrite;
reg [31:0] pwdata;
wire pready;
wire [31:0] prdata;
//reg pslverr;
reg dac_clock;
wire [9:0] dac_data;
wire dac_en;
wire [9:0] adc_idata;
wire [9:0] adc_qdata;
wire tx_status_led;
wire tx_dma_ready;
wire rx_status_led;
wire rx_dma_ready;
wire clear_enable;

wire tx_fifo_re;
reg [31:0] tx_fifo_rdata;
wire tx_fifo_we;
wire [31:0] tx_fifo_wdata;
reg tx_fifo_full;
reg tx_fifo_empty;
reg tx_fifo_afull;
reg tx_fifo_aempty;
wire [9:0] tx_fifo_afval;
wire [9:0] tx_fifo_aeval;
reg tx_fifo_wack;
reg tx_fifo_dvld;
reg tx_fifo_overflow;
reg tx_fifo_underflow;
reg [10:0] tx_fifo_rdcnt;
reg [10:0] tx_fifo_wrcnt;
reg tx_fifo_wclk;
reg tx_fifo_rclk;

wire rx_fifo_re;
reg [31:0] rx_fifo_rdata;
wire rx_fifo_we;
wire [31:0] rx_fifo_wdata;
reg rx_fifo_full;
reg rx_fifo_empty;
reg rx_fifo_afull;
reg rx_fifo_aempty;
wire [9:0] rx_fifo_afval;
wire [9:0] rx_fifo_aeval;
reg rx_fifo_wack;
reg rx_fifo_dvld;
reg rx_fifo_overflow;
reg rx_fifo_underflow;
reg [10:0] rx_fifo_rdcnt;
reg [10:0] rx_fifo_wrcnt;
reg rx_fifo_wclk;
reg rx_fifo_rclk;

wire [8:0] fir_coeff_ram_addr;
wire [8:0] fir_coeff_ram_din0;
wire [8:0] fir_coeff_ram_din1;
wire fir_coeff_ram_width0 = 1;
wire fir_coeff_ram_width1 = 1;
wire fir_coeff_ram_pipe = 1;
wire fir_coeff_ram_wmode = 0;
wire fir_coeff_ram_blk;
wire fir_coeff_ram_wen;
reg fir_coeff_ram_clk;
reg [8:0] fir_coeff_ram_dout0;
reg [8:0] fir_coeff_ram_dout1;

wire [8:0] fir_load_coeff_ram_addr;
wire [8:0] fir_load_coeff_ram_din0;
wire [8:0] fir_load_coeff_ram_din1;
wire fir_load_coeff_ram_width0 = 1;
wire fir_load_coeff_ram_width1 = 1;
wire fir_load_coeff_ram_pipe = 1;
wire fir_load_coeff_ram_wmode = 0;
wire fir_load_coeff_ram_blk;
wire fir_load_coeff_ram_wen;
reg fir_load_coeff_ram_clk;
reg [8:0] fir_load_coeff_ram_dout0;
reg [8:0] fir_load_coeff_ram_dout1;

wire [8:0] fir_delay_line_i_ram_addr;
wire [8:0] fir_delay_line_i_ram_din;
wire fir_delay_line_i_ram_width0 = 1;
wire fir_delay_line_i_ram_width1 = 1;
wire fir_delay_line_i_ram_pipe = 1;
wire fir_delay_line_i_ram_wmode = 0;
wire fir_delay_line_i_ram_blk;
wire fir_delay_line_i_ram_wen;
reg fir_delay_line_i_ram_clk;
reg [8:0] fir_delay_line_i_ram_dout;

wire [8:0] fir_delay_line_q_ram_addr;
wire [8:0] fir_delay_line_q_ram_din;
wire fir_delay_line_q_ram_width0 = 1;
wire fir_delay_line_q_ram_width1 = 1;
wire fir_delay_line_q_ram_pipe = 1;
wire fir_delay_line_q_ram_wmode = 0;
wire fir_delay_line_q_ram_blk;
wire fir_delay_line_q_ram_wen;
reg fir_delay_line_q_ram_clk;
reg [8:0] fir_delay_line_q_ram_dout;

initial begin
    $dumpfile({`COSIM_NAME, ".vcd"});
    $dumpvars;
    $from_myhdl(resetn,
            dac2x_clock, dac_clock,
            pclk, paddr, psel, penable, pwrite, pwdata,
            tx_fifo_rdata, tx_fifo_full, tx_fifo_afull, tx_fifo_empty, tx_fifo_aempty,
            tx_fifo_wack, tx_fifo_dvld, tx_fifo_overflow, tx_fifo_underflow,
            tx_fifo_rdcnt, tx_fifo_wrcnt,
            tx_fifo_rclk, tx_fifo_wclk,
            rx_fifo_rdata, rx_fifo_full, rx_fifo_afull, rx_fifo_empty, rx_fifo_aempty,
            rx_fifo_wack, rx_fifo_dvld, rx_fifo_overflow, rx_fifo_underflow,
            rx_fifo_rdcnt, rx_fifo_wrcnt,
            rx_fifo_rclk, rx_fifo_wclk,
            fir_coeff_ram_clk,
            fir_coeff_ram_dout0, fir_coeff_ram_dout1,
            fir_load_coeff_ram_clk,
            fir_load_coeff_ram_dout0, fir_load_coeff_ram_dout1,
            fir_delay_line_i_ram_clk, fir_delay_line_i_ram_dout,
            fir_delay_line_q_ram_clk, fir_delay_line_q_ram_dout);
    $to_myhdl(pready, prdata, //pslverr,
            dac_data, dac_en,
            tx_status_led, tx_dmaready,
            rx_status_led, rx_dmaready,
            clearn, clear_enable,
            tx_fifo_re, tx_fifo_we, tx_fifo_wdata,
            tx_fifo_afval, tx_fifo_aeval,
            rx_fifo_re, rx_fifo_we, rx_fifo_wdata,
            rx_fifo_afval, rx_fifo_aeval,
            fir_coeff_ram_addr, fir_coeff_ram_din0, fir_coeff_ram_din1, fir_coeff_ram_width0, fir_coeff_ram_width1, fir_coeff_ram_pipe, fir_coeff_ram_wmode, fir_coeff_ram_blk, fir_coeff_ram_wen,
            fir_load_coeff_ram_addr, fir_load_coeff_ram_din0, fir_load_coeff_ram_din1, fir_load_coeff_ram_width0, fir_load_coeff_ram_width1, fir_load_coeff_ram_pipe, fir_load_coeff_ram_wmode, fir_load_coeff_ram_blk, fir_load_coeff_ram_wen,
            fir_delay_line_i_ram_addr, fir_delay_line_i_ram_din, fir_delay_line_i_ram_width0, fir_delay_line_i_ram_width1, fir_delay_line_i_ram_pipe, fir_delay_line_i_ram_wmode, fir_delay_line_i_ram_blk, fir_delay_line_i_ram_wen,
            fir_delay_line_q_ram_addr, fir_delay_line_q_ram_din, fir_delay_line_q_ram_width0, fir_delay_line_q_ram_width1, fir_delay_line_q_ram_pipe, fir_delay_line_q_ram_wmode, fir_delay_line_q_ram_blk, fir_delay_line_q_ram_wen
            );
end

whitebox_reset whitebox_reset_0 (
    .resetn(resetn),
    .dac_clock(dac_clock),
    .clear_enable(clear_enable),
    .clearn(clearn)
);

whitebox whitebox_0 (
    .resetn(resetn),
    .pclk(pclk),
    .paddr(paddr),
    .psel(psel),
    .penable(penable),
    .pwrite(pwrite),
    .pwdata(pwdata),
    .pready(pready),
    .prdata(prdata),
    //.pslverr(pslverr),

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
    .fir_coeff_ram_din0(fir_coeff_ram_din0),
    .fir_coeff_ram_din1(fir_coeff_ram_din1),
    .fir_coeff_ram_blk(fir_coeff_ram_blk),
    .fir_coeff_ram_wen(fir_coeff_ram_wen),
    .fir_coeff_ram_dout0(fir_coeff_ram_dout0),
    .fir_coeff_ram_dout1(fir_coeff_ram_dout1),
    .fir_load_coeff_ram_addr(fir_load_coeff_ram_addr),
    .fir_load_coeff_ram_din0(fir_load_coeff_ram_din0),
    .fir_load_coeff_ram_din1(fir_load_coeff_ram_din1),
    .fir_load_coeff_ram_blk(fir_load_coeff_ram_blk),
    .fir_load_coeff_ram_wen(fir_load_coeff_ram_wen),
    .fir_load_coeff_ram_dout0(fir_load_coeff_ram_dout0),
    .fir_load_coeff_ram_dout1(fir_load_coeff_ram_dout1),
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
