import "lib/io"

newtype 'a Point = {
  x: 'a,
  y: 'a,
}

newtype ('a, 'b) Pair = {
  x: 'a,
  y: 'b,
}

let main() i32 =
  let p: i32 Point = {69, 420} in
  let q: i64 Point = {1303948, 38438273} in
  let xs: &[i32]   = let t: [i32] = [32, 33, 34, 35, 36, 37, 38] in t[..] in
  let pair1: (i32, str) Pair = {69, "Hello, World"} in
  let pair2: (u32, u16) Pair = {690, 6969} in
  print_int(p.x);        print_newline();
  print_int(p.y);        print_newline();
  print_int(q.x);        print_newline();
  print_int(q.y);        print_newline();
  print_slice(xs);       print_newline();
  print_int(pair1.x);    print_newline();
  print_string(pair1.y); print_newline();
  print_int(pair2.x);    print_newline();
  print_int(pair2.y);    print_newline();
  0