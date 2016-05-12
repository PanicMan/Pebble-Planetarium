#define PI          3.14159265358979323846
#define TPI			2*PI
#define RADEG       (180.0/PI)
#define DEGRAD      (PI/180.0)
#define sind(x)     sin((x)*DEGRAD)
#define cosd(x)     cos((x)*DEGRAD)
#define tand(x)     tan((x)*DEGRAD)
#define asind(x)    (RADEG*asin(x))
#define acosd(x)    (RADEG*acos(x))
#define atand(x)    (RADEG*atan(x))
#define atan2d(y,x) (RADEG*atan2((y),(x)))
#define min(x,y)	(x < y ? x : y)
#define max(x,y)	(x > y ? x : y)

#define FNday(y, m, d, h) (367 * y - 7 * (y + (m + 9) / 12) / 4 + 275 * m / 9 + d - 730530 + h / 24)

double Date2J2000 (double y, double m, double d, double h)
{
	return (367.0 * y - 7.0 * (y + (m + 9.0) / 12.0) / 4.0 + 275.0 * m / 9.0 + d - 730530.0 + h / 24.0);
}

double my_floor(double x)
{
	if(x < 0.0)
		return ((int32_t)x)-1;        
	else
		return ((int32_t)x);
}

double rev( double x )
{
	return  x - my_floor(x/360.0)*360.0;
}

void DrawEllipse(GContext *ctx, int16_t x, int16_t y, int16_t w, int16_t h, int16_t a, int16_t e)
{
	GPoint pt1, pt2;
	
	//Begin always bigger than zero
	while (a < 0)
		a += 360;
	//End always bigger than start
	while (a > e)
		e += 360;
	
	//Line lenght, optimized
	int32_t step = (e-a)/18;
	if (e-a > 180)
		step = (e-a)/72;
	else if (e-a > 90)
		step = (e-a)/36;

	//First Point
	int32_t angle = TRIG_MAX_ANGLE * (a % 360) / 360;
	pt1.x = (sin_lookup(angle) * w / TRIG_MAX_RATIO) + x;
	pt1.y = (-cos_lookup(angle) * h / TRIG_MAX_RATIO) + y;
	
    for(int32_t pos = a+step;  pos <= e;  pos += step)
	{ 
		angle = TRIG_MAX_ANGLE * (pos % 360) / 360;
		pt2.x = (sin_lookup(angle) * w / TRIG_MAX_RATIO) + x;
		pt2.y = (-cos_lookup(angle) * h / TRIG_MAX_RATIO) + y;
		
		graphics_draw_line(ctx, pt1, pt2);
		pt1 = pt2;
	}
}

void DrawArc2(GContext *ctx, GPoint p, int radius, int thickness, int start, int end) 
{
	for (int rad = radius-thickness+1; rad <=radius; rad++)
		DrawEllipse(ctx, p.x, p.y, rad, rad, start, end);
}

void DrawArc(GContext *ctx, GPoint p, int radius, int thickness, int start, int end) 
{
  start = start % 360;
  end = end % 360;
 
  while (start < 0) 
	  start += 360;
  while (end < 0) 
	  end += 360;
 
  if (end == 0) 
	  end = 360;
  
  float sslope = (float)cos_lookup(start * TRIG_MAX_ANGLE / 360) / (float)sin_lookup(start * TRIG_MAX_ANGLE / 360);
  float eslope = (float)cos_lookup(end * TRIG_MAX_ANGLE / 360) / (float)sin_lookup(end * TRIG_MAX_ANGLE / 360);
 
  if (end == 360) 
	  eslope = -1000000;
 
  int ir2 = (radius - thickness) * (radius - thickness);
  int or2 = radius * radius;
 
  for (int x = -radius; x <= radius; x++)
    for (int y = -radius; y <= radius; y++)
    {
      int x2 = x * x;
      int y2 = y * y;
 
      if (
        (x2 + y2 < or2 && x2 + y2 >= ir2) &&
        (
          (y > 0 && start < 180 && x <= y * sslope) ||
          (y < 0 && start > 180 && x >= y * sslope) ||
          (y < 0 && start <= 180) ||
          (y == 0 && start <= 180 && x < 0) ||
          (y == 0 && start == 0 && x > 0)
        ) &&
        (
          (y > 0 && end < 180 && x >= y * eslope) ||
          (y < 0 && end > 180 && x <= y * eslope) ||
          (y > 0 && end >= 180) ||
          (y == 0 && end >= 180 && x < 0) ||
          (y == 0 && start == 0 && x > 0)
        )
      )
        graphics_draw_pixel(ctx, GPoint(p.x + x, p.y + y));
    }
}