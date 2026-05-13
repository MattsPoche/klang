newtype FILE = void*

/*
 * FILE *stdout;
 */

let stdout: FILE = #extern(stdout)

let fwrite: (void*, u64, u64, FILE) -> u64 = #extern(fwrite)

let print_char(c: i8) =
  fwrite(&c, 1, 1, stdout)

let print_string(str: &[i8]) =
  fwrite(#ptr(str), 1, #len(str), stdout)

let print_digits(n: 'a) =
  if n > 0 then
    let c: 'a = (n % 10) + 48 in
	print_digits(n / 10);
	print_char(c as i8)
  end

let print_int(n: 'a) =
  if n = 0 then
    print_char(#"0")
  else
    let n: 'a = if n < 0 then print_char(#"-"); -n else n end in
    print_digits(n)
  end

let print_bool(x: bool) =
  print_string(if x then "true" else "false" end)

let print_newline() = print_char(#"\n")

let puts(str: &[i8]) =
  print_string(str);
  print_newline()

let print_slice(slice: &['a]) =
  let mut i: u64 = 0 in
  while i < #len(slice) do
    print_int(slice[i]);
    print_char(#" ");
    i += 1
  done;
  print_newline()

let main() i32 =
  print_int(69 as i8);  print_newline();
  print_int(420 as i32); print_newline();
  print_int(-348 as i64); print_newline();
  0