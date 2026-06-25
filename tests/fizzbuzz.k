import "lib/io"

let fizzbuzz(n: i32) =
  let mod3: bool = (n % 3) = 0 in
  let mod5: bool = (n % 5) = 0 in
  if mod3 && mod5 then
    print_string("fizzbuzz\n")
  elif mod3 then
    print_string("fizz\n")
  elif mod5 then
    print_string("buzz\n")
  else
    print_int(n as i64);
	print_newline()
  end

let main() i32 =
  let mut n: i32 = 0 in
  while n < 100 do
    fizzbuzz(n);
	n += 1
  done;
  0
