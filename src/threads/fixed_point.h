// Using 17.14 floating point format 
// as given in manual
#define p 17
#define q (31 - p)
#define f (1 << q)

// Convert int to fixed point
#define int_to_fixed_point(n) ((n) * (f))

// Addition and subtraction of fixed point values
#define add(x, y) ((x) + (y))
#define subtract(x, y) ((x) - (y))

// Round off to zero and nearest integer
#define round_off_to_zero(x) ((x) / (f))
#define round_off(x) (x >= 0 ? ((x + ((f) >> (1))) / (f)) : ((x - ((f) >> (1))) / (f)))

// Addition and subtraction of fixed point values with integers
#define add_fixed_point_int(x, n) ((x) + ((n) * (f)))
#define sub_fixed_point_int(x, n) ((x) - ((n) * (f)))

// Multiplication within fixed point values and with integers
#define mult(x, y) ((((int64_t) x) * (y)) / f)
#define mult_fixed_point_int(x, n) ((x) * (n))

// Division within fixed point values and with integers
#define div(x, y) ((((int64_t) x) * f) / (y))
#define div_fixed_point_int(x, n) ((x) / (n))

