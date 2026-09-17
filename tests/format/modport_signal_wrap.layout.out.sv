// Long modport directions wrap their signal lists without repeating the direction.
interface demo;
    modport endpoint (
        input request_data, response_data, limit_data, source_data, query_index, status_code,
              group_index, item_index, enable_flag, alert_flag, format_index, selected_side,
        output result_data, result_group, client_index
    );
    modport producer (
        output request_data, response_data, limit_data, source_data, query_index, status_code,
               group_index, item_index, enable_flag, alert_flag, format_index, selected_side,
        input .result(result_data), client_index
    );
endinterface
