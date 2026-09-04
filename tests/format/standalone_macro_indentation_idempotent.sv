// Source indentation inside a standalone macro must not compound with structural indentation.
`ifndef EXAMPLE_SV
`define EXAMPLE_SV

class example;
   item_t items[ITEM_COUNT];
   config_t cfg; // Configuration handle

      `REGISTER_TYPE(example)

   function new(string name = "");
      super.new(name);
   endfunction
endclass

`endif
