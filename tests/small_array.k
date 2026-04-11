let foo(pair: [i32, 2]) i32 = pair[0]
let bar(pair: [i32]*) i32 = pair[1]
let baz(data: [i32, 8], idx: i32) i32 = data[idx]

let main() i32 =
  let pair: [i32, 2] = {42, 69} in
  foo(pair) + bar(&pair) + baz({3, 4, 5, 6, 7, 8, 9, 10}, 0)
