//*********************************************************
//
// PUT YOUR NAME HERE!!!!
// Operating Systems
// Writing Your Own Shell: hfsh
//
//*********************************************************


//*********************************************************
//
// Includes and Defines
//
//*********************************************************
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define STR_MYEXIT "myexit"


//*********************************************************
//
// Extern Declarations
//
//*********************************************************
extern char **gettoks();


//*********************************************************
//
// Function Prototypes
//
//*********************************************************



//*********************************************************
//
// Main Function
//
//*********************************************************
int main( int argc, char *argv[] )
{
  /* local variables */
  int ii;
  char **toks;
  int retval;

  /* initialize local variables */
  ii = 0;
  toks = NULL;
  retval = 0;

  /* main (infinite) loop */
  while(true){
    /* get arguments */
    toks = gettoks();

    if(toks[0] != NULL){
      /* simple loop to echo all arguments */
      for(ii=0; toks[ii] != NULL; ii++){
        printf( "Argument %d: %s\n", ii, toks[ii] );
      }

      if(!strcmp(toks[0], STR_MYEXIT))
        break;
    }
  }

  /* return to calling environment */
  return( retval );
}
