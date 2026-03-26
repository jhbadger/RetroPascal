program sudoku;

const
  KEY_UP    = #1;
  KEY_DOWN  = #2;
  KEY_LEFT  = #3;
  KEY_RIGHT = #4;

var
  j, l, o, p, q, limk1, limk2, limi1, limi2: integer;
  ch: char;
  x, y, no1, no2, i, s, k, nfixed: integer;
  b, t, v, quit, play_again, board_ok: boolean;
  key, yn: char;
  M:   array [1..81] of char;
  xy1: array [1..81] of integer;
  xy2: array [1..81] of integer;

{ M stored row-major: M[(col-1)*9 + row], row and col are 1..9 }
function mcell(row, col: integer): char;
begin
  mcell := M[(col - 1) * 9 + row];
end;

procedure setcell(row, col: integer; c: char);
begin
  M[(col - 1) * 9 + row] := c;
end;

procedure box_limits(val: integer; var lo, hi: integer);
begin
  if val <= 3 then begin lo := 1; hi := 3; end
  else if val <= 6 then begin lo := 4; hi := 6; end
  else begin lo := 7; hi := 9; end;
end;

function is_fixed(cx, cy: integer): boolean;
var k2: integer;
begin
  is_fixed := false;
  for k2 := 1 to nfixed do
    if (xy1[k2] = cx) and (xy2[k2] = cy) then
      is_fixed := true;
end;

procedure clear_msg;
begin
  gotoxy(2, 22);
  write('                                                   ');
end;

function cur_col: integer;
begin cur_col := ((x - 4) div 6) + 1; end;

function cur_row: integer;
begin cur_row := ((y - 2) div 2) + 1; end;

procedure generate_board;
begin
  board_ok := false;
  while not board_ok do begin
    for i := 1 to 81 do M[i] := ' ';
    board_ok := true;
    k := 1;
    while (k <= 9) and board_ok do begin
      box_limits(k, limk1, limk2);
      no2 := 0;
      i := 1;
      while (i <= 9) and board_ok do begin
        box_limits(i, limi1, limi2);
        no1 := 0;
        b := false;
        while not b do begin
          no1 := no1 + 1;
          ch := chr(ord('1') + random(9));
          b := true;
          for j := 1 to 9 do
            if ch = mcell(j, k) then b := false;
          for l := 1 to 9 do
            if ch = mcell(i, l) then b := false;
          for p := limk1 to limk2 do
            for q := limi1 to limi2 do
              if ch = mcell(q, p) then b := false;
          if b then
            setcell(i, k, ch)
          else if no1 >= 100 then begin
            for o := 1 to 9 do setcell(o, k, ' ');
            no2 := no2 + 1;
            if no2 >= 100 then
              board_ok := false
            else
              i := 0;
            b := true;
          end;
        end;
        i := i + 1;
      end;
      k := k + 1;
    end;
  end;
end;

procedure draw_board;
begin
  clrscr;
  writeln(' _____________________________________________________ ');
  writeln('|     |     |     |     |     |     |     |     |     |');
  writeln('|_____|_____|_____|_____|_____|_____|_____|_____|_____|');
  writeln('|     |     |     |     |     |     |     |     |     |');
  writeln('|_____|_____|_____|_____|_____|_____|_____|_____|_____|');
  writeln('|     |     |     |     |     |     |     |     |     |');
  writeln('|_____|_____|_____|_____|_____|_____|_____|_____|_____|');
  writeln('|     |     |     |     |     |     |     |     |     |');
  writeln('|_____|_____|_____|_____|_____|_____|_____|_____|_____|');
  writeln('|     |     |     |     |     |     |     |     |     |');
  writeln('|_____|_____|_____|_____|_____|_____|_____|_____|_____|');
  writeln('|     |     |     |     |     |     |     |     |     |');
  writeln('|_____|_____|_____|_____|_____|_____|_____|_____|_____|');
  writeln('|     |     |     |     |     |     |     |     |     |');
  writeln('|_____|_____|_____|_____|_____|_____|_____|_____|_____|');
  writeln('|     |     |     |     |     |     |     |     |     |');
  writeln('|_____|_____|_____|_____|_____|_____|_____|_____|_____|');
  writeln('|     |     |     |     |     |     |     |     |     |');
  writeln('|_____|_____|_____|_____|_____|_____|_____|_____|_____|');
  gotoxy(60, 3);  write('+Instructions+');
  gotoxy(57, 5);  write('*Keys to move:');
  gotoxy(58, 6);  write('Arrow keys or Z/S/Q/D.');
  gotoxy(57, 9);  write('*Enter numbers 1-9.');
  gotoxy(58, 10); write('"Space" to erase.');
  gotoxy(57, 13); write('-----------------------');
  gotoxy(57, 15); write('*Press "P" to exit.');
  gotoxy(57, 17); write('-----------------------');
  gotoxy(63, 19); write('Enjoy! ^-^');
