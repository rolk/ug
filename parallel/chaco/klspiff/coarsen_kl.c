// NOTE: The current revision of this file was left untouched when the DUNE source files were reindented!
// NOTE: It contained invalid syntax that could not be processed by uncrustify.

/* This software was developed by Bruce Hendrickson and Robert Leland   *
 * at Sandia National Laboratories under US Department of Energy        *
 * contract DE-AC04-76DP00789 and is copyrighted by Sandia Corporation. */

#include "config.h"
#include <stdio.h>
#include "../main/params.h"
#include "../main/defs.h"
#include "../main/structs.h"


void coarsen_kl(graph, nvtxs, nedges, ndims, nstep, step, vmax, eigtol,
           assignment, nsets, hops, mediantype, mkconnected, using_ewgts,
	   using_vwgts, solver_flag, goal, vwgt_max, give_up, part_type)
/* Coarsen until nvtxs < vmax, compute and uncoarsen. */
struct vtx_data **graph;	/* array of vtx data for graph */
int nvtxs;			/* number of vertices in graph */
int nedges;			/* number of edges in graph */
int ndims;			/* number of eigenvectors to calculate */
int nstep;			/* number of coarsenings between RQI steps */
int step;			/* current step number */
int vmax;			/* largest subgraph to stop coarsening */
double eigtol;			/* tolerence in eigen calculation */
short *assignment;		/* processor each vertex gets assigned to */
int nsets;			/* number of sets being divided into */
short (*hops)[MAXSETS];		/* cost of edge between sets */
int mediantype;			/* flag for different assignment strategies */
int using_ewgts;		/* are edge weights being used? */
int using_vwgts;		/* are vertices weights being used? */
int solver_flag;		/* which eigensolver to use */
double *goal;			/* desired set sizes */
int vwgt_max;			/* largest vertex weight */
int give_up;			/* has coarsening bogged down? */
int part_type;
{
   extern Heap   *heap;     /* pointer to heap of multigrid */
   extern double *MEM_OK;   /* variable for memory overflow exeception */
   extern int DEBUG_TRACE;	/* trace the execution of the code */
   extern int DEBUG_COARSEN;	/* debug flag for coarsening */
   extern int DEBUG_CONNECTED;	/* debug flag for connectivity checking */
   extern float COARSEN_RATIO_MIN;	/* vtx reduction demanded */
   struct connect_data *cdata;	/* data structure for enforcing connectivity */
   struct vtx_data **cgraph;	/* array of vtx data for coarsened graph */
   double *yvecs[MAXDIMS+1];	/* eigenvectors for subgraph */
   double evals[MAXDIMS+1];	/* eigenvalues returned */
   double *vwsqrt = NULL;	/* square root of vertex weights */
   double maxdeg;		/* maximum weighted degree of a vertex */
   int *active;			/* space for assign routine */
   int *v2cv;			/* mapping from fine to coarse vertices */
   short *cassignment;		/* set assignments for coarsened vertices */
   int cnvtxs;			/* number of vertices in coarsened graph */
   int cnedges;			/* number of edges in coarsened graph */
   int cvwgt_max;		/* largest vertex weight in coarsened graph */
   int nextstep;		/* next step in RQI test */
   int i;			/* loop counters */
   
   double find_maxdeg();
   void makevwsqrt(), make_connected(), print_connected(), eigensolve();
   void make_unconnected(), assign(), klspiff(), coarsen1_kl(), free_graph();

   if (DEBUG_COARSEN > 0 || DEBUG_TRACE > 0) {
     {char buf[150]; sprintf(buf," Entering coarsen_kl, step=%d, nvtxs=%d, nedges=%d, vmax=%d\n", step, nvtxs, nedges, vmax);UserWrite(buf);}
   }

   /* Is problem small enough to solve? */
   if (nvtxs <= vmax || give_up) {
      if (using_vwgts) {
         vwsqrt = (double *) (MEM_OK = smalloc((unsigned) (nvtxs+1)*sizeof(double));
         if (!MEM_OK) return;
	 makevwsqrt(vwsqrt, graph, nvtxs);
      }
      else vwsqrt = NULL;

      /* Create space for subgraph yvecs. */
      for (i=1; i<=ndims; i++) {
         yvecs[i] = (double *) (MEM_OK = smalloc((unsigned) (nvtxs+1)*sizeof(double));
         if (!MEM_OK) return;
      }

# ifdef __DEBUG__
/* debug_graph */
{
extern int DEBUG_GRAPH;

    /* debug_graph */
    if (DEBUG_GRAPH>0)
    {
		int flag;
        char buf[100];

        flag = check(graph,nvtxs,nedges);
        if (flag == FALSE)
        {
            sprintf(buf,"FATAL: check_graph returned FALSE in coarsen_kl() before mk_cnt\n");
            UserWrite(buf);
            sprintf(buf,"nvtxs=%d,nedges=%d,ndims=%d,step=%d\n",nvtxs,nedges,ndims,step);
            UserWrite(buf);
        }
        else
        {
            sprintf(buf,"OK: check_graph = TRUE in coarsen_kl() before mk_cnt\n");
            UserWrite(buf);
            sprintf(buf,"nvtxs=%d,nedges=%d,ndims=%d,step=%d\n",nvtxs,nedges,ndims,step);
            UserWrite(buf);
        }

    }
}
# endif


      active = (int *) (MEM_OK = smalloc((unsigned) nvtxs*sizeof(int));
      if (!MEM_OK) return;
      if (mkconnected) {
	 make_connected(graph, nvtxs, &nedges, (short *) &(yvecs[1][0]), 
			active, &cdata);
     if (!MEM_OK) return;
	 if (DEBUG_CONNECTED > 0) {
	   {char buf[150]; sprintf(buf,"Enforcing connectivity on coarse graph\n");UserWrite(buf);}
	    print_connected(cdata);
	 }
      }

# ifdef __DEBUG__
/* debug_graph */
{
extern int DEBUG_GRAPH;

    /* debug_graph */
    if (DEBUG_GRAPH>0)
    {
		int flag;
        char buf[100];

        flag = check(graph,nvtxs,nedges);
        if (flag == FALSE)
        {
            sprintf(buf,"FATAL: check_graph returned FALSE in coarsen_kl after mk_cnt\n");
            UserWrite(buf);
            sprintf(buf,"nvtxs=%d,nedges=%d,ndims=%d,step=%d\n",nvtxs,nedges,ndims,step);
            UserWrite(buf);
        }
        else
        {
            sprintf(buf,"OK: check_graph = TRUE in coarsen_kl() after mk_cnt\n");
            UserWrite(buf);
            sprintf(buf,"nvtxs=%d,nedges=%d,ndims=%d,step=%d\n",nvtxs,nedges,ndims,step);
            UserWrite(buf);
        }
    }
}
# endif


      /* Partition coarse graph with spectral method */
      maxdeg = find_maxdeg(graph, nvtxs);
      eigensolve(graph, nvtxs, nedges, maxdeg, yvecs, evals, ndims, eigtol,
		 vwsqrt, solver_flag, FALSE, 0, 0, (float **) NULL, using_ewgts,
		using_vwgts);
      if (!MEM_OK) return;

# ifdef __DEBUG__
/* debug_graph */
{
extern int DEBUG_GRAPH;

    /* debug_graph */
    if (DEBUG_GRAPH>0)
    {
        int flag;
        char buf[100];

        flag = check(graph,nvtxs,nedges);
        if (flag == FALSE)
        {
            sprintf(buf,"FATAL: check_graph returned FALSE in coarsen_kl after eigensolve\n");
            UserWrite(buf);
            sprintf(buf,"nvtxs=%d,nedges=%d,ndims=%d,step=%d\n",nvtxs,nedges,ndims,step);
            UserWrite(buf);
        }
        else
        {
            sprintf(buf,"OK: check_graph = TRUE in coarsen_kl() after eigensolve\n");
            UserWrite(buf);
            sprintf(buf,"nvtxs=%d,nedges=%d,ndims=%d,step=%d\n",nvtxs,nedges,ndims,step);
            UserWrite(buf);
        }
    }
}
# endif

      if (mkconnected) make_unconnected(graph, &nedges, &cdata);

#ifdef __DEBUG__
/* debug_graph */
{
extern int DEBUG_GRAPH;

    /* debug_graph */
    if (DEBUG_GRAPH>0)
    {
        int flag;
        char buf[100];

        flag = check(graph,nvtxs,nedges);
        if (flag == FALSE)
        {
            sprintf(buf,"FATAL: check_graph returned FALSE in coarsen_kl after un_cnt\n");
            UserWrite(buf);
            sprintf(buf,"nvtxs=%d,nedges=%d,ndims=%d,step=%d\n",nvtxs,nedges,ndims,step);
            UserWrite(buf);
        }
        else
        {
            sprintf(buf,"OK: check_graph = TRUE in coarsen_kl() after un_cnt\n");
            UserWrite(buf);
            sprintf(buf,"nvtxs=%d,nedges=%d,ndims=%d,step=%d\n",nvtxs,nedges,ndims,step);
            UserWrite(buf);
        }
    }
}
# endif 

      assign(graph, yvecs, nvtxs, ndims, vwsqrt, assignment, active,
	     mediantype, goal, vwgt_max, part_type);
      if (!MEM_OK) return;
      sfree((char *) active);

# ifdef __DEBUG__
/* debug_graph */
{
extern int DEBUG_GRAPH;

    /* debug_graph */
    if (DEBUG_GRAPH>0)
    {
        int flag;
        char buf[100];

        flag = check(graph,nvtxs,nedges);
        if (flag == FALSE)
        {
            sprintf(buf,"FATAL: check_graph returned FALSE in coarsen_kl after assign\n");
            UserWrite(buf);
            sprintf(buf,"nvtxs=%d,nedges=%d,ndims=%d,step=%d\n",nvtxs,nedges,ndims,step);
            UserWrite(buf);
        }
        else
        {
            sprintf(buf,"OK: check_graph = TRUE in coarsen_kl() after assign\n");
            UserWrite(buf);
            sprintf(buf,"nvtxs=%d,nedges=%d,ndims=%d,step=%d\n",nvtxs,nedges,ndims,step);
            UserWrite(buf);
        }
    }
}
# endif 


      if (step == 0 || step == nstep) {
         klspiff(graph, nvtxs, assignment, nsets, hops, goal, vwgt_max,
		 maxdeg);
         if (!MEM_OK) return;
      }
      /*sfree((char *) ewgts);*/

      if (vwsqrt != NULL) sfree((char *) vwsqrt);
      for (i=ndims; i>0; i--) sfree((char *) yvecs[i]);
      return;
   }

#ifdef __DEBUG__
/* debug_graph */
{
extern int DEBUG_GRAPH;

    /* debug_graph */
    if (DEBUG_GRAPH>0)
    {
		int flag;
        char buf[100];

        flag = check(graph,nvtxs,nedges);
        if (flag == FALSE)
        {
            sprintf(buf,"FATAL: check_graph returned FALSE in coarsen_kl() before coarsen_1\n");
            UserWrite(buf);
            sprintf(buf,"nvtxs=%d,nedges=%d,ndims=%d,step=%d\n",nvtxs,nedges,ndims,step);
            UserWrite(buf);
        }
        else
        {
            sprintf(buf,"OK: check_graph = TRUE in coarsen_kl() before coarsen_1\n");
            UserWrite(buf);
            sprintf(buf,"nvtxs=%d,nedges=%d,ndims=%d,step=%d\n",nvtxs,nedges,ndims,step);
            UserWrite(buf);
        }
    }
}
# endif

   /* Otherwise I have to coarsen. */
   coarsen1_kl(graph, nvtxs, nedges, &cgraph, &cnvtxs, &cnedges,
	       &v2cv, 0, (float **) NULL, (float **) NULL, using_ewgts);
   if (!MEM_OK) return;

   /* If coarsening isn't working very well, give up and partition. */
   give_up = FALSE;
   if (nvtxs*COARSEN_RATIO_MIN < cnvtxs) {
     {char buf[150]; sprintf(buf,"WARNING: Coarsening not making enough progress, nvtxs = %d, cnvtxs = %d.\n", nvtxs, cnvtxs);UserWrite(buf);}
     {char buf[150]; sprintf(buf,"         Recursive coarsening being stopped prematurely.\n");UserWrite(buf);}
      give_up = TRUE;
   }

# ifdef __DEBUG__
/* debug_graph */
{
extern int DEBUG_GRAPH;

    /* debug_graph */
    if (DEBUG_GRAPH>0)
    {
		int flag;
        char buf[100];

        flag = check(graph,nvtxs,nedges);
        if (flag == FALSE)
        {
            sprintf(buf,"FATAL: check_graph returned FALSE in coarsen_kl after coarsen_1\n");
            UserWrite(buf);
            sprintf(buf,"nvtxs=%d,nedges=%d,ndims=%d,step=%d\n",nvtxs,nedges,ndims,step);
            UserWrite(buf);
        }
        else
        {
            sprintf(buf,"OK: check_graph = TRUE in coarsen_kl() after coarsen_1\n");
            UserWrite(buf);
            sprintf(buf,"nvtxs=%d,nedges=%d,ndims=%d,step=%d\n",nvtxs,nedges,ndims,step);
            UserWrite(buf);
        }
    }
}
# endif 


   /* Now recurse on coarse subgraph. */
   cvwgt_max = 0;
   for (i=1; i<=cnvtxs; i++) {
      if (cgraph[i]->vwgt > cvwgt_max) cvwgt_max = cgraph[i]->vwgt;
   }
   cassignment = (short *) (MEM_OK = smalloc((unsigned) (cnvtxs+1)*sizeof(short));
   if (!MEM_OK) return;
   nextstep = step - 1;
   if (step <= 0) nextstep = nstep - 1; 
   coarsen_kl(cgraph, cnvtxs, cnedges, ndims, nstep, nextstep, vmax, eigtol,
	      cassignment, nsets, hops, mediantype, mkconnected, TRUE, TRUE,
	      solver_flag, goal, cvwgt_max, give_up, part_type);
   if (!MEM_OK) return;

   /* Interpolate assignment back to fine graph. */
   for (i=1; i<=nvtxs; i++) assignment[i] = cassignment[v2cv[i]];

   /* Smooth using KL every nstep steps. */
   if (step == 0 || step == nstep) {
      maxdeg = find_maxdeg(graph, nvtxs);
      klspiff(graph, nvtxs, assignment, nsets, hops, goal, vwgt_max,
	      maxdeg);
      if (!MEM_OK) return;
   }

   if (DEBUG_COARSEN > 0) {
     {char buf[150]; sprintf(buf," Leaving coarsen_kl, step=%d\n", step);UserWrite(buf);}
   }

   /* Free the space that was allocated. */
   sfree((char *) cassignment);
   free_graph(cgraph);
   sfree((char *) v2cv);
}
