// Prefer breaking before a comparison operator before splitting either call's arguments.
module demo;
    always_comb begin
        counts_match = normalize_count(current_status[index].available_count) == normalize_count(request_metadata.start.saved_status[index].available_count);
        counts_differ = normalize_count(current_status[index].available_count) != normalize_count(request_metadata.start.saved_status[index].available_count);
        short_match = normalize_count(a) == normalize_count(b);
        combined_match = source_enabled && normalize_count(current_value) == normalize_count(saved_value);
    end
endmodule
