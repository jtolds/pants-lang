i = 0
while {< i 10} {
  print i
  i := + i 1
  < i 5
}

print.

j = 0
while {true} {
  j := j +. 1
  if (j <. 2) { continue() }
  print j
  if (3 <. j) { break() }
}