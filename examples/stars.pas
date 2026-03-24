program Stars;
var
  i, x, y, c: integer;
  gd, gm: integer;
begin
  gd := 0; gm := 0;
  initgraph(gd, gm, '');
  if graphresult <> 0 then halt;

  randomize;
  setbkcolor(1);
  cleardevice;

  for i := 1 to 300 do begin
    x := random(640);
    y := random(480);
    c := random(15) + 1;
    putpixel(x, y, c)
  end;

  setcolor(14);
  outtextxy(200, 220, 'Press any key...');
  readkey;
  closegraph
end.