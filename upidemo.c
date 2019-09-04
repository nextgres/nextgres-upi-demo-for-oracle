/* ========================================================================= */
/* INTERACTIVE SQL EXAMPLE                                                   */
/* Oracle User Program Interface (UPI)                                       */
/*                                                                           */
/*                                  Jonah H. Harris <jonah.harris@gmail.com> */
/*                                          http://www.oracle-internals.com/ */
/* ========================================================================= */

/* Standard Headers */
#include <stdio.h>                                /* Standard C Input/Output */
#include <stdlib.h>                               /* Standard C Library */

/* Oracle-specific Headers */
#include <oratypes.h>                      /* Oracle-declared Platform Types */
#include "upidef.h"                        /* My UPI Definitions */

/* ========================================================================= */
/* << DEFINITIONS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
/* ========================================================================= */

#define MAXATTR    1024           /* Max number of attributes in select-list */

/* ========================================================================= */
/* << STATIC VARIABLES >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
/* ========================================================================= */

static ub1 hda[512] = { 0 };                       /* Host Data Area for UPI */
static uword cnum;                                 /* Cursor Number */

/* ========================================================================= */
/* << FORWARD DECLARATIONS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
/* ========================================================================= */

static void upi_error(void);

/* ========================================================================= */
/* << PUBLIC FUNCTIONS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
/* ========================================================================= */

int
main (
  int    argc,
  char **argv
) {
  char sql_statement[256];
  sword i, pos;
  sword ret;

  /* Describe-related variables */
  ub1 podt[MAXATTR];
  ub1 ppre[MAXATTR];
  ub1 pnul[MAXATTR];
  ub2 pmxl[MAXATTR];
  ub2 coll[MAXATTR];
  ub2 pmxdisl[MAXATTR];
  ub2 pvll[MAXATTR];
  sb1 pscl[MAXATTR];
  sb2 perc[MAXATTR];

  /* Result-related variables */
  text coln[MAXATTR][31];             /* Array of Column Names */
  text *obuf[MAXATTR];                /* Array of Output (Column) Buffers */
  sb2 oind[MAXATTR];                  /* Array of Output (Column) Indicators */

  /* Logon */
  if (upilog(&hda[0],
    (text *) "scott/tiger@orcl",
    -1, 0,
    -1, 0,
    -1, UPILFDEF, UPILMDEF)) {
    printf("Cannot connect as scott. Exiting...\n");
    exit(1);
  }

  /* Open a cursor */
  if (upiopn(&hda[0], &cnum, -1)) {
    printf("couldn't open cursor!\n");
    upi_error();
    exit(1);
  }

  /* Main loop */
  while (true) {
    ub4 rnum = 0;       /* A generic record number counter */

    printf("\nEnter a query or \"exit\"> ");
    fgets(sql_statement, sizeof(sql_statement), stdin);

    /* If user typed, "exit", bail! */
    if (strncmp(sql_statement, "exit", 4) == 0) {
      break;
    }

    /* Parse the statement */
    if (upiosd(&hda[0], cnum, (text *) sql_statement, -1,
      (ub4) 1, (sword) 1)) {
      upi_error();
      continue;
    }

    /* Iterate over each attribute in the select-list */
    for (pos = 1; pos < MAXATTR; pos++) {
      coll[pos] = sizeof(coln[pos]);

      /* Describe the select-list item */
      ret = upidsc(&hda[0], cnum, pos,
          &pmxl[pos], &pvll[pos],
          &perc[pos], &podt[pos], (text *) &coln[pos],
          &coll[pos], &pmxdisl[pos],
          &ppre[pos], &pscl[pos],
          &pnul[pos]);

      if (coll[pos] > sizeof(coln[pos])) {
        coll[pos] = sizeof(coln[pos]);
      }

      /* Make sure this string is null-terminated */
      coln[pos][coll[pos]] = '\0';

      if (ret) {
        if (ret == 1007) {
          break;
        }
        upi_error();
        continue;
      }
    }

    /*
     * Print out the total count and the names of the select-list
     * items, column sizes, and datatype codes.
     */
    pos--;
    printf("\nThere were %d select-list items.\n", pos);
    printf("Item name                     Length   Datatype\n");
    printf("\n");
    for (i = 1; i <= pos; i++) {
      printf("%*.*s", coll[i], coll[i], coln[i]);
      printf("%*c", 31 - coll[i], ' ');
      printf("%6d   %8d\n", coll[i], podt[i]);

      /* Allocate the output */
      obuf[i] = (text *) malloc(coll[i]+1);
      if (obuf[i] == NULL) {
        fprintf(stderr, "Failed to allocate %u bytes!\n",
          coll[i]+1);
        exit(EXIT_FAILURE);
      }

      /* Define the output parameter */
      if (upidfn(&hda[0], cnum, (sword) i,
        obuf[i], coll[i],
        (sword) SQLT_STR, (b2 *) &oind[i],
        (text *) 0, (size_t) 0,
        (ub2 *) 0, (ub2 *) 0,
        (sword) 0)) {
        upi_error();
        break;
      }
    }

    /* Execute */
    if (upiexe(&hda[0], cnum)) {
      upi_error();
      continue;
    }

    /* Fetch & Display */
    while (upifch(&hda[0], cnum) == 0) {
      printf("\n-- RECORD %04u -------------------------\n", ++rnum);
      for (i = 1; i <= pos; i++) {
        printf("%s", coln[i]);
        printf("%*c ", 31 - coll[i], ':');
        printf("%s\n", (oind[i]) ? "NULL" : obuf[i]);
      }
    }

    /* Display the summary */
    printf("\n%u record%s returned.\n", rnum, (rnum == 1) ? "" : "s");

    /* Cleanup */
    for (i = 1; i <= pos; i++) {
      if (obuf[i] != NULL) {
        free(obuf[i]);
      }
    }
  } /* while() */

  /* Close the cursor */
  upicls(&hda[0], cnum);

  /* Logoff */
  upilof(&hda[0]);

  /* Bail! */
  exit(EXIT_SUCCESS);
} /* main() */


/* ========================================================================= */
/* << PRIVATE FUNCTIONS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
/* ========================================================================= */

static void
upi_error (
  void
) {
  text msg[512] = { 0 };

  fprintf(stderr, "\nOracle ERROR\n");
  upigem(&hda[0], (text *) &msg);
  fprintf(stderr, "%s", msg);
  fflush(stderr);
} /* upi_error() */
