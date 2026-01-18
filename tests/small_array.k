let fst(pair: [i32, 2]) i32 = pair[0]
let snd(pair: [i32]*) i32 = pair[1]

let main() i32 =
  let pair: [i32, 2] = {42, 69} in
  let x: i32 = fst(pair) in
  let y: i32 = snd(&pair) in
  x + y