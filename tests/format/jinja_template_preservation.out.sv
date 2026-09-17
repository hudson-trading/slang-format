// Jinja control flow and substituted names must survive formatting unchanged.
{% macro declare_type(item) -%}
class {{type_name(item)}} extends base;
  {% for field in fields(item) -%}
    {{field_type(field)}}__{{field_name(field)}} value;
  {% endfor -%}
endclass
{% endmacro %}
