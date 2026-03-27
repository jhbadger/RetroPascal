program Pets;

type
Pet = object
  name : string;
  procedure speak;
  procedure init(n: string);
end;

Cat = object(Pet)
  procedure speak;  // Forward declaration
  procedure factorial(n: integer);
end;

Bird = object(Pet)
   procedure speak;  // Forward declaration
end;


Dog = object(Pet)
  procedure speak;
end;


function fac(n: integer) : integer;
begin
  if (n < 2) then fac := 1
  else fac := n*fac(n-1);
end;

procedure Pet.init(n:string);
begin
  name := n;
end;

procedure Pet.speak;
begin
   writeln(name, ': [generic pet noises]');
end;

procedure Cat.speak;
begin
  writeln(name, ': meow!');
end;

procedure Cat.factorial(n : integer);
var f: integer;
begin
  f := fac(n);
  writeln(name, ': The factorial of ', n, ' is ', f, '.');
end;

procedure Bird.speak;
begin
  writeln(name, ': chirp chirp!');
end;

procedure Dog.speak;
begin
  writeln(name, ': bark!');
end;
  
var 
pico: Cat;
snuggles: Dog;
tweety: Bird;
pety: Pet;

begin
  pico.init('Pico');
  snuggles.init('Snuggles');
  tweety.init('Tweety');
  pety.init('Pety');
  pico.speak;
  pico.factorial(6);
  snuggles.speak;
  tweety.speak;
  pety.speak;
end.
