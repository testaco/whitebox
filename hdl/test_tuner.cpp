#include <sys/types.h>
#include <sys/stat.h>
#include <iostream>
#include <fstream>
#include <queue>
#include <getopt.h>

#include "obj_dir/Vtuner.h"
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

static struct options {
    bool trace;
    bool gnuplot;
    float ifo_freq;
    uint32_t cic_n; // CIC resample rate, interp or decim

    options() : trace(false), gnuplot(false), ifo_freq(0.0), cic_n(1) {}
} global_options;

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

class ReadableFileFifo : public Fifo {
public:
    ReadableFileFifo(const char* filename) {
        f.open(filename, ios::binary | ios::in);
        if (!f)
            throw;
    }

    ~ReadableFileFifo() {
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

class WriteableFileFifo : public Fifo {
public:
    WriteableFileFifo(const char* filename) {
        f.open(filename, ios::binary | ios::out);
        if (!f)
            throw;
        cnt = 0;
    }

    ~WriteableFileFifo() {
        f.close();
    }

    bool full() {
        return f.bad();  // Meh
    }

    void write_next(uint32_t sample) {
        if (full()) throw;

        if (!global_options.gnuplot) {
            f.write((char*)(&sample), sizeof(sample));
        } else {
            int16_t i, q;
            i = sample & 0x0000ffff;
            q = (sample & 0xffff0000) >> 16;
            f << i << " " << q << "\n";
            if (++cnt == 64) {
                cnt = 0;
                f << "replot" << std::endl;
            }
        }
    }

private:
    std::ofstream f;
    int cnt;
};

int parse(int argc, char** argv) {
    while (true) {
        static struct option long_options[] = {
            {"trace", no_argument, NULL, 't'},
            {"gnuplot", no_argument, NULL, 'g'},
            {"ifo-freq", required_argument, NULL, 'i'},
            {"cic-n", required_argument, NULL, 'n'},
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
                global_options.trace = true;
                std::cerr << "option " << long_options[option_index].name
                          <<  " enabled" << std::endl;
                break;
            case 'g':
                global_options.gnuplot = true;
                std::cerr << "option " << long_options[option_index].name
                          <<  " enabled" << std::endl;
                break;
            case 'i':
                global_options.ifo_freq = atof(optarg);
                std::cerr << "option " << long_options[option_index].name
                          << " set to " << global_options.ifo_freq
                          << " Hz" << std::endl;
                break;
            case 'n':
                global_options.cic_n = atoi(optarg);
                std::cerr << "option " << long_options[option_index].name
                          << " set to " << global_options.cic_n
                          << std::endl;
                break;
            default:
                std::cerr << "unkown option " << c << std::endl;
                break;
        }
    }
}

struct BusCommand {
    BusCommand(uint32_t addr, bool write, int32_t wdata) :
        addr(addr), write(write), wdata(wdata) {}
    uint32_t addr;
    bool write;
    uint32_t wdata;
};

class BusController {
public:
    BusController(Vtuner * top) : top(top), in_command(false) {}
    ~BusController() {}

    void add_write(uint32_t addr, uint32_t wdata) {
        command_queue.push(BusCommand(addr, true, wdata));
    }

    void eval() {
        if (in_command) {
            if (top->bus_pready) {
                in_command = false;
                command_queue.pop();
                top->bus_psel = false;
                top->bus_penable = false;
                top->bus_paddr = 0;
                top->bus_pwrite = false;
                top->bus_pwdata = 0;
            }
        } else {
            if (!command_queue.empty()) {
                BusCommand & cmd = command_queue.front();
                in_command = true;
                top->bus_paddr = cmd.addr;
                top->bus_pwrite = cmd.write;
                top->bus_pwdata = cmd.wdata;
                top->bus_psel = true;
                top->bus_penable = true;
            }
        }
    }

private:
    Vtuner * top;
    std::queue<BusCommand> command_queue;
    bool in_command;
};

uint32_t freq_to_fcw(float freq) {
    float pa_cnt = 1 << 25;
    uint32_t fcw = (uint32_t)((freq / (10e6 / pa_cnt)) + .5);
    return fcw;
}

int main(int argc, char** argv) {
    // first parse the input arguments
    parse(argc, argv); 

    ReadableFileFifo * rf_in_fifo = new ReadableFileFifo("/dev/stdin");
    WriteableFileFifo * bb_in_fifo = new WriteableFileFifo("/dev/stdout");

    Verilated::commandArgs(argc, argv);   // Remember args
    Vtuner* top = new Vtuner;             // Create instance
    top->clearn = 0;           // Set some inputs
    top->bus_presetn = 0;

    BusController * bus_controller = new BusController(top);

    // if receive
    bus_controller->add_write(2, global_options.cic_n);
    bus_controller->add_write(1, freq_to_fcw(global_options.ifo_freq));
    bus_controller->add_write(0, (0 | (1 << 1)));
    // if transmit

    VerilatedVcdC* tfp = NULL;

    if (global_options.trace) {
        Verilated::traceEverOn(true);  // Turn on tracing
        tfp = new VerilatedVcdC;
        top->trace(tfp, 99);
        tfp->open("output.vcd");
    }

    bool done = false;
    bool pclk_posedge = false;
    bool dclk_posedge = false;
    bool sclk = false;
    bool sclk_posedge = false;
    bool sclk_negedge = false;

    while (!done) {
        // Done when the input FIFO is empty
        if (rf_in_fifo->empty())
            done = true;

        // Reset logic
        if (!top->clearn && main_time > 10) {
            top->clearn = 1;   // Deassert reset
            top->bus_presetn = 1;
            std::cerr << "Reset Done" << std::endl;
        } else if (main_time > 10) {
            // Update the clocks
            if (main_time % 6 == 0) { // ~83 MHz
                pclk_posedge = !top->bus_pclk;
                top->bus_pclk = !top->bus_pclk;
            } else {
                pclk_posedge = false;
            }

            if (main_time % 12 == 0) { // ~40 MHz
                dclk_posedge = !top->dclk;
                top->dclk = !top->dclk;
            } else {
                dclk_posedge = false;
            }
            if (main_time % 50 == 0) { // 10MHz
                sclk_posedge = !sclk;
                sclk_negedge = !sclk_posedge;
                sclk = !sclk;
            } else {
                sclk_posedge = false;
                sclk_negedge = false;
            }

            // Bus controller
            if (pclk_posedge) {
                bus_controller->eval();
            }

            // RF Input to Receiver Chain
            if (sclk_posedge && top->rf_in_ready && !rf_in_fifo->empty()) {
                uint32_t sample = rf_in_fifo->read_next();
                top->rf_in_valid = 1;
                top->rf_in_i = sample & 0x0000ffff;
                top->rf_in_q = (sample & 0xffff0000) >> 16;
            } else if (sclk_negedge) {
                top->rf_in_valid = 0;
                top->rf_in_i = 0;
                top->rf_in_q = 0;
            }

            // Run the HDL block
            top->eval();

            // Baseband Output, end of Receive chain
            if (dclk_posedge && top->bb_in_valid) {
                uint32_t sample =
                    (top->bb_in_i & 0x0000ffff) |
                    ((top->bb_in_q & 0x0000ffff) << 16);
                bb_in_fifo->write_next(sample);
            }

            // RF Output to Transmitter
            if (top->rf_out_valid) {
                std::cerr << "rf_out" << std::endl;
            }
        }

        // Advance the time and update the trace
        main_time++;
        if (global_options.trace)
            tfp->dump(main_time);
    }

    if (global_options.trace)
        tfp->close();

    top->final();               // Done simulating

    if (global_options.trace)
        delete tfp;

    delete bus_controller;
    delete top;

    delete bb_in_fifo;
    delete rf_in_fifo;

    return 0;
}
