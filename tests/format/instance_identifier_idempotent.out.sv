module instance_identifier_idempotent;
    `SOURCE_TIE_OFF(source_message)
    `SINK_TIE_OFF(sink_message[0])

    NullPeripheral
      nullPeripheralA (.bus(bus[0]));
    NullPeripheral
      nullPeripheralB (.bus(bus[1]));

    Seeder
      seederA (.clk);
    Seeder
      seederB (.clk);
endmodule
