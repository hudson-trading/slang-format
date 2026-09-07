// A recovered macro following endfunction is a separate class member, so it
// must not change whether declarations in the next method are aligned.
class example;
    function void first();
    endfunction `REGISTER_CONSTRUCTOR

    virtual function void second();
        short_t first_value;
        much_longer_t second_value;
    endfunction
endclass
