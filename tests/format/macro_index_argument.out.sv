// An indexed macro stays inside its argument when the list wraps.
if (`ITEMS[2].valid[index]) process_item(
    `ITEMS[2].memory.data[index],
    index_t'(index),
    way_t'(2)
);

// A newline before an index does not separate it from the macro expression.
if (`ITEMS[2].valid[index]) process_item(
    `ITEMS[2].memory.data[index],
    index_t'(index),
    way_t'(2)
);
