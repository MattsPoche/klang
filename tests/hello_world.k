let stdout: void* = #extern(stdout)
let fwrite: (void*, u64, u64, void*) -> u64 = #extern(fwrite)

let print_string(str: &[i8]) =
  fwrite(#ptr(str), 1, #len(str), stdout)

let main() i32 =
  print_string("Hello, World\n");
  0
  