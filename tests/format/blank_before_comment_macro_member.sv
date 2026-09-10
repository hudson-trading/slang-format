// Keep a section-separating blank before a comment when the next member starts with a macro.
module foo;
initial begin
if (enabled) begin
foreach (items[i]) begin
consume(items[i]);
end

// Start the next section.
`CHECK(ready)
finish();
end
end
endmodule

// Preserve section boundaries on either side of macros in a task body.
task run(int index);
`WAIT(ready,,, "timeout")

// Clean up after the wait.
cleanup();

// Prepare the next operation.
`CREATE(item_type, item)
if (index > 0) begin
consume(item);
end
endtask : run
