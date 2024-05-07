// for some reason -fno-math-errno doesn't work in devkitpro
int *__errno () {
  return 0;
}
