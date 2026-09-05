module escaped_ids (
    input logic clk,
    output logic \data.out ,
    inout wire \bus.a[0]
);
    wire \net.x ;
    wire \net.y ;

    assign \net.x = clk;
    assign \data.out = \net.x & \net.y ;

    sub_mod u0 (
        .\port.a (\net.x ),
        .\port.b (\net.y )
    );
endmodule
