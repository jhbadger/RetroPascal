program RecTest;
type
  TPoint = record
    x: real;
    y: real;
  end;
var p: TPoint;
begin
  p.x := 3.0;
  p.y := 4.0;
  writeln('x=', p.x, ' y=', p.y);
  writeln('dist=', sqrt(p.x*p.x + p.y*p.y))
end.
