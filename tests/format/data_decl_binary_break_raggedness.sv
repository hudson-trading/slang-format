// Assignment and binary breaks in an initialized declaration compete at the
// same priority; equal line counts keep the RHS beside the assignment.
module data_decl_binary_break_raggedness;
    wire process_event_syncs_raw = held_metadata.flags.start.process_event_syncs_raw && held_metadata.valid;

    // Contrast: a short binary initializer remains on one line.
    wire event_ready = input_valid && output_ready;
endmodule
