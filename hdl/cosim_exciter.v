module cosim_exciter;

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
reg pslverr;
reg dac_clock;
wire [9:0] dac_data;
wire dac_en;
wire status_led;
wire dmaready;
wire txirq;
wire clear_enable;

wire clearn;
wire fifo_re;
reg fifo_rclk;
reg [31:0] fifo_rdata;
wire fifo_we;
reg fifo_wclk;
wire [31:0] fifo_wdata;
reg fifo_full;
reg fifo_afull;
reg fifo_empty;
reg fifo_aempty;
wire [11:0] fifo_afval;
wire [11:0] fifo_aeval;

initial begin
    $dumpfile({`COSIM_NAME, ".vcd"});
    $dumpvars;
    $from_myhdl(resetn,
            dac2x_clock, dac_clock,
            pclk, paddr, psel, penable, pwrite, pwdata,
            fifo_rclk, fifo_rdata, fifo_wclk,
            fifo_full, fifo_afull, fifo_empty, fifo_aempty);
    $to_myhdl(pready, prdata, pslverr,
            dac_data, dac_en,
            status_led,
            dmaready, txirq, clear_enable,
            clearn, fifo_re, fifo_we, fifo_wdata, fifo_afval, fifo_aeval);
end

exciter_reset er (
    .resetn(resetn),
    .dac_clock(dac_clock),
    .clear_enable(clear_enable),
    .clearn(clearn)
);

exciter e (
    .resetn(resetn),
    .clearn(clearn),
    .dac2x_clock(dac2x_clock),
    .pclk(pclk),
    .paddr(paddr),
    .psel(psel),
    .penable(penable),
    .pwrite(pwrite),
    .pwdata(pwdata),
    .pready(pready),
    .prdata(prdata),
    //.pslverr(pslverr),
    .dac_clock(dac_clock),
    .dac_data(dac_data),
    .dac_en(dac_en),
    .status_led(status_led),
    .dmaready(dmaready),
    .txirq(txirq),
    .fifo_re(fifo_re),
    .fifo_rclk(fifo_rclk),
    .fifo_rdata(fifo_rdata),
    .fifo_we(fifo_we),
    .fifo_wclk(fifo_wclk),
    .fifo_wdata(fifo_wdata),
    .fifo_full(fifo_full),
    .fifo_afull(fifo_afull),
    .fifo_empty(fifo_empty),
    .fifo_aempty(fifo_aempty),
    .fifo_afval(fifo_afval),
    .fifo_aeval(fifo_aeval),
    .clear_enable(clear_enable)
);

endmodule
