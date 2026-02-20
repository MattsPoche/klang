/*
 * FILE *stdout;
 */
let stdout: void* = #extern(stdout)

/*
 * size_t fwrite(const void *ptr, size_t size, size_t n, FILE *stream);
 */
let fwrite: (void*, u64, u64, void*) -> u64 = #extern(fwrite)

let print_char(c: i8) =
  fwrite(&c, 1, 1, stdout)

let print_string(str: &[i8]) =
  fwrite(#ptr(str), 1, #len(str), stdout)

let print_digits(n: i64) =
  if n > 0 then
    let c: i64 = (n % 10) + 48 in
	print_digits(n / 10);
	print_char(c as i8)
  end

let print_int(n: i64) =
  if n = 0 then
    print_char("0"[0])
  else
    let n: i64 = if n < 0 then print_char("-"[0]); -n else n end in
    print_digits(n)
  end

let print_newline() = print_char("\n"[0])

let puts(str: &[i8]) =
  print_string(str);
  print_newline()

let print_slice(slice: &[i64]) =
  let mut i: u64 = 0 in
  while i < #len(slice) do
    print_int(slice[i]);
    print_char(" "[0]);
    i += 1
  done;
  print_newline()

let main() i32 =
  let msg: &[i8] = "Hello, World" in
  puts(msg);
  puts(msg[0..5]);
  puts(msg[7..]);
  let arr: [i64, 5] = {0, 1, 2, 3, 4} in
  print_slice(arr[..]);
  print_slice(arr[..3]);
  print_slice(arr[2..3]);
  0
