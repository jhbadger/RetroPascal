program Objtest;

type
  TShape = object
    color: string;
    procedure init(c: string);
    function area: real;
    function describe: string;
    procedure print;
  end;

  TCircle = object(TShape)
    radius: real;
    procedure init(c: string; r: real);
    function area: real;
    function describe: string;
  end;

  TRectangle = object(TShape)
    width: real;
    height: real;
    procedure init(c: string; w: real; h: real);
    function area: real;
    function describe: string;
  end;

{ TShape methods }
procedure TShape.init(c: string);
begin
  color := c
end;

function TShape.area: real;
begin
  area := 0.0
end;

function TShape.describe: string;
begin
  describe := 'Shape(' + color + ')'
end;

procedure TShape.print;
begin
  writeln(describe, ' area=', area)
end;

{ TCircle methods }
procedure TCircle.init(c: string; r: real);
begin
  color  := c;
  radius := r
end;

function TCircle.area: real;
begin
  area := 3.14159265 * radius * radius
end;

function TCircle.describe: string;
begin
  describe := 'Circle(r=' + color + ')'
end;

{ TRectangle methods }
procedure TRectangle.init(c: string; w: real; h: real);
begin
  color  := c;
  width  := w;
  height := h
end;

function TRectangle.area: real;
begin
  area := width * height
end;

function TRectangle.describe: string;
begin
  describe := 'Rectangle(' + color + ')'
end;

var
  s: TShape;
  c: TCircle;
  r: TRectangle;
  totalArea: real;

begin
  s.init('grey');
  c.init('red', 5.0);
  r.init('blue', 4.0, 6.0);

  writeln('=== Shapes ===');
  s.print;
  c.print;
  r.print;

  totalArea := c.area + r.area;
  writeln('Total area of circle + rectangle = ', totalArea);

  writeln('Circle radius: ', c.radius);
  writeln('Rectangle: ', r.width, ' x ', r.height)
end.