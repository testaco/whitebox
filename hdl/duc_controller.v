export "DPI-C" task set_interp;

task set_interp;
    input int i;
    begin
        system_interp = i;
    end
endtask
