package sample_pkg;
class GenericComponent #(
parameter int WIDTH = 32
) extends BaseComponent;
`REGISTER_COMPONENT(GenericComponent #(WIDTH))
GenericBus_pkg::Requester #(WIDTH) requester;
logic [63:0] memory[int];
int burst_address;
string endpoint_path;
endclass
endpackage
