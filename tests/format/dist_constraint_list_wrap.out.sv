// Distribution lists wrap when their items exceed the column limit.
class example;
    rand mode_t mode;

    constraint mode_c {
        mode dist {Enabled :/ cfg.enabled_weight, Disabled :/ (100 - cfg.enabled_weight)};
        mode dist {
            Enabled :/ cfg.selection_enabled_percentage,
            Disabled :/ (100 - cfg.selection_enabled_percentage)
        };
    }
endclass
