program Fibonacci;
function fib(n: integer): integer;
begin
  if n <= 1 then fib := n
  else fib := fib(n-1) + fib(n-2)
end;
var i: integer;
begin
  for i := 0 to 30 do
    writeln('fib(', i, ') = ', fib(i))
end.