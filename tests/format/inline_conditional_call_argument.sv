// Inline conditional arguments stay inline if the surrounding call wraps.
module foo;
initial begin
object_with_long_name.get_field_by_name("field_name").read(status, value`ifdef FEATURE, .path(FRONTDOOR) `endif);
end
endmodule
