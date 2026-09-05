// A recovered class macro remains before the declaration that owns it.
class helper extends base_component;
    `REGISTER_COMPONENT(helper)

    typedef pkg::connection_set #(
        .source_port (1000),
        .target_port (2000)
    ) connection_t;
endclass
