program Snake_Game;

{ Arrow key codes returned by readkey as single chars }
const
  KEY_UP    = #1;
  KEY_DOWN  = #2;
  KEY_LEFT  = #3;
  KEY_RIGHT = #4;

var
  body, c, cc, head, goc, wcc: Char;
  fx, fy: Integer;
  nr, nd1, k1: Integer;
  b, finish:   Boolean;
  i, j, k, x, y, cx, cy, cxp, cyp, nd2, ndp, s, slength, Count: Integer;
  m:   array [1..16, 1..61] of Char;
  tx, ty: array [1..826] of Integer;

  procedure initialisations;
  begin
    nd1 := 1;
    nd2 := 370;
    ndp := 120;
    nr  := 3;
  end;

  procedure limits;
  begin
    clrscr;
    for k := 1 to 16 do
      for i := 1 to 61 do
        m[k, i] := ' ';
    for i := 1 to 61 do m[1, i] := '#';
    for i := 2 to 16 do m[i, 1] := '#';
    for i := 2 to 61 do m[16, i] := '#';
    for i := 2 to 15 do m[i, 61] := '#';
    body := 'O';
    head := '@';
    for i := 3 to 6 do m[8, i] := body;
    m[8, 7] := head;
  end;

  procedure delay(time: Integer);
  var timer: Integer;
  begin
    timer := gettickcount + time;
    repeat until gettickcount >= timer;
  end;

  procedure writein(x1, y1: Integer; s1: String);
  begin
    gotoxy(x1, y1);
    Write(s1);
  end;

  procedure display;
  begin
    j := 5;
    gotoxy(10, j);
    for k := 1 to 16 do begin
      j := j + 1;
      for i := 1 to 61 do Write(m[k, i]);
      gotoxy(10, j);
    end;
  end;

  procedure start;
  begin
    slength := 5;
    Count   := 0;
    s       := 0;
    x       := 7;
    y       := 8;
    k       := 3;
    for i := 1 to slength do begin
      tx[i] := k;
      ty[i] := 8;
      k := k + 1;
    end;
    for i := 16 to 64 do writein(i, 3, '_');
    k := 15;
    repeat
      for i := 1 to 3 do writein(k, i, '|');
      k := k + 25;
    until k = 90;
    writein(22, 2, 'Score : 0000');
    writein(44, 2, 'Snake length : 005');
    for i := 1 to 80 do writein(i, 22, '-');
    writein(4,  24, 'Up=^');
    writein(15, 24, 'Down=v');
    writein(28, 24, 'Left=<');
    writein(41, 24, 'Right=>');
    writein(55, 24, 'Pause=P');
    c := KEY_RIGHT;
    for i := 23 to 57 do
      for k := 11 to 13 do
        writein(i, k, ' ');
    repeat
      k := 10;
      repeat
        for i := 23 to 57 do writein(i, k, '_');
        k := k + 4;
      until k = 18;
      k := 22;
      repeat
        for i := 11 to 14 do writein(k, i, '|');
        k := k + 36;
      until k = 94;
      writein(24, 12, 'Press any arrow key to start...');
      delay(500);
      for i := 23 to 57 do
        for k := 11 to 13 do
          writein(i, k, ' ');
      delay(500);
      if keypressed then c := readkey;
    until c in [KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT];
    c := KEY_RIGHT;
  end;

  procedure walk;

    procedure move_snake(nox, noy: Integer);

      procedure move_body;
        procedure erase_tail;
        begin
          Count := Count + 1;
          m[ty[Count], tx[Count]] := ' ';
          writein(tx[Count] + 9, ty[Count] + 4, ' ');
          ty[Count] := y;
          tx[Count] := x;
          if Count = slength then Count := 0;
        end;
      begin
        erase_tail;
        m[y, x] := head;
        writein(x + 9, y + 4, head);
      end;

    begin
      x := x + nox;
      y := y + noy;
      move_body;
      m[y - noy, x - nox] := body;
      writein(x + 9 - nox, y + 4 - noy, body);
    end;

    procedure verify(vc: Char);
    begin
      if cc = vc then c := cc;
    end;

  begin
    case c of
      KEY_RIGHT: move_snake(1, 0);
      KEY_LEFT:  move_snake(-1, 0);
      KEY_UP:    move_snake(0, -1);
      KEY_DOWN:  move_snake(0, 1);
    end;
    cc := c;
    if keypressed then begin
      c := readkey;
      if not (c in [KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, #27, 'p']) then
        c := cc;
      case c of
        KEY_RIGHT: verify(KEY_LEFT);
        KEY_LEFT:  verify(KEY_RIGHT);
        KEY_UP:    verify(KEY_DOWN);
        KEY_DOWN:  verify(KEY_UP);
      end;
    end;
    delay(ndp);
  end;

  function in_wall_or_himself: Boolean;
    procedure verify(ny, nx: Integer);
    begin
      if (m[y + ny, x + nx] = '#') or (m[y + ny, x + nx] = body) then
        in_wall_or_himself := True
      else
        in_wall_or_himself := False;
    end;
  begin
    case c of
      KEY_RIGHT: verify(0, 1);
      KEY_LEFT:  verify(0, -1);
      KEY_UP:    verify(-1, 0);
      KEY_DOWN:  verify(1, 0);
    end;
  end;

  procedure food;
  begin
    randomize;
    repeat
      fy := random(13) + 2;
      fx := random(58) + 2;
    until m[fy, fx] = ' ';
    m[fy, fx] := '+';
    writein(fx + 9, fy + 4, '+');
  end;

  function food_eaten: Boolean;
  begin
    b := True;
    for k := 2 to 15 do
      for i := 2 to 60 do
        b := b and not (m[k, i] = '+');
    food_eaten := b;
  end;

  procedure enlarge;
  begin
    slength := slength + 1;
    for i := slength downto Count + 2 do begin
      tx[i] := tx[i - 1];
      ty[i] := ty[i - 1];
    end;
    repeat
      ty[Count + 1] := random(13) + 2;
      tx[Count + 1] := random(58) + 2;
    until m[ty[Count + 1], tx[Count + 1]] = ' ';
  end;

  procedure score;
  var ns, nl: String;
    procedure conv_s(no, f: Integer; var ch: String);
    begin
      str(no:f, ch);
      repeat
        insert('0', ch, pos(' ', ch));
        delete(ch, pos(' ', ch) + 1, 1);
      until pos(' ', ch) = 0;
    end;
  begin
    s := s + nr + 1;
    conv_s(s, 4, ns);
    conv_s(slength, 3, nl);
    writein(30, 2, ns);
    writein(59, 2, nl);
  end;

  procedure instructions;
  var mc: Char;
  begin
    clrscr;
    writein(21, 7,  ' * Make the snake grow longer by');
    writein(21, 8,  'directing it to the food using the arrow keys.');
    writein(21, 11, ' * Your goal is to grow as long a tail');
    writein(21, 12, 'as possible by eating the character "+".');
    writein(21, 14, ' * The game is over if you hit the level');
    writein(21, 15, 'borders "#" or your tail.');
    for i := 1 to 80 do writein(i, 22, '-');
    writein(36, 24, 'Back=Enter');
    repeat gotoxy(46, 15); mc := readkey; until mc = #13;
  end;

  procedure about;
  var mc: Char;
  begin
    clrscr;
    writein(20, 7,  '.Snake_Game created By -Sameur Ben Hmouda-.');
    writein(20, 11, '   All rights reserved to CrYmFoX 2017.');
    for i := 1 to 80 do writein(i, 22, '-');
    writein(36, 24, 'Back=Enter');
    repeat gotoxy(59, 11); mc := readkey; until mc = #13;
  end;

  procedure settings;
    procedure move_cursor(int1: Integer);
    begin
      writein(k1, 20, ' ');
      k1 := k1 + int1;
      writein(k1, 20, '^');
    end;
    procedure increasement(int2: Integer);
    begin
      nr := nr + int2;
      for i := 18 downto 18 - nr do writein(k1, i, '|');
      if int2 < 0 then
        for k := 18 downto 18 - nr - 1 do writein(k1 + 4, k, ' ');
    end;
  var mc: Char;
  begin
    clrscr;
    ndp := 120;
    writein(22, 7, 'Speed level :');
    for i := 1 to 80 do writein(i, 22, '-');
    writein(4,  24, 'Apply=Enter');
    writein(36, 24, 'Left=<(-)');
    writein(70, 24, 'Right=>(+)');
    k1 := 25;
    repeat writein(k1, 19, 'U'); k1 := k1 + 4 until k1 = 61;
    k1 := 25; nr := 1;
    repeat
      for i := 1 to nr do writein(k1, 19 - i, '|');
      k1 := k1 + 4; nr := nr + 1
    until k1 = 41;
    k1 := 37; nr := 3;
    repeat
      repeat
        writein(k1, 20, '^');
        gotoxy(k1, 20);
        mc := readkey;
      until mc in [KEY_LEFT, KEY_RIGHT, #13];
      case mc of
        KEY_RIGHT: begin move_cursor(4);  increasement(1);  ndp := ndp - 20; end;
        KEY_LEFT:  begin move_cursor(-4); increasement(-1); ndp := ndp + 20; end;
      end;
      case k1 of
        21: begin move_cursor(4);  increasement(1);  ndp := ndp - 20; end;
        61: begin move_cursor(-4); increasement(-1); ndp := ndp + 20; end;
      end;
    until mc = #13;
  end;

  function do_main_menu: Boolean;
    procedure cursor;
    var mc: Char;
      procedure move_cursor(int: Integer);
      begin
        writein(cx - 1, cy, ' ');
        cy := cy + int;
        writein(cx - 1, cy, '>');
        gotoxy(cx, cy);
      end;
    begin
      cx := 30; cy := 12;
      writein(cx - 1, cy, '>');
      gotoxy(cx, cy);
      repeat
        repeat mc := readkey; until mc in [KEY_UP, KEY_DOWN, #13];
        case mc of
          KEY_DOWN: move_cursor(2);
          KEY_UP:   move_cursor(-2);
        end;
        case cy of
          10: move_cursor(2);
          22: begin move_cursor(-2); writein(cx-1, cy+2, '-'); gotoxy(cx, cy); end;
        end;
      until mc = #13;
    end;
  var Name: array [1..5] of String;
  begin
    clrscr;
    for i := 1 to 80 do writein(i, 22, '-');
    writein(4,  24, 'Up=^');
    writein(25, 24, 'Down=v');
    writein(50, 24, 'Select=Enter');
    Name[1] := '  ____              __             ___         ___       __               ___';
    Name[2] := ' /       |\   |    /  \    |  /   |           /   \     /  \    |\  /|   |';
    Name[3] := ' \____   | \  |   |____|   |_/    |___       |   __    |____|   | \/ |   |___';
    Name[4] := '      \  |  \ |   |    |   | \    |          |     \   |    |   |    |   |';
    Name[5] := ' _____/  |   \|   |    |   |  \   |___        \____/   |    |   |    |   |___';
    writein(1, 1, Name[1]);
    for i := 2 to 5 do begin
      for k := 1 to length(Name[i]) do begin
        delay(nd1);
        writein(k, i, Name[i][k]);
      end;
      writeln;
    end;
    for i := 40 to 44 do writein(i, 6, '_');
    delay(nd2);
    for i := 27 to 57 do writein(i, 9, '~');
    writein(31, 12, '+    Start the game');
    writein(31, 14, '+     Instructions');
    writein(31, 16, '+       Settings');
    writein(31, 18, '+        About');
    writein(31, 20, '+        Exit');
    cursor;
    nd1 := 0; nd2 := 0;
    while cy <> 12 do begin
      case cy of
        14: instructions;
        16: settings;
        18: about;
        20: begin do_main_menu := False; exit; end;
      end;
      clrscr;
      for i := 1 to 80 do writein(i, 22, '-');
      writein(4,  24, 'Up=^');
      writein(25, 24, 'Down=v');
      writein(50, 24, 'Select=Enter');
      writein(31, 12, '+    Start the game');
      writein(31, 14, '+     Instructions');
      writein(31, 16, '+       Settings');
      writein(31, 18, '+        About');
      writein(31, 20, '+        Exit');
      cursor;
    end;
    do_main_menu := True;
  end;

  function do_pause_menu: Integer;
    procedure cursor;
    var mc: Char;
      procedure move_cursor(int: Integer);
      begin
        writein(cxp - 1, cyp, ' ');
        cyp := cyp + int;
        writein(cxp - 1, cyp, '>');
        gotoxy(cxp, cyp);
      end;
    begin
      cxp := 34; cyp := 12;
      writein(cxp - 1, cyp, '>');
      gotoxy(cxp, cyp);
      repeat
        repeat mc := readkey; until mc in [KEY_UP, KEY_DOWN, #13];
        case mc of
          KEY_DOWN: move_cursor(2);
          KEY_UP:   move_cursor(-2);
        end;
        case cyp of
          10: move_cursor(2);
          18: begin move_cursor(-2); writein(cxp-1, cyp+2, '_'); gotoxy(cxp, cyp); end;
        end;
      until mc = #13;
    end;
  begin
    for i := 31 to 50 do
      for k := 7 to 18 do writein(i, k, ' ');
    k := 6;
    repeat for i := 31 to 50 do writein(i, k, '_'); k := k + 12; until k = 30;
    k := 30;
    repeat for i := 7 to 18 do writein(k, i, '|'); k := k + 21; until k = 72;
    for i := 31 to 50 do writein(i, 9, '_');
    writein(35, 8,  '-Pause Menu-');
    writein(35, 12, '+ Continue');
    writein(35, 14, '+ New Game');
    writein(35, 16, '+   Quit');
    cursor;
    case cyp of
      12: do_pause_menu := 0;
      14: do_pause_menu := 1;
      16: do_pause_menu := 2;
    end;
  end;

  procedure game_over;
  begin
    for i := 1 to 9 do begin
      writein(x+9, y+4, '-'); delay(25);
      writein(x+9, y+4, '\'); delay(25);
      writein(x+9, y+4, '|'); delay(25);
      writein(x+9, y+4, '/'); delay(25);
    end;
    gotoxy(x+9, y+4); Write('X');
    delay(730);
    for i := 29 to 52 do
      for k := 10 to 15 do writein(i, k, ' ');
    k := 9;
    repeat for i := 29 to 52 do writein(i, k, '_'); k := k + 6; until k = 21;
    k := 28;
    repeat for i := 10 to 15 do writein(k, i, '|'); k := k + 25; until k = 78;
    writein(36, 11, 'Game Over');
    writein(33, 13, 'Your score : ');
    gotoxy(46, 13); Write(s);
    writein(32, 14, 'Replay? (Y/N)...< >');
    repeat gotoxy(49, 14); goc := readkey; until goc in ['y', 'n'];
  end;

  function game_complete: Boolean;
  begin
    finish := True;
    for k := 2 to 15 do
      for i := 2 to 60 do
        finish := finish and ((m[k,i] = body) or (m[k,i] = head) or (m[k,i] = '+'));
    game_complete := finish;
  end;

  procedure winner_congrat;
  begin
    for k := 10 to 14 do
      for i := 27 to 53 do writein(i, k, ' ');
    for i := 27 to 53 do writein(i, 9, '_');
    for i := 27 to 53 do writein(i, 15, '_');
    for i := 10 to 15 do writein(26, i, '|');
    for i := 10 to 15 do writein(54, i, '|');
    writein(32, 11, 'Congratulations!');
    writein(28, 13, 'You have beaten the game.');
    writein(37, 14, '.......');
    for i := 1 to 5 do
      for k := 37 to 44 do begin
        delay(95);
        if not (k = 44) then begin
          writein(k, 14, '-');
          if not (k = 37) then writein(k-1, 14, '.');
        end else
          writein(k-1, 14, '.');
      end;
    writein(31, 14, 'Play again? (Y/N) < >');
    repeat gotoxy(50, 14); wcc := readkey; until wcc in ['y', 'n'];
  end;

  function run_game: Boolean;
  begin
    limits;
    display;
    food;
    start;
    display;
    run_game := False;
    while True do begin
      walk;
      if c = 'p' then begin
        case do_pause_menu of
          0: c := cc;
          1: begin run_game := True;  exit; end;
          2: begin run_game := False; exit; end;
        end;
      end;
      if game_complete then begin
        winner_congrat;
        if wcc = 'y' then begin run_game := True;  exit; end
        else               begin run_game := False; exit; end;
      end;
      if food_eaten then begin
        food; enlarge; score;
      end;
      if in_wall_or_himself then begin
        game_over;
        if goc = 'y' then begin run_game := True;  exit; end
        else               begin run_game := False; exit; end;
      end;
    end;
  end;

begin
  initialisations;
  while do_main_menu do
    while run_game do;
end.
