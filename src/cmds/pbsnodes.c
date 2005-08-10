/*
*         OpenPBS (Portable Batch System) v2.3 Software License
* 
* Copyright (c) 1999-2000 Veridian Information Solutions, Inc.
* All rights reserved.
* 
* ---------------------------------------------------------------------------
* For a license to use or redistribute the OpenPBS software under conditions
* other than those described below, or to purchase support for this software,
* please contact Veridian Systems, PBS Products Department ("Licensor") at:
* 
*    www.OpenPBS.org  +1 650 967-4675                  sales@OpenPBS.org
*                        877 902-4PBS (US toll-free)
* ---------------------------------------------------------------------------
* 
* This license covers use of the OpenPBS v2.3 software (the "Software") at
* your site or location, and, for certain users, redistribution of the
* Software to other sites and locations.  Use and redistribution of
* OpenPBS v2.3 in source and binary forms, with or without modification,
* are permitted provided that all of the following conditions are met.
* After December 31, 2001, only conditions 3-6 must be met:
* 
* 1. Commercial and/or non-commercial use of the Software is permitted
*    provided a current software registration is on file at www.OpenPBS.org.
*    If use of this software contributes to a publication, product, or
*    service, proper attribution must be given; see www.OpenPBS.org/credit.html
* 
* 2. Redistribution in any form is only permitted for non-commercial,
*    non-profit purposes.  There can be no charge for the Software or any
*    software incorporating the Software.  Further, there can be no
*    expectation of revenue generated as a consequence of redistributing
*    the Software.
* 
* 3. Any Redistribution of source code must retain the above copyright notice
*    and the acknowledgment contained in paragraph 6, this list of conditions
*    and the disclaimer contained in paragraph 7.
* 
* 4. Any Redistribution in binary form must reproduce the above copyright
*    notice and the acknowledgment contained in paragraph 6, this list of
*    conditions and the disclaimer contained in paragraph 7 in the
*    documentation and/or other materials provided with the distribution.
* 
* 5. Redistributions in any form must be accompanied by information on how to
*    obtain complete source code for the OpenPBS software and any
*    modifications and/or additions to the OpenPBS software.  The source code
*    must either be included in the distribution or be available for no more
*    than the cost of distribution plus a nominal fee, and all modifications
*    and additions to the Software must be freely redistributable by any party
*    (including Licensor) without restriction.
* 
* 6. All advertising materials mentioning features or use of the Software must
*    display the following acknowledgment:
* 
*     "This product includes software developed by NASA Ames Research Center,
*     Lawrence Livermore National Laboratory, and Veridian Information 
*     Solutions, Inc.
*     Visit www.OpenPBS.org for OpenPBS software support,
*     products, and information."
* 
* 7. DISCLAIMER OF WARRANTY
* 
* THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND. ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT
* ARE EXPRESSLY DISCLAIMED.
* 
* IN NO EVENT SHALL VERIDIAN CORPORATION, ITS AFFILIATED COMPANIES, OR THE
* U.S. GOVERNMENT OR ANY OF ITS AGENCIES BE LIABLE FOR ANY DIRECT OR INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
* 
* This license will be governed by the laws of the Commonwealth of Virginia,
* without reference to its choice of law rules.
*/
/*
**	This program exists to give a way to mark nodes
**	Down, Offline, or Free in PBS.
**
**	usage: pbsnodes [-s server][-{c|l|o|r}] node node ...
**
**	where the node(s) are the names given in the node
**	description file.
**
**	pbsnodes			clear "DOWN" from all nodes so marked
** 
**	pbsnodes node1 node2		set nodes node1, node2 "DOWN"
**					unmark "DOWN" from any other node
**
**	pbsnodes -a			list all nodes 
**
**	pbsnodes -l			list all nodes marked in any way
**
**	pbsnodes -o node1 node2		mark nodes node1, node2 as OFF_LINE
**					even if currently in use.
**
**	pbsnodes -r node1 node2 	clear OFF_LINE from listed nodes
**
**	pbsnodes -c node1 node2		clear OFF_LINE or DOWN from listed nodes
*/
#include	<pbs_config.h>   /* the master config generated by configure */

#include	<stdio.h>
#include	<unistd.h>
#include	<string.h>
#include	<stdlib.h>

#include	"portability.h"
#include	"pbs_ifl.h"
#include        "pbs_version.h"
#include        "mcom.h"
  
#define	DOWN	0
#define	LIST	1
#define	CLEAR	2
#define	OFFLINE	3
#define	RESET	4
#define ALLI	5
#define PURGE   6
#define DIAG    7

int quiet = 0;

/* prototypes */

extern int MXMLCreateE(mxml_t **,char *);
extern int MXMLAddE(mxml_t *,mxml_t *);
extern int MXMLSetVal(mxml_t *,char *);
extern int MXMLDestroyE(mxml_t **);
extern int MXMLToString(mxml_t *,char *,int,int *,mbool_t);

/* END prototypes */

/* globals */

mbool_t DisplayXML = FALSE;

/* END globals */


