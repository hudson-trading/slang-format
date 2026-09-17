// Failure-only assertion actions start one level below the assertion, not the enclosing if.
module assertion_else_indent;
initial begin
if (enabled)
assert (sampled_data_pipe[configured_delay] == expected_data) else $error(
"Unexpected value"
);

assert (ready) else $error("Not ready");
assume (valid) else $warning("Invalid value");
assert #0 (ready) else $error("Deferred check failed");

if (enabled)
assert (ready) else begin
$error("Not ready");
$finish;
end
else
$display("Disabled");

assert (ready) // Check before proceeding.
else $error("Not ready");

assert (ready) else if (verbose) $error("Not ready");

// A terminating semicolon makes the else belong to the enclosing if.
if (enabled) assert (ready); else $display("Disabled");
assert (ready);
assert (ready) $display("Ready"); else $error("Not ready");
assert (ready) begin
$display("Ready");
end else begin
$error("Not ready");
end
end

check_ready: assert property (@(posedge clk) ready) else $error("Not ready");
endmodule
