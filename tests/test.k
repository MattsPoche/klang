/*
 * let array: [i64, 6] = {1, 2, 3, 4, 5, 6}
 * let lambda foo(x: f32, y: f32) f32 = ... in
 * let foo: proc(x: i32, y: i32) i32 = ... in
 * let some_function(f: lambda(x: f32, y: f32) f32) = ... in
 * some_function(lambda(x: f32, y: f32) f32 -> ...)
 * struct{x = 32, y = 69} // anonymous struct initializer
 * (:my_type){x = 32, y = 69} // named struct initializer
 */

// let main(args: &[[i8]*]) i32 =
//   let x: i32 = 69 in
//   let y: i32 = 0xdeadbeaf + if foo < x then something; 69 else 420 end in
//   let arg0: {u64, i8*} =
//     { /* |strview| */
// 	 cstrlen(argv[0]),
// 	 argv[:str, 0]
// 	}
//   in
//   pickles + -f(x, y).foo
// let foo: i32 = 420

/* heap allocation example */
// let foo() {i64, i64}* =
//   let alloc pair: {i64, i64}* =
//     // pair = malloc(sizeof({i64, i64}));
//     {some_computation1(), some_computation2()}
//   in
//   ...

// let foo(x: i32) i32 = x * 2

// let bar(x: i32, y: i32) i32 = x / y

// let main() i32 =
//   let x: i32 = 34 in
//   let y: i32 = 35 in
//   let z: i32 = 22 in
//   let a: i32 = x + (y - z) * 2 in
//   a

// let main() i32 =
//   let x: i32 = 34 in
//   let y: i32 = 35 in
//   let z: i32 = 22 in
//   x + (y - z) * 2 

// let main() i32 =
//   let x: i32 = 34 in
//   let y: i32 = 35 in
//   x + y

// let main() i32 = 69

// let foo(x: i32, y: i32) i32 =
//   if x = 1 || y = 1
//   then 69
//   else 42
//   end

// let main() i32 = foo(0, 1)

// let foo() i32* =
//   let mut x: i32 = 69 in
//   &x

// let main() i32 =
//   let mut foo: i32 = 32 in
//   foo / 2;
//   *(&foo) := 69;
//   foo

// let foo() i32 =
//   case 69
//   of 1, 2 do
//     1 + 2
//   of 69 do
//     420
//   else
//     0
//   end

// let main() i32 =
//   if true then
//     69
//   elif false then
//     42
//   else
//     69 - 42
//   end

// let min(x: i32, y: i32) i32 =
//   if x < y then x else y end

// let main() i32 =
//   let x: i32 = 69 in
//   let y: i32 = 42 in
//   20 - min(x, y)

// let min(x: i32, y: i32) i32 =
//   if x < y then x else y end

// let main() i32 =
//   let x: i32 = 69 in
//   let y: i32 = 42 in
//   20 - min(x, y)

let main() i32 =
  let x: i32 = 69 in
  let y: i32 = 2  in
  x << 1