
// gcc -o start   -g  -Wall -Wmissing-prototypes -Wunused -Wshadow -Wpointer-arith -falign-jumps=1 -falign-loops=1 -falign-functions=1 -Wundef  start.c
/* __start
int
main ()
{
asm ("incl _start")
  ;
  return 0;
}
*/

/* __bss_start
int
main ()
{
asm ("incl __bss_start")
  ;
  return 0;
}
*/
/*
int
main ()
{
asm ("incl edata")      / OK /
  ;
  return 0;
}
*/
/*
int
main ()
{
asm ("incl _edata")    / OK /
  ;
  return 0;
}
*/
/*
int
main ()
{
asm ("incl end")      / OK /
  ;
  return 0;
}
*/
int
main ()
{
asm ("incl _end")
  ;
  return 0;
}



