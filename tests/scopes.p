x = (3)
print x
{ |x| print x; x := 4; print x }(2)
print x

print.

x = 1
y = { print x }
y.
print x
x := 2
y.
print x

print.

x = 1
y = { print x }
y.
print x
x = 2
y.
print x

print.

x = 1
y = ( z = x; { print z } )
y.
print x
x = 2
y.
print x

print.

z = {
  (
    cont 3
  )
  cont 4
}

print z()

z = {
  cont 4
  (
    cont 3
  )
}

print z()

print.

x = 10
(
  x = 11
)
print x

x = 10
(
  x := 11
)
print x

print.

var = 100
get = { print var }
set = { |val| var := val }
var = 101
print var
get.
set 102
print var
get.

print.

var2 = 200
get = { print var2 }
set = { |val| var2 := val }
var2 := 201
print var2
get.
set 202
print var2
get.

print.

recursion = {|x|
  if (< x 10) {
    recursion (+ x 1)
  }
  print x
}
recursion 1

print.

recursion = {|x, y|
  x := y
  print x
  if (< y 10) {
    recursion 1 (+ y 1)
  }
  print x
}
recursion 1 1

print.

x = 3
test = {
  print x
  x = 4
  print x
}
test.
test.

print.