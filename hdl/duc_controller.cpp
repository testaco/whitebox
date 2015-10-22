#include <sys/types.h>
#include <sys/stat.h>
#include <iostream>
#include <fstream>
#include <getopt.h>

#include "obj_dir/Vduc.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

#define TRACE false
#define EUNDERRUN -1

vluint64_t main_time = 0;       // Current simulation time
// This is a 64-bit integer to reduce wrap over issues and
// allow modulus.  You can also use a double, if you wish.

double sc_time_stamp () {       // Called by $time in Verilog
    return main_time;           // converts to double, to match
                                // what SystemC does
}

class Fifo {
public:
    Fifo() :
        size(10<<20),
        head(size-1),
        tail(0)
    {
        buf = new uint32_t[size];
        for (int i = 0; i < size; ++i)
            buf[i] = i << 16 | i;
    }

    ~Fifo() {
        delete[] buf;
    }

    virtual bool empty() {
        return (data_available() == 0);
    }

    virtual bool full() {
        return (space_available() == 0);
    }

    virtual uint32_t read_next() {
        return buf[tail++];
        tail &= size - 1;
    }

    virtual void write_next(uint32_t n) {
        buf[head++] = n;
        head &= size - 1;
    }

protected:
    int data_available() {
        return ((head - tail) & (size-1));
    }

    int space_available() {
        return ((tail - head + 1) & (size-1));
    }

private:
    const int size; // 1024
    uint32_t* buf;
    int head;
    int tail;
};

class FileFifo : public Fifo {
public:
    FileFifo(const char* filename) {
        f.open(filename, ios::binary | ios::in);
        if (!f)
            throw;
    }

    ~FileFifo() {
        f.close();
    }

    bool empty() {
        return f.eof();
    }

    uint32_t read_next() {
        uint32_t ret;
        if (f.eof()) return -1;
        f.read((char*)(&ret), sizeof(ret));
        return ret;
    }

private:
    std::ifstream f;
};

class Script {
public:
    Script(Vduc* d) :
        dut(d)
    {
        // made with
        // $ sox -n -r 10e6 -e signed -b 32 -c 2 -t raw - synth .02 0 0 sin 1e3 synth .02 0 90 sin 1e3
        tx_fifo = new FileFifo("/dev/stdin");
    }

    ~Script() {
        delete tx_fifo;
    }

    virtual int start() = 0;
    virtual int done() = 0;
    virtual int step() = 0;

protected:
    Vduc* dut;
    Fifo* tx_fifo;
};

class TestTxChain : public Script {
public:
    TestTxChain(Vduc* d) :
        Script(d)
        { }

    int start() {
        //std::cout << "start" << std::endl;
        dut->system_interp = 1;
        dut->system_txen = true;
        dut->system_txstop = true;
    }

    int done() {
        return Verilated::gotFinish() || dut->dac_last;
    }

    int step() {
        if (dut->underrun) {
            return -EUNDERRUN;
        }
        
        // First update the write FIFO
        if (dut->fifo_re) {
            if (tx_fifo->empty()) {
                dut->fifo_dvld = false;
                dut->fifo_underflow = true;
            } else {
                dut->fifo_dvld = true;
                dut->fifo_underflow = false;
                dut->fifo_rdata = tx_fifo->read_next();
            }
        } else {
            dut->fifo_dvld = false;
            dut->fifo_underflow = false;
        }
        dut->fifo_empty = tx_fifo->empty();

        //std::cout << "negedge @(dac_clock) " << main_time << std::endl;
        return 0;
    };
private:
};

static struct options {
    bool trace;
    bool gnuplot;
} global_options;

int parse(int argc, char** argv) {
    while (true) {
        static int trace_flag;
        static int gnuplot_flag;
        static struct option long_options[] = {
            {"trace", no_argument, &trace_flag, 't'},
            {"gnuplot", no_argument, &gnuplot_flag, 'g'},
            {0, 0, 0, 0}
        };
        /* getopt_long stores the option index here. */
        int option_index = 0;

        char c = getopt_long(argc, argv, "tg",
                         long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;

        switch (c)
        {
            case 't':
                //std::cout << "option " << long_options[option_index].name <<  " enabled" << std::endl;
                global_options.trace = true;
                break;
            case 'g':
                //std::cout << "option " << long_options[option_index].name <<  " enabled" << std::endl;
                global_options.gnuplot = true;
                break;
            default:
                //std::cout << "unkown option " << c << std::endl;
                break;
        }
    }
}

int main(int argc, char** argv) {
    // first parse the input arguments
    parse(argc, argv); 

    Verilated::commandArgs(argc, argv);   // Remember args
    Vduc* top = new Vduc;             // Create instance
    top->clearn = 0;           // Set some inputs
    int count = 0;
    int producer_state = 0;
    bool dac_clock_posedge = false;
    bool dac_clock_negedge = false;
    bool started = false;
    int samples_count = 0;

    Script* script = new TestTxChain(top);
    Fifo tx_fifo_out;
    uint32_t dac_sample;

    VerilatedVcdC* tfp = NULL;

    if (global_options.trace) {
        Verilated::traceEverOn(true);  // Turn on tracing
        tfp = new VerilatedVcdC;
        top->trace(tfp, 99);
        tfp->open("output.vcd");
    }

    while (!script->done()) {
        if (!top->clearn && main_time > 10) {
            top->clearn = 1;   // Deassert reset
            //std::cout << "Reset Done" << std::endl;
        }
        //if (top->dsp_clock)
        //    std::cout << "negedge @(dsp_clock) " << main_time << std::endl;
            
        top->dsp_clock = !top->dsp_clock;
        if ((main_time % 8) == 1) {
            top->dac_clock = 1;       // Toggle clock
            dac_clock_posedge = true;
        } else
            dac_clock_posedge = false;

        if ((main_time % 8) == 5) {
            top->dac_clock = 0;
            dac_clock_negedge = true;
        } else
            dac_clock_negedge = false;

        // We're not in reset, do something.
        if (top->clearn && !top->dsp_clock) {
            if (!started) {
                script->start();
                started = true;
            } else {
                int res = script->step();
                if (res < 0) return res;
            }
        } 
        top->eval();            // Evaluate model
        //std::cout << "foo" << top->dac_en << std::endl;       // Read a output

        if (top->dac_en) {
            if (dac_clock_posedge) {
                if (global_options.gnuplot)
                    std::cout << (int16_t)top->dac_data << " ";
                else
                    std::cout.write(reinterpret_cast<const char*>(&top->dac_data), sizeof(int16_t));
            }
            if (dac_clock_negedge) {
                if (global_options.gnuplot) {
                    std::cout << (int16_t)top->dac_data << std::endl;
                    if (++samples_count == 1024) {
                        samples_count = 0;
                        std::cout << "replot" << std::endl;
                    }
                }
                else {
                    std::cout.write(reinterpret_cast<const char*>(&top->dac_data), sizeof(int16_t));
                }
            }
        }

        main_time++;

        if (global_options.trace)
            tfp->dump(main_time);
    }

    if (global_options.trace)
        tfp->close();

    top->final();               // Done simulating

    if (global_options.gnuplot)
        std::cout << "exit";

    return 0;
}

