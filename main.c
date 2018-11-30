#include <stdio.h>

void five_times() {
  int i = 5;
  do {
    puts("Hello, world");
  } while (i-- >0);
  return;
}

int main(int argc, char** argv) {
  five_times();
  return 0;
}
