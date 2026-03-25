program Pets;

type
Pet = object
  name : string;
  speak: procedure;
end;

Cat = object(Pet)
  procedure factorial(n: integer);
end;

Dog = object(Pet)
end;

function factorial(n: integer) : integer;
begin
  if (n < 2) then factorial := 1
  else factorial := n*factorial(n-1);
end;

procedure Pet.init(n:string);
begin
  name := n;
end;

procedure Cat.speak;
begin
  writeln(name, ': meow!');
end;

procedure Cat.factorial(n : integer);
var f: integer;
begin
  f := factorial(n);
  writeln(name, ': The factorial of ', n, ' is ', f, '.');
end;

procedure Dog.speak;
begin
  writeln(name, ': bark!');
end;
  
var 
pico: Cat;
snuggles: Dog;

begin
  pico.init('Pico');
  snuggles.init('Snuggles');
  pico.speak;
  pico.factorial(6);
  snuggles.speak;
end.
