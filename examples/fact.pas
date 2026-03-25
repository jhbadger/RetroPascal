program facter;

function fact(x: integer) : integer;
begin
  if ( x < 2) then fact := 1
  else fact := x*fact(x-1);
end;

var x: integer;
begin
  write('Enter a number: ');
  readln(x);
  writeln(fact(x));
end.
