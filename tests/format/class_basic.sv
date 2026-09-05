class Packet;
    rand bit [7:0] header;
    rand bit [7:0] payload[];
    bit [15:0] crc;

    constraint size_c {
        payload.size inside {[1:64]};
    }

    constraint header_c {
        header[7:4] == 4'hA;
    }

    function void post_randomize();
        crc = calc_crc();
    endfunction

    function bit [15:0] calc_crc();
        bit [15:0] result = 16'hFFFF;
        result ^= header;
        foreach (payload[i])
            result ^= payload[i];
        return result;
    endfunction
endclass

class ExtPacket extends Packet;
    rand bit [3:0] priority_level;

    constraint priority_c {
        priority_level < 10;
        soft priority_level == 5;
    }
endclass
