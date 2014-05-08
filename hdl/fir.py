from myhdl import Signal, always, always_comb, always_seq, \
                  intbv, enum, concat, modbv, ResetSignal

def fir(clearn, clock, in_sign, out_sign,
        coeff_ram_addr,
        coeff_ram_din0,
        coeff_ram_din1,
        coeff_ram_blk,
        coeff_ram_wen,
        coeff_ram_dout0,
        coeff_ram_dout1,
        delay_line_i_ram_addr,
        delay_line_i_ram_din,
        delay_line_i_ram_blk,
        delay_line_i_ram_wen,
        delay_line_i_ram_dout,
        delay_line_q_ram_addr,
        delay_line_q_ram_din,
        delay_line_q_ram_blk,
        delay_line_q_ram_wen,
        delay_line_q_ram_dout,
        enable, bank1, bank0, N,
        **kwargs):
    
    state_t = enum('READY', 'WAIT1', 'WAIT2', 'WAIT3', 'RUN', 'DONE')
    state = Signal(state_t.READY)

    coeff_width = len(coeff_ram_dout1) + len(coeff_ram_dout0)
    delay_line_width = len(delay_line_i_ram_dout)
    mo = coeff_width + delay_line_width - 1
    ao = mo + 1

    in_valid = in_sign.valid
    in_last = in_sign.last
    in_i = in_sign.i
    in_q = in_sign.q

    out_valid = out_sign.valid
    out_last = out_sign.last
    out_i = out_sign.i
    out_q = out_sign.q

    last = Signal(bool(0))
    n = Signal(modbv(0, min=0, max=2**7))
    k = Signal(modbv(0, min=0, max=2**7))
    m_i = Signal(intbv(0, min=-2**mo, max=2**mo))
    m_q = Signal(intbv(0, min=-2**mo, max=2**mo))
    ac_i = Signal(intbv(0, min=-2**ao, max=2**ao))
    ac_q = Signal(intbv(0, min=-2**ao, max=2**ao))

    @always_seq(clock.posedge, reset=clearn)
    def accessor():
        if state == state_t.READY:
            coeff_ram_blk.next = True
            if enable and in_valid:
                delay_line_i_ram_addr.next = concat(bank1, bank0, n)
                delay_line_i_ram_din.next = in_i
                delay_line_i_ram_blk.next = False
                delay_line_i_ram_wen.next = False
                delay_line_q_ram_addr.next = concat(bank1, bank0, n)
                delay_line_q_ram_din.next = in_q
                delay_line_q_ram_blk.next = False
                delay_line_q_ram_wen.next = False
            else:
                delay_line_i_ram_blk.next = True
                delay_line_q_ram_blk.next = True
                
        elif state == state_t.WAIT1 or state == state_t.WAIT2 or state == state_t.WAIT3 or state == state_t.RUN:
            delay_line_i_ram_addr.next = concat(bank1, bank0, modbv(n - k, min=0, max=2**7-1))
            delay_line_i_ram_blk.next = False
            delay_line_i_ram_wen.next = True
            delay_line_q_ram_addr.next = concat(bank1, bank0, modbv(n - k, min=0, max=2**7-1))
            delay_line_q_ram_blk.next = False
            delay_line_q_ram_wen.next = True
            coeff_ram_addr.next = concat(bank1, bank0, k)
            coeff_ram_blk.next = False
            coeff_ram_wen.next = True
        else:
            delay_line_i_ram_blk.next = True
            delay_line_q_ram_blk.next = True
            coeff_ram_blk.next = True

    @always_seq(clock.posedge, reset=clearn)
    def operator():
        if state == state_t.READY:
            k.next = 0
            ac_i.next = 0
            if not enable:
                out_valid.next = in_valid
                out_last.next = in_last
                out_i.next = in_i
                out_q.next = in_q
            elif in_valid:
                last.next = in_last
                state.next = state_t.WAIT1
                out_valid.next = False
            else:
                out_valid.next = False
                out_last.next = False
                out_i.next = 0
                out_q.next = 0
        elif state == state_t.DONE:
            out_valid.next = True
            out_last.next = last
            out_i.next = ac_i[len(ac_i):len(ac_i)-len(out_i)].signed()
            out_q.next = ac_q[len(ac_q):len(ac_q)-len(out_q)].signed()
            state.next = state_t.READY
            n.next = n + 1
            ac_i.next = 0
            ac_q.next = 0
            k.next = 0
        else:
            k.next = k + 1
            m_i.next = concat(coeff_ram_dout1, coeff_ram_dout0).signed() * delay_line_i_ram_dout.signed()
            m_q.next = concat(coeff_ram_dout1, coeff_ram_dout0).signed() * delay_line_q_ram_dout.signed()
            if state == state_t.RUN:
                ac_i.next = ac_i + m_i
                ac_q.next = ac_q + m_q
            elif state == state_t.WAIT1:
                state.next = state_t.WAIT2
            elif state == state_t.WAIT2:
                state.next = state_t.WAIT3
            elif state == state_t.WAIT3:
                state.next = state_t.RUN

            if k == N + 3:
                state.next = state_t.DONE

    return accessor, operator