/*
 * cmp_node_name - compare two node names, allow the second to match the
 *	first if the same up to a dot ('.') in the second; i.e.
 *	"foo" == "foo.bar"
 *
 *	return 0 if match, 1 if not
 */

static int cmp_node_name(

  char *n1, 
  char *n2)

  {
  while ((*n1 != '\0') && (*n2 != '\0')) 
    {
    if (*n1 != *n2)
      break;

    n1++;
    n2++;
    }

  if (*n1 == *n2) 
    {
    return(0);
    }

  if ((*n1 == '.') && (*n2 == '\0'))
    {
    return(0);
    }

  return(1);
  }




static void prt_node_attr(

  struct batch_status *pbs)  /* I */

  {
  struct attrl *pat;

  for (pat = pbs->attribs;pat;pat = pat->next) 
    {
    printf("     %s = %s\n", 
      pat->name, 
      pat->value);
    }

  return;
  }




static char *get_nstate(

  struct batch_status *pbs)  /* I */

  {
  struct attrl *pat;

  for (pat = pbs->attribs;pat != NULL;pat = pat->next) 
    {
    if (strcmp(pat->name,ATTR_NODE_state) == 0)
      {
      return(pat->value);
      }
    }

  return("");
  }





/*
 * is_down - returns indication if node is marked down or not
 *
 *	returns 1 if node is down and 0 if not down
 */

static int is_down(

  struct batch_status *pbs)  /* I */

  {

  if (strstr(get_nstate(pbs),ND_down) != NULL)
    {
    return(1);
    }

  return(0);
  }





static int is_offline(

  struct batch_status *pbs)

  {

  if (strstr(get_nstate(pbs),ND_offline) != NULL)
    {
    return(1);
    }

  return(0);
  }




static int is_unknown(

  struct batch_status *pbs)

  {

  if (strstr(get_nstate(pbs),ND_state_unknown) != NULL)
    {
    return(1);
    }

  return(0);
  }




static int marknode(

  int   con, 
  char *name, 
  char *state1, 
  enum batch_op op1,
  char *state2, 
  enum batch_op op2)

  {
  char	         *errmsg;
  struct attropl  new[2];
  int             rc;

  new[0].name     = ATTR_NODE_state;
  new[0].resource = NULL;
  new[0].value    = state1;
  new[0].op       = op1;

  if (state2 == NULL) 
    {
    new[0].next     = NULL;
    } 
  else 
    {
    new[0].next     = &new[1];
    new[1].next     = NULL;
    new[1].name     = ATTR_NODE_state;
    new[1].resource = NULL;
    new[1].value    = state2;
    new[1].op	    = op2;
    }

  rc = pbs_manager(
    con, 
    MGR_CMD_SET, 
    MGR_OBJ_NODE, 
    name, 
    new, 
    NULL);

  if (rc && !quiet) 
    {
    fprintf(stderr,"Error marking node %s - ", 
      name);

    if ((errmsg = pbs_geterrmsg(con)) != NULL)
      fprintf(stderr,"%s\n", 
        errmsg);
    else
      fprintf(stderr,"error: %d\n",
        pbs_errno);
    }

  return(rc);
  }  /* END marknode() */





