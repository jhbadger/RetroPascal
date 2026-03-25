program RecTest;
type
  TPoint = record
    name: string;
    x: real;
    y: real;
  end;
  
var p: TPoint;
begin
  p.name := 'Mr. Pointy';;
  p.x := 3.0;
  p.y := 4.0;
  writeln('x=', p.x, ' y=', p.y);
  writeln(p.name, ': dist=', sqrt(p.x*p.x + p.y*p.y));
end.
