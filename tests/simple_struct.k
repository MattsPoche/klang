import "lib/io"

let main() i32 =
  let s: {i32, &[i8]} = {69, "Hello, World\n"} in  
  print_string(s.1);
  s.0