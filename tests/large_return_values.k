let array() [i32, 8] = {4, 5, 6, 7, 8, 9, 10, 11}
let quad() [i32, 4] = {4, 5, 6, 7}

let main() i32 =
  array()[2] + quad()[1]