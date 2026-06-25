import "lib/io"

let main() i32 =
  let msg: &[i8] = "Hello, World" in
  puts(msg);
  puts(msg[0..5]);
  puts(msg[7..]);
  let arr: [i64, 5] = [0, 1, 2, 3, 4] in
  print_slice(arr[..]);
  print_newline();
  print_slice(arr[..3]);
  print_newline();
  print_slice(arr[2..3]);
  print_newline();
  0
