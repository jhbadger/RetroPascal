program Mandelbrot;
var
  gd, gm: integer;
  px, py, iter, maxiter: integer;
  x, y, x0, y0, xtemp: real;
begin
  gd := 0; gm := 0;
  initgraph(gd, gm, '');
  if graphresult <> 0 then halt;

  maxiter := 32;   { keep low for speed in the interpreter }
  cleardevice;
  outtextxy(220, 230, 'Calculating - please wait...');

  for px := 0 to getmaxx do begin
    for py := 0 to getmaxy do begin
      x0 := (px / getmaxx) * 3.5 - 2.5;
      y0 := (py / getmaxy) * 2.0 - 1.0;
      x := 0; y := 0;
      iter := 0;
      while (x*x + y*y <= 4.0) and (iter < maxiter) do begin
        xtemp := x*x - y*y + x0;
        y := 2*x*y + y0;
        x := xtemp;
        iter := iter + 1
      end;
      if iter < maxiter then
        putpixel(px, py, (iter mod 15) + 1)
      else
        putpixel(px, py, 0)
    end
  end;

  outtextxy(10, 10, 'Mandelbrot Set - press a key');
  readkey;
  closegraph
end.