outside_func = { |x| function {
  outside_return = return
  inside_func = { |y| function {
    outside_return y
  }}
  inside_func 3
  2
}}

print outside_func(1)

print.

outside_func = { |x| function {
  outside_return = freeze(return)
  inside_func = { |y| function {
    outside_return y
  }}
  inside_func 3
  2
}}

print outside_func(1)