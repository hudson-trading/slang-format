module loops_control;
    logic [7:0] mem[256];
    logic [7:0] values[$];
    int         result;

    initial begin
        // For loop
        for (int i = 0; i < 256; i++) begin
            mem[i] = i[7:0];
        end

        // Foreach
        foreach (mem[idx]) begin
            if (mem[idx] == 8'hFF)
                $display("Found FF at %0d", idx);
        end

        // While
        result = 0;
        while (result < 100) begin
            result = result + 7;
        end

        // Do-while
        result = 1;
        do begin
            result = result * 2;
        end while (result < 1024);

        // Repeat
        repeat (10) begin
            values.push_back(8'hAA);
        end

        // Forever with break
        forever begin
            if (values.size() > 20)
                break;
            values.push_back(8'hBB);
        end

        // Nested loops with continue
        for (int i = 0; i < 16; i++) begin
            for (int j = 0; j < 16; j++) begin
                if (j == i)
                    continue;
                mem[i*16+j] = 8'h00;
            end
        end
    end
endmodule
