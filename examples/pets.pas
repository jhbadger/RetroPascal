program Pets;

type
Pet = object
  name : string;
  speak: procedure;
end;

Cat = object(Pet)
end;

Dog = object(Pet)
end;

procedure Pet.init(n:string);
begin
  name := n;
end;

procedure Cat.speak;
begin
  writeln(name, ': meow!');
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
  snuggles.speak;
end.
