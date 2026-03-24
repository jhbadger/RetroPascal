program BouncingBall;
var
  gd, gm: integer;
  x, y, dx, dy, r: integer;
  i: integer;
begin
  gd := 0; gm := 0;
  initgraph(gd, gm, '');
  if graphresult <> 0 then halt;

  x := 320; y := 240;
  dx := 3; dy := 2;
  r := 20;

  for i := 1 to 500 do begin
    { erase old ball }
    setcolor(0);
    setfillstyle(1, 0);
    fillellipse(x, y, r, r);

    { move }
    x := x + dx;
    y := y + dy;
    if (x - r < 0) or (x + r > getmaxx) then dx := -dx;
    if (y - r < 0) or (y + r > getmaxy) then dy := -dy;

    { draw new ball }
    setcolor(14);
    setfillstyle(1, 12);
    fillellipse(x, y, r, r);

    delay(16)
  end;

  readkey;
  closegraph
end.