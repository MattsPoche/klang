import "lib/io"

let main(argc: i32, argv: [[i8]*]*) i32 =
  print_string("argc = ");
  print_int(argc);
  print_newline();
  let filename: str = str_of_cstr(argv[0]) in
  print_string("file = ");
  print_string(filename);
  print_newline();
  case open_read_file(#ptr(filename)) of
    Ok(file) ->
	  print_string("Opened file: ");
	  print_string(filename);
	  print_newline();
	  fclose(file),
    Error(&e) ->	
	  print_string("Error code: ");
	  print_int(e.errno);
	  print_newline();
	  print_string(e.msg);
	  print_newline();
	  0,
  end;
  case open_read_file(#ptr("foo_bar\0")) of
    Ok(file) ->
	  print_string("Opened file: ");
	  print_string(filename);
	  print_newline();
	  fclose(file);
	  0,
    Error(&e) ->
	  print_string("Error code: ");
	  print_int(e.errno);
	  print_newline();
	  print_string(e.msg);
	  print_newline();
	  0,
  end
