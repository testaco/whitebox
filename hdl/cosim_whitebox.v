module cosim_whitebox;

reg resetn;
reg dsp_clock;
reg pclk;
reg [31:0] paddr;
reg psel;
reg psel_streamer;
reg psel_converter;
reg psel_modem;
reg psel_tuner;
reg penable;
reg pwrite;
reg [31:0] pwdata;
wire pready;
wire pready_streamer;
wire pready_converter;
wire pready_modem;
wire pready_tuner;
wire [31:0] prdata;
wire [31:0] prdata_streamer;
wire [31:0] prdata_converter;
wire [31:0] prdata_modem;
wire [31:0] prdata_tuner;
//reg pslverr;
reg dac_clock;
wire [9:0] dac_data;
wire dac_en;
reg [9:0] adc_idata;
reg [9:0] adc_qdata;
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

initial begin
    $dumpfile({`COSIM_NAME, ".vcd"});
    $dumpvars;
    $from_myhdl(resetn,
            dsp_clock, dac_clock,
            pclk, paddr, psel, penable, pwrite, pwdata,
            psel_streamer, psel_modem, psel_converter, psel_tuner,
            tx_fifo_rdata, tx_fifo_full, tx_fifo_afull, tx_fifo_empty, tx_fifo_aempty,
            tx_fifo_wack, tx_fifo_dvld, tx_fifo_overflow, tx_fifo_underflow,
            tx_fifo_rdcnt, tx_fifo_wrcnt,
            tx_fifo_rclk, tx_fifo_wclk,
            rx_fifo_rdata, rx_fifo_full, rx_fifo_afull, rx_fifo_empty, rx_fifo_aempty,
            rx_fifo_wack, rx_fifo_dvld, rx_fifo_overflow, rx_fifo_underflow,
            rx_fifo_rdcnt, rx_fifo_wrcnt,
            rx_fifo_rclk, rx_fifo_wclk,
            adc_idata, adc_qdata);
    $to_myhdl(pready, prdata, //pslverr,
            pready_streamer, pready_modem, pready_converter, pready_tuner,
            prdata_streamer, prdata_modem, prdata_converter, prdata_tuner,
            dac_data, dac_en,
            tx_status_led, tx_dmaready,
            rx_status_led, rx_dmaready,
            clearn, clear_enable,
            tx_fifo_re, tx_fifo_we, tx_fifo_wdata,
            tx_fifo_afval, tx_fifo_aeval,
            rx_fifo_re, rx_fifo_we, rx_fifo_wdata,
            rx_fifo_afval, rx_fifo_aeval);
end

whitebox_reset whitebox_reset_0 (
    .resetn(resetn),
    .dsp_clock(dsp_clock),
    .clear_enable(clear_enable),
    .clearn(clearn)
);

whitebox whitebox_0 (
    .resetn(resetn),
    .pclk(pclk),
    .paddr(paddr),
    .psel(psel),
    .psel_streamer(psel_streamer),
    .psel_modem(psel_modem),
    .psel_tuner(psel_tuner),
    .psel_converter(psel_converter),
    .penable(penable),
    .pwrite(pwrite),
    .pwdata(pwdata),
    .pready(pready),
    .pready_streamer(pready_streamer),
    .pready_modem(pready_modem),
    .pready_tuner(pready_tuner),
    .pready_converter(pready_converter),
    .prdata(prdata),
    .prdata_streamer(prdata_streamer),
    .prdata_modem(prdata_modem),
    .prdata_tuner(prdata_tuner),
    .prdata_converter(prdata_converter),
    //.pslverr(pslverr),

    .clearn(clearn),
    .clear_enable(clear_enable),
    .dac_clock(dac_clock),
    .dsp_clock(dsp_clock),
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
    .rx_fifo_wrcnt(rx_fifo_wrcnt)
);

endmodule
