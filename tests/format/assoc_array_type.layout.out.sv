// Test: associative array with type key should preserve space in type
// e.g., [int unsigned] should NOT become [intunsigned]

class test_cls;
    task run();
        int unsigned expected_data[int unsigned];
        string names[string];
        logic [7:0] mem[int];
    endtask
endclass
