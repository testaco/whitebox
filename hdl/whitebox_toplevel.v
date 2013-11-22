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
    wire [11:0] tx_fifo_afval;
    wire [11:0] tx_fifo_aeval;
    wire rx_fifo_re;
    wire [31:0] rx_fifo_rdata;
    wire rx_fifo_we;
    wire [31:0] rx_fifo_wdata;
    wire rx_fifo_full;
    wire rx_fifo_empty;
    wire rx_fifo_afull;
    wire rx_fifo_aempty;
    wire [11:0] rx_fifo_afval;
    wire [11:0] rx_fifo_aeval;

    // Whitebox Reset
    whitebox_reset whitebox_reset_0(
        .resetn(resetn),
        .dac_clock(dac_clock),
        .clear_enable(clear_enable),
        .clearn(clearn)
    );
    // tx fifo
    actel_fifo_32_1024 tx_fifo(
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
        .AEVAL(tx_fifo_aeval)
    );
    // rx fifo
    actel_fifo_32_1024 rx_fifo(
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
        .AEVAL(rx_fifo_aeval)
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
        .rx_fifo_re(rx_fifo_re),
        .rx_fifo_rdata(rx_fifo_rdata),
        .rx_fifo_we(rx_fifo_we),
        .rx_fifo_wdata(rx_fifo_wdata),
        .rx_fifo_full(rx_fifo_full),
        .rx_fifo_empty(rx_fifo_empty),
        .rx_fifo_afull(rx_fifo_afull),
        .rx_fifo_aempty(rx_fifo_aempty),
        .rx_fifo_afval(rx_fifo_afval),
        .rx_fifo_aeval(rx_fifo_aeval)
    );
endmodule
