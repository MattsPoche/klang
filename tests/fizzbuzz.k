/*
 * FILE *stdout;
 */
let stdout: void* = #extern(stdout)

/*
 * size_t fwrite(const void *ptr, size_t size, size_t n, FILE *stream);
 */
let fwrite: (void*, u64, u64, void*) -> u64 = #extern(fwrite)

let print_char(c: i8) =
  fwrite(&c, 1, 1, stdout) as void

let print_string(str: &[i8]) =
  fwrite(#ptr(str), 1, #len(str), stdout) as void

let print_digits(n: i64) =
  if n > 0 then
    let c: i64 = (n % 10) + 48 in
	print_digits(n / 10);
	print_char(c as i8)
  end

let print_int(n: i64) =
  if n = 0 then
    print_char(#"0")
  else
    let n: i64 = if n < 0 then print_char(#"-"); -n else n end in
    print_digits(n)
  end

let print_newline() = print_char(#"\n")

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
