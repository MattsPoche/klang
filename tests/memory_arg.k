let index(x: [i32, 8], y: u64) i32 = x[y]

let main() i32 =
  let x: [i32, 8] = {4, 5, 6, 7, 8, 9, 10, 11} in
  let a: i32 = index(x, 2) in
  let b: i32 = index(x, 5) in
  a + b