int main(

  int	 argc,
  char **argv)

  {
  struct batch_status *bstatus = NULL;
  int	 con;
  char	*def_server;
  int	 errflg = 0;
  char	*errmsg;
  int	 i;
  extern char	*optarg;
  extern int	 optind;
  char	       **pa;
  struct batch_status *pbstat;
  int	flag = DOWN;

  /* get default server, may be changed by -s option */

  def_server = pbs_default();

  if (def_server == NULL)
    def_server = "";

  while ((i = getopt(argc,argv,"acdlopqrs:x-:")) != EOF)
    {
    switch(i) 
      {
      case 'a':

        flag = ALLI;
 
        break;

      case 'c':

        flag = CLEAR;

        break;

      case 'd':

        flag = DIAG;

        break;

      case 'l':

        flag = LIST;

        break;

      case 'o':

        flag = OFFLINE;

        break;

      case 'p':

        flag = PURGE;

        break;

      case 'q':

        quiet = 1;

        break;

      case 'r':

        flag = RESET;

        break;

      case 's':
   
        def_server = optarg;

        break;

      case 'x':

        flag = ALLI;

        DisplayXML = TRUE;

        break;

      case '-':

        if ((optarg != NULL) && !strcmp(optarg,"version"))
          {
          fprintf(stderr,"version: %s\n",
            PBS_VERSION);

          exit(0);
          }

        errflg = 1;

        break;

      case '?':
      default:

        errflg = 1;

        break;
      }  /* END switch (i) */
    }    /* END while (i == getopt()) */

  if ((errflg != 0) || ((flag == LIST) && (optind != argc))) 
    {
    if (!quiet)
      {
      fprintf(stderr,"usage:\t%s [-{c|d|o|p|r}][-s server] [-q] node node ...\n",
        argv[0]);

      fprintf(stderr,"\t%s -l [-s server] [-q]\n",
        argv[0]);

      fprintf(stderr,"\t%s -{a|x} [-s server] [-q] [node]\n",
        argv[0]);
      }

    exit(1);
    }

  con = cnt2server(def_server);

  if (con <= 0) 
    {
    if (!quiet)
      {
      fprintf(stderr, "%s: cannot connect to server %s, error=%d\n",
        argv[0],           
        def_server, 
        pbs_errno);
      }

    exit(1);
    }

  /* if flag is ALLI, DOWN or LIST, get status of all nodes */

  if ((flag == ALLI) || (flag == DOWN) || (flag == LIST) || (flag == DIAG)) 
    {
    if ((flag == ALLI) || (flag == DIAG))
      {
      /* allow node specification */

      if (argv[optind] != NULL)
        bstatus = pbs_statnode(con,argv[optind],NULL,NULL);
      else
        bstatus = pbs_statnode(con,"",NULL,NULL);
      }
    else
      {
      /* node specification not allowed */

      bstatus = pbs_statnode(con,"",NULL,NULL);
      }

    if (bstatus == NULL) 
      {
      if (pbs_errno) 
        {
        if (!quiet) 
          {
          if ((errmsg = pbs_geterrmsg(con)) != NULL)
            {
            fprintf(stderr,"%s: %s\n",
              argv[0],
              errmsg);
            }
          else
            {
            fprintf(stderr,"%s: Error %d\n",
              argv[0],
              pbs_errno);
            }
          }

        exit(1);
        }
 
      if (!quiet)
        fprintf(stderr,"%s: No nodes found\n", 
          argv[0]);

      exit(0);
      }
    }    /* END if ((flag == ALLI) || (flag == DOWN) || (flag == LIST) || (flag == DIAG)) */

  switch (flag) 
    {
    case DIAG:

      /* NYI */

      break;

    case DOWN:

      /*
       * loop through the list of nodes returned above:
       *   if node is up and is in argv list, mark it down;
       *   if node is down and not in argv list, mark it up;
       * for all changed nodes, send in request to server
       */

      for (pbstat = bstatus;pbstat != NULL;pbstat = pbstat->next) 
        {
        for (pa = argv + optind;*pa;pa++) 
          {
          if (cmp_node_name(*pa,pbstat->name) == 0) 
            {
            if (is_down(pbstat) == 0) 
              {
              marknode(
                con, 
                pbstat->name,
                ND_down, 
                INCR, 
                NULL, 
                INCR);

              break;
              }
            }
          }

        if (*pa == NULL) 
          {
          /* node not in list, if down now, set up */

          if (is_down(pbstat) == 1)
            {
            marknode(con,pbstat->name, 
              ND_down,DECR,NULL,DECR);
            }
          }
        }

      break;

    case CLEAR:

      /* clear  OFFLINE from specified nodes */

      for (pa = argv + optind;*pa;pa++) 
        {
        marknode(con,*pa,ND_offline,DECR,NULL,DECR);	
        }

      break;

    case RESET:

      /* clear OFFLINE, add DOWN to specified nodes */

      for (pa = argv + optind;*pa;pa++) 
        {
        marknode(con,*pa,ND_offline,DECR,ND_down,INCR);	
        }

      break;

    case OFFLINE:

      /* set OFFLINE on specified nodes */

      for (pa = argv + optind;*pa;pa++) 
        {
        marknode(con,*pa,ND_offline,INCR,NULL,INCR);	
        }

      break;

    case PURGE:

      /* remove node record */

      /* NYI */

      break;

    case ALLI:

      if (DisplayXML == TRUE)
        {
        struct attrl *pat;

        char tmpBuf[400000];

        mxml_t *DE;
        mxml_t *NE;
        mxml_t *AE;

        DE = NULL;

        MXMLCreateE(&DE,"Data");

        for (pbstat = bstatus;pbstat;pbstat = pbstat->next)
          {
          NE = NULL;

          MXMLCreateE(&NE,"Node");

          MXMLAddE(DE,NE);

          for (pat = pbstat->attribs;pat;pat = pat->next)
            {
            AE = NULL;

            if (pat->value == NULL)
              continue;

            MXMLCreateE(&AE,pat->name);

            MXMLSetVal(AE,pat->value);

            MXMLAddE(NE,AE);
            }
          }    /* END for (pbstat) */

        MXMLToString(DE,tmpBuf,sizeof(tmpBuf),NULL,TRUE);

        MXMLDestroyE(&DE);

        fprintf(stdout,"%s\n",
          tmpBuf);
        }
      else
        {
        for (pbstat = bstatus;pbstat;pbstat = pbstat->next) 
          {
          printf("%s\n", 
            pbstat->name);
            prt_node_attr(pbstat);

          putchar('\n');
          }
        }

      break;

    case LIST:

      /* list any node that is DOWN, OFFLINE, or UNKNOWN */

      for (pbstat = bstatus; pbstat; pbstat = pbstat->next) 
        {
        if (is_down(pbstat) || is_offline(pbstat) || is_unknown(pbstat)) 
          {
          printf("%-20.20s %s\n", 
            pbstat->name,
            get_nstate(pbstat));
          }
        }

      break;
    }  /* END switch (flag) */

  pbs_disconnect(con);
  
  return(0);
  }  /* END main() */

/* END pbsnodes.c */

