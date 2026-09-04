// A preserved blank line after a nested list must not grow on subsequent passes.
class example;
    constraint values_c {
        mode dist {
            MODE_A :/ 1,
            MODE_B :/ 2
        };

        count >= 1;
        count <= 8;
    }
endclass