end;

procedure place_numbers;
var cx, cy: integer;
begin
  cx := 4;
  for k := 1 to 9 do begin
    cy := 2;
    for i := 1 to 9 do begin
      gotoxy(cx, cy);
      write(mcell(i, k));
      cy := cy + 2;
    end;
    cx := cx + 6;
  end;
end;

procedure record_fixed;
begin
  nfixed := 0;
  for k := 1 to 9 do
    for i := 1 to 9 do
      if mcell(i, k) <> ' ' then begin
        nfixed := nfixed + 1;
        xy1[nfixed] := (k - 1) * 6 + 4;
        xy2[nfixed] := (i - 1) * 2 + 2;
      end;
end;

procedure run_game;
begin
  randomize;
  generate_board;
  for i := 1 to 38 do begin
    limi1 := random(9) + 1;
    limi2 := random(9) + 1;
    setcell(limi1, limi2, ' ');
  end;
  draw_board;
  place_numbers;
  record_fixed;

  x := 28; y := 10;
  gotoxy(x, y);
  quit := false;
  v := false;

  while not quit do begin
    key := readkey;

    { movement - arrow keys and ZSDQ }
    if (key = KEY_UP) or (key = 'z') then begin
      clear_msg; y := y - 2;
    end else if (key = KEY_DOWN) or (key = 's') then begin
      clear_msg; y := y + 2;
    end else if (key = KEY_LEFT) or (key = 'q') then begin
      clear_msg; x := x - 6;
    end else if (key = KEY_RIGHT) or (key = 'd') then begin
      clear_msg; x := x + 6;
    end else if key = 'p' then begin
      quit := true;
    end else if key = ' ' then begin
      clear_msg;
      if not is_fixed(x, y) then begin
        setcell(cur_row, cur_col, ' ');
        gotoxy(x, y); write(' ');
      end;
    end else if (key >= '1') and (key <= '9') then begin
      clear_msg;
      if not is_fixed(x, y) then begin
        box_limits(cur_col, limk1, limk2);
        box_limits(cur_row, limi1, limi2);
        t := true;
        for j := 1 to 9 do
          if key = mcell(j, cur_col) then t := false;
        for l := 1 to 9 do
          if key = mcell(cur_row, l) then t := false;
        for s := limk1 to limk2 do
          for o := limi1 to limi2 do
            if key = mcell(o, s) then t := false;
        if t then begin
          setcell(cur_row, cur_col, key);
          gotoxy(x, y); write(key);
        end else begin
          if key = mcell(cur_row, cur_col) then begin
            gotoxy(2, 22);
            write('#Already entered!#');
          end else begin
            gotoxy(2, 22);
            write('#Not valid here#');
          end;
        end;
      end;
    end;

    if not quit then begin
      if y < 2  then y := 2;
      if y > 18 then y := 18;
      if x < 4  then x := 4;
      if x > 52 then x := 52;
      gotoxy(x, y);
      v := true;
      for k := 1 to 9 do
        for i := 1 to 9 do
          if mcell(i, k) = ' ' then v := false;
      if v then quit := true;
    end;
  end;

  play_again := false;
  if v then begin
    clrscr;
    gotoxy(17, 11); write('Excellent! You solved the puzzle!');
    gotoxy(17, 13); write('Play again? (Y/N)');
    repeat
      yn := readkey;
    until (upcase(yn) = 'Y') or (upcase(yn) = 'N');
    play_again := upcase(yn) = 'Y';
  end;
end;

begin
  clrscr;
  gotoxy(11, 11); write('*Welcome to Sudoku!*');
  gotoxy(11, 12); write('Press ENTER to start...');
  gotoxy(9,  24); write('Adapted from CrYmFoX 2016.');
  repeat
    yn := readkey;
  until yn = chr(13);

  play_again := true;
  while play_again do begin
    v := false;
    run_game;
  end;

  clrscr;
  gotoxy(20, 16); write('Thanks for playing! Press ENTER...');
  repeat yn := readkey; until yn = chr(13);
end.
