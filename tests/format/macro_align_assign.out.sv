module test;
    task tt();
        col_a[0] <= (x_p[1] > y_p[1]) ^ get_s(z_p[1]);
        col_b[0] <= `ABS_DIFF(
                      {x_p[1], 1'b0},
                      {y_p[1], 1'b0});
        col_c[0] <= {y_p[1], 1'b0};
        col_a[1] <= (u_p[1] > v_p[1]) ^ get_s(z_p[1]);
        col_b[1] <= `ABS_DIFF(
                      {x_p[1], 1'b0},
                      {u_p[1], 1'b0});
        col_c[1] <= {u_p[1], 1'b0};
    endtask
endmodule
