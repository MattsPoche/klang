newtype 'a Option =
  | None
  | Some: 'a

/*
newtype ('a, 'e) Result =
  | Ok:  'a
  | Err: 'e
*/

newtype FILE = void*

type str = &[i8]

/* let nullptr: void* = 0 as void* */
let stdout: FILE = #extern(stdout)
let stdin:  FILE = #extern(stdin)
let stderr: FILE = #extern(stderr)

let libc_fopen: ([i8]*, [i8]*) -> FILE = #extern(fopen)
let libc_fwrite: (void*, u64, u64, FILE) -> u64 = #extern(fwrite)
let fclose: (FILE) -> i32 = #extern(fclose)

let foo(flag: bool) [i64, 4] =
  if flag then
    [1, 2, 3, 4]
  else
    [5, 6, 7, 8]
  end

let fopen(path: [i8]*, mode: [i8]*) FILE Option =
  let f: FILE = libc_fopen(path, mode) in
  if f = 0 as FILE then None else Some(f) end

let print_char(c: i8) =
  libc_fwrite(&c, 1, 1, stdout)

let print_string(str: str) =
  libc_fwrite(#ptr(str), 1, #len(str), stdout)

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

let str_of_cstr(cstr: [i8]*) str =
  let mut length: u64 = 0 in
  while cstr[length] != 0 do
    length += 1
  done;
  cstr[..length]

let main(argc: i32, argv: [[i8]*]*) i32 =
  print_string("argc = ");
  print_int(argc);
  print_newline();
  let filename: str = str_of_cstr(argv[0]) in
  print_string("file = ");
  print_string(filename);
  print_newline();
  case fopen(#ptr(filename), #ptr("r\0")) of
    Some(file) ->
	  print_string("Opened file: ");
	  print_string(filename);
	  print_newline();
	  fclose(file),
    None ->
	  print_string("Unable to open file: ");
	  print_string(filename);
	  print_newline();
	  0
  end;
  0
