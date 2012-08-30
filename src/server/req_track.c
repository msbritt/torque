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
 * req_track.c
 *
 * Functions relation to the Track Job Request and job tracking.
 */
#include <pbs_config.h>   /* the master config generated by configure */

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include "libpbs.h"
#include <fcntl.h>
#include <signal.h>
#include "server_limits.h"
#include "list_link.h"
#include "attribute.h"
#include "server.h"
#include "credential.h"
#include "batch_request.h"
#include "pbs_job.h"
#include "pbs_error.h"
#include "work_task.h"
#include "tracking.h"
#include "svrfunc.h"
#include "log.h"
#include "../lib/Liblog/pbs_log.h"
#include "../lib/Liblog/log_event.h"

/* External functions */

int issue_to_svr(char *svr, struct batch_request *, void (*func)(struct work_task *));

/* Global Data Items: */

extern char         *path_track;

extern struct server server;
extern char          server_name[];
extern int           LOGLEVEL;

/*
 * req_track - record job tracking information
 */

int req_track(

  void *vp)

  {
  struct batch_request *preq = (struct batch_request *)vp;
  struct tracking      *empty = (struct tracking *)0;
  int                   i;
  int                   need;
  time_t                time_now = time(NULL);

  struct tracking      *new;
  struct tracking      *ptk;
  struct rq_track      *prqt;

  /*  make sure request is from a server */

  if (!preq->rq_fromsvr)
    {
    req_reject(PBSE_IVALREQ, 0, preq, NULL, NULL);

    return(PBSE_NONE);
    }

  /* attempt to locate tracking record for this job    */
  /* also remember first empty slot in case its needed */

  prqt = &preq->rq_ind.rq_track;

  ptk = server.sv_track;

  for (i = 0;i < server.sv_tracksize;i++)
    {
    if ((ptk + i)->tk_mtime)
      {
      if (!strcmp((ptk + i)->tk_jobid, prqt->rq_jid))
        {
        /* found record, discard it if state == exiting,
         * otherwise, update it if older */

        if (*prqt->rq_state == 'E')
          {
          (ptk + i)->tk_mtime = 0;
          }
        else if ((ptk + i)->tk_hopcount < prqt->rq_hopcount)
          {
          (ptk + i)->tk_hopcount = prqt->rq_hopcount;

          strcpy((ptk + i)->tk_location, prqt->rq_location);

          (ptk + i)->tk_state = *prqt->rq_state;

          (ptk + i)->tk_mtime = time_now;
          }

        server.sv_trackmodifed = 1;

        reply_ack(preq);

        return(PBSE_NONE);
        }
      }
    else if (empty == NULL)
      {
      empty = ptk + i;
      }
    }

  /* if we got here, didn't find it... */

  if (*prqt->rq_state != 'E')
    {
    /* and need to add it */

    if (empty == NULL)
      {
      /* need to make room for more */

      need = server.sv_tracksize * 3 / 2;

      new = calloc(need, sizeof(struct tracking));

      if (new == NULL)
        {
        /* FAILURE */

        log_err(errno, "req_track", "calloc failed");

        req_reject(PBSE_SYSTEM, 0, preq, NULL, NULL);

        return(PBSE_NONE);
        }

      memcpy(new, server.sv_track, server.sv_tracksize);

      empty = new + server.sv_tracksize; /* first new slot */

      for (i = server.sv_tracksize;i < need;i++)
        (new + i)->tk_mtime = 0;

      server.sv_tracksize = need;
      free(server.sv_track);
      server.sv_track     = new;
      }

    empty->tk_mtime = time_now;

    empty->tk_hopcount = prqt->rq_hopcount;
    strcpy(empty->tk_jobid, prqt->rq_jid);
    strcpy(empty->tk_location, prqt->rq_location);
    empty->tk_state = *prqt->rq_state;
    server.sv_trackmodifed = 1;

    if (LOGLEVEL >= 7)
      {
      log_event(
        PBSEVENT_JOB,
        PBS_EVENTCLASS_JOB,
        prqt->rq_jid,
        "job added to server tracking list");
      }
    }

  reply_ack(preq);

  return(PBSE_NONE);
  } /* END req_track() */




/*
 * track_save - save the tracking records to a file
 *
 * This routine is invoked periodically by a timed work task entry.
 * The first entry is created at server initialization time and then
 * recreated on each entry.
 *
 * On server shutdown, track_save is called with a null work task pointer.
 */

void track_save(

  struct work_task *pwt)  /* unused */

  {
  int        fd;
  char      *myid = "save_track";
  time_t     time_now = time(NULL);
  work_task *wt;

  /* set task for next round trip */

  if (pwt)    /* set up another work task for next time period */
    {
    free(pwt->wt_mutex);
    free(pwt);

    wt = set_task(WORK_Timed, (long)time_now + PBS_SAVE_TRACK_TM, track_save, NULL, FALSE);

    if (wt == NULL)
      log_err(errno, myid, "Unable to set task for save");
    }

  if (server.sv_trackmodifed == 0)
    return;   /* nothing to do this time */

  fd = open(path_track, O_WRONLY, 0);

  if (fd < 0)
    {
    log_err(errno, myid, "Unable to open tracking file");
    return;
    }

  if (write(fd, (char *)server.sv_track, server.sv_tracksize * sizeof(struct tracking)) !=
      (ssize_t)(server.sv_tracksize * sizeof(struct tracking)))
    {
    log_err(errno, myid, "failed to write to track file");
    }

  if (close(fd) < 0)
    {
    log_err(errno, myid, "failed to close track file after saving");

    return;
    }

  server.sv_trackmodifed = 0;

  return;
  }




/*
 * issue_track - issue a Track Job Request to another server
 */

void issue_track(
    
  job *pjob)

  {

  struct batch_request   *preq;
  char         *pc;

  preq = alloc_br(PBS_BATCH_TrackJob);

  if (preq == (struct batch_request *)0)
    return;

  preq->rq_ind.rq_track.rq_hopcount = pjob->ji_wattr[JOB_ATR_hopcount].at_val.at_long;

  strcpy(preq->rq_ind.rq_track.rq_jid, pjob->ji_qs.ji_jobid);
  strcpy(preq->rq_ind.rq_track.rq_location, server_name);

  preq->rq_ind.rq_track.rq_state[0] = pjob->ji_wattr[JOB_ATR_state].at_val.at_char;

  pc = pjob->ji_qs.ji_jobid;

  while (*pc != '.')
    pc++;

  issue_to_svr(++pc, preq, NULL);
  free_br(preq);
  }
