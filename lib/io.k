newtype ('a, 'e) Result =
  | Ok:    'a
  | Error: 'e

newtype FILE = void*

type str = &[i8]

/* let nullptr: void* = 0 as void* */

let libc_errno_location: () -> i32* = #extern(__errno_location)
let libc_strerror: (i32) -> [i8]* = #extern(strerror)

let stdout: FILE = #extern(stdout)
let stdin:  FILE = #extern(stdin)
let stderr: FILE = #extern(stderr)

let libc_fopen: ([i8]*, [i8]*) -> FILE = #extern(fopen)
let libc_fwrite: (void*, u64, u64, FILE) -> u64 = #extern(fwrite)
let fclose: (FILE) -> i32 = #extern(fclose)

let get_errno() i32 = *libc_errno_location()
  
let get_error_str(errno: i32) str = str_of_cstr(libc_strerror(errno))

let open_read_file(path: [i8]*) (FILE, {errno: i32, msg: str}) Result =
  let f: FILE = libc_fopen(path, #ptr("r\0")) in
  if f = 0 as FILE then
    let e: i32 = get_errno() in
    Error{errno = e, msg = get_error_str(e)}
  else
    Ok(f)  

let print_char(c: i8) =
  libc_fwrite(&c, 1, 1, stdout)

let print_string(str: str) =
  libc_fwrite(#ptr(str), 1, #len(str), stdout)

let print_digits(n: 'a) =
  if n > 0 then
    let c: 'a = (n % 10) + 48 in
	print_digits(n / 10);
	print_char(c as i8)

let print_int(n: 'a) =
  if n = 0 then
    print_char(#"0")
  else
    let n: 'a = if n < 0 then print_char(#"-"); -n else n in
    print_digits(n)

let print_bool(x: bool) =
  print_string(if x then "true" else "false")

let print_newline() = print_char(#"\n")

let puts(str: str) =
  print_string(str);
  print_newline()

let print_slice(slice: &['a]) =
  let mut i: u64 = 0 in
  while i < #len(slice) do
    print_int(slice[i]);
    print_char(#" ");
    i += 1

let str_of_cstr(cstr: [i8]*) str =
  let mut length: u64 = 0 in
  (while cstr[length] != 0 do
     length += 1);
  cstr[..length]
