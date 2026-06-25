import "lib/io"

let main() i32 =
  print_int((69 + 420) as i64);   print_newline();
  print_int((69 - 420) as i64);   print_newline();
  print_int((69 * 420) as i64);   print_newline();
  print_int((69 / 420) as i64);   print_newline();
  print_int((69 ^ 420) as i64);   print_newline();
  print_int((69 & 420) as i64);   print_newline();
  print_bool(69 = 420);           print_newline();
  print_bool(69 < 420);           print_newline();
  print_bool(69 > 420);           print_newline();
  print_bool(69 <= 420);          print_newline();
  print_bool(69 >= 420);          print_newline();
  0
