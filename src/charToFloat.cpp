
#define isdigit(c) (c >= '0' && c <= '9')

// we could lot let work atof (crash). Could be a toolchain bug and this was
// reimplemented in the mean time this is a simplified version of atof that does
// not deal with 'e' and 'E'

float charToFloat(char* string) {
  // This function stolen from either Rolf Neugebauer or Andrew Tolmach.
  // Probably Rolf.
  double a = 0.0;
  int e = 0;
  int c;
  while ((c = *string++) != '\0' && isdigit(c)) {
    a = a * 10.0 + (c - '0');
  }
  if (c == '.') {
    while ((c = *string++) != '\0' && isdigit(c)) {
      a = a * 10.0 + (c - '0');
      e = e - 1;
    }
  }
  while (e > 0) {
    a *= 10.0;
    e--;
  }
  while (e < 0) {
    a *= 0.1;
    e++;
  }
  return a;
}