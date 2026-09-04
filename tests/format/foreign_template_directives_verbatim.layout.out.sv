// Foreign template directives make the unexpanded document opaque.
parameter int WIDTH=8;
% if feature_enabled:
logic enabled;
% else:
logic disabled;
% endif
