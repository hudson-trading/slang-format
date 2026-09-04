// A standalone comment before a leading comma remains a standalone list boundary.
module example;
    parameter int values[] = {
        FIRST_VALUE

        // Second group.
        ,SECOND_VALUE
    };
endmodule
