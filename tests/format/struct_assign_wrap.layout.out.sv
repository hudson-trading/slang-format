package test_pkg;

    // Short LHS — equal-aligned is fine
    localparam config_t SHORT_CFG = '{
        alpha: 1,
        beta: 2,
        gamma: 3
    };

    // Long LHS — should split after = instead of equal-aligning
    localparam LongPkgName_pkg::long_struct_config_t LONG_STRUCT_CONFIG = '{
        first_field_type: ModePkg::MODE_NONE,
        second_field_type: ModePkg::MODE_NONE,
        third_field_type: ModePkg::MODE_NONE,
        fourth_field_type: ModePkg::MODE_VALID,
        fifth_field_type: ModePkg::MODE_NONE,
        sixth_field_type: ModePkg::MODE_NONE,
        seventh_field_type: ModePkg::MODE_VALID,
        eighth_field_type: ModePkg::MODE_NONE,
        ninth_field_type: ModePkg::MODE_VALID
    };

    // Another long assignment with struct
    localparam AnotherLongPkgName_pkg::another_long_type_name_t ANOTHER_CONFIG = '{
        field_a: ValuePkg::SOME_VALUE,
        field_b: ValuePkg::ANOTHER_VALUE
    };

endpackage
