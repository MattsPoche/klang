newtype FILE = void*

type str = &[i8]

newtype 'a Point = {
  x: 'a,
  y: 'a,
}

newtype ('a, 'b) Pair = {
  x: 'a,
  y: 'b,
}

/*
 * FILE *stdout;
 */

let stdout: FILE = #extern(stdout)

let fwrite: (void*, u64, u64, FILE) -> u64 = #extern(fwrite)

let print_char(c: i8) =
  fwrite(&c, 1, 1, stdout)

let print_string(str: str) =
  fwrite(#ptr(str), 1, #len(str), stdout)

let print_digits(n: 'a) =
  if n > 0 then
    let c: 'a = (n % 10) + 48 in
	print_digits(n / 10);
	print_char(c as i8)
  end

let print_int(n: 'a) =
  if n = 0 then
    print_char("0"[0])
  else
    let n: 'a = if n < 0 then print_char("-"[0]); -n else n end in
    print_digits(n)
  end

let print_bool(x: bool) =
  print_string(if x then "true" else "false" end)

let print_newline() = print_char("\n"[0])

let puts(str: str) =
  print_string(str);
  print_newline()

let print_slice(slice: &['a]) =
  let mut i: u64 = 0 in
  while i < #len(slice) do
    print_int(slice[i]);
    print_char(" "[0]);
    i += 1
  done

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