// A directive after a task containing recovered macro text must remain a directive.
class producer;
    `define HANDLE state

    task run();
        if (enabled) `HANDLE.value = '0;
    endtask

    `undef HANDLE
endclass

class consumer;
    int value;
endclass
