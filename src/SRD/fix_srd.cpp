/* ----------------------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   http://lammps.sandia.gov, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov
   
   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under 
   the GNU General Public License.
   
   See the README file in the top-level LAMMPS directory.
 ------------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
   Contributing authors: Jeremy Lechman (SNL), Pieter in 't Veld (BASF)
------------------------------------------------------------------------- */

#include "math.h"
#include "string.h"
#include "stdlib.h"
#include "fix_srd.h"
#include "atom.h"
#include "atom_vec.h"
#include "group.h"
#include "update.h"
#include "force.h"
#include "pair.h"
#include "domain.h"
#include "neighbor.h"
#include "comm.h"
#include "modify.h"
#include "fix_deform.h"
#include "random_mars.h"
#include "random_park.h"
#include "memory.h"
#include "error.h"

using namespace LAMMPS_NS;

enum{SLIP,NOSLIP};
enum{SPHERE,ELLIPSOID};
enum{SPHERE_SHAPE,SPHERE_RADIUS};
enum{ANGULAR_OMEGA,ANGULAR_ANGMOM};
enum{INSIDE_ERROR,INSIDE_WARN,INSIDE_IGNORE};
enum{BIG_MOVE,SRD_MOVE,SRD_ROTATE};
enum{CUBIC_ERROR,CUBIC_WARN};
enum{SHIFT_NO,SHIFT_YES,SHIFT_POSSIBLE};

enum{NO_REMAP,X_REMAP,V_REMAP};                   // same as fix_deform.cpp

#define INERTIA 0.4
#define ATOMPERBIN 10
#define BIG 1.0e20
#define VBINSIZE 5

#define MIN(A,B) ((A) < (B)) ? (A) : (B)
#define MAX(A,B) ((A) > (B)) ? (A) : (B)

//#define SRD_DEBUG 1
//#define SRD_DEBUG_ATOMID 4426
//#define SRD_DEBUG_TIMESTEP 1

/* ---------------------------------------------------------------------- */

FixSRD::FixSRD(LAMMPS *lmp, int narg, char **arg) : Fix(lmp, narg, arg)
{
  if (narg < 8) error->all("Illegal fix srd command");

  restart_pbc = 1;
  vector_flag = 1;
  size_vector = 12;
  global_freq = 1;
  extvector = 0;

  nevery = atoi(arg[3]);

  bigexist = 1;
  if (strcmp(arg[4],"NULL") == 0) bigexist = 0;
  else biggroup = group->find(arg[4]);

  temperature_srd = atof(arg[5]);
  gridsrd = atof(arg[6]);
  int seed = atoi(arg[7]);

  // parse options

  lamdaflag = 0;
  collidestyle = NOSLIP;
  overlap = 0;
  insideflag = INSIDE_ERROR;
  exactflag = 1;
  radfactor = 1.0;
  maxbounceallow = 0;
  gridsearch = gridsrd;
  cubicflag = CUBIC_ERROR;
  cubictol = 0.01;
  shiftuser = SHIFT_NO;
  shiftseed = 0;
  streamflag = 1;

  int iarg = 8;
  while (iarg < narg) {
    if (strcmp(arg[iarg],"lamda") == 0) {
      if (iarg+2 > narg) error->all("Illegal fix srd command");
      lamda = atof(arg[iarg+1]);
      lamdaflag = 1;
      iarg += 2;
    } else if (strcmp(arg[iarg],"collision") == 0) {
      if (iarg+2 > narg) error->all("Illegal fix srd command");
      if (strcmp(arg[iarg+1],"slip") == 0) collidestyle = SLIP;
      else if (strcmp(arg[iarg+1],"noslip") == 0) collidestyle = NOSLIP;
      else error->all("Illegal fix srd command");
      iarg += 2;
    } else if (strcmp(arg[iarg],"overlap") == 0) {
      if (iarg+2 > narg) error->all("Illegal fix srd command");
      if (strcmp(arg[iarg+1],"yes") == 0) overlap = 1;
      else if (strcmp(arg[iarg+1],"no") == 0) overlap = 0;
      else error->all("Illegal fix srd command");
      iarg += 2;
    } else if (strcmp(arg[iarg],"inside") == 0) {
      if (iarg+2 > narg) error->all("Illegal fix srd command");
      if (strcmp(arg[iarg+1],"error") == 0) insideflag = INSIDE_ERROR;
      else if (strcmp(arg[iarg+1],"warn") == 0) insideflag = INSIDE_WARN;
      else if (strcmp(arg[iarg+1],"ignore") == 0) insideflag = INSIDE_IGNORE;
      else error->all("Illegal fix srd command");
      iarg += 2;
    } else if (strcmp(arg[iarg],"exact") == 0) {
      if (iarg+2 > narg) error->all("Illegal fix srd command");
      if (strcmp(arg[iarg+1],"yes") == 0) exactflag = 1;
      else if (strcmp(arg[iarg+1],"no") == 0) exactflag = 0;
      else error->all("Illegal fix srd command");
      iarg += 2;
    } else if (strcmp(arg[iarg],"radius") == 0) {
      if (iarg+2 > narg) error->all("Illegal fix srd command");
      radfactor = atof(arg[iarg+1]);
      iarg += 2;
    } else if (strcmp(arg[iarg],"bounce") == 0) {
      if (iarg+2 > narg) error->all("Illegal fix srd command");
      maxbounceallow = atoi(arg[iarg+1]);
      iarg += 2;
    } else if (strcmp(arg[iarg],"search") == 0) {
      if (iarg+2 > narg) error->all("Illegal fix srd command");
      gridsearch = atof(arg[iarg+1]);
      iarg += 2;
    } else if (strcmp(arg[iarg],"cubic") == 0) {
      if (iarg+3 > narg) error->all("Illegal fix srd command");
      if (strcmp(arg[iarg+1],"error") == 0) cubicflag = CUBIC_ERROR;
      else if (strcmp(arg[iarg+1],"warn") == 0) cubicflag = CUBIC_WARN;
      else error->all("Illegal fix srd command");
      cubictol = atof(arg[iarg+2]);
      iarg += 3;
    } else if (strcmp(arg[iarg],"shift") == 0) {
      if (iarg+3 > narg) error->all("Illegal fix srd command");
      else if (strcmp(arg[iarg+1],"no") == 0) shiftuser = SHIFT_NO;
      else if (strcmp(arg[iarg+1],"yes") == 0) shiftuser = SHIFT_YES;
      else if (strcmp(arg[iarg+1],"possible") == 0) shiftuser = SHIFT_POSSIBLE;
      else error->all("Illegal fix srd command");
      shiftseed = atoi(arg[iarg+2]);
      iarg += 3;
    } else if (strcmp(arg[iarg],"stream") == 0) {
      if (iarg+2 > narg) error->all("Illegal fix srd command");
      if (strcmp(arg[iarg+1],"yes") == 0) streamflag = 1;
      else if (strcmp(arg[iarg+1],"no") == 0) streamflag = 0;
      else error->all("Illegal fix srd command");
      iarg += 2;
    } else error->all("Illegal fix srd command");
  }

  // error check

  if (nevery <= 0) error->all("Illegal fix srd command");
  if (bigexist && biggroup < 0) error->all("Could not find fix srd group ID");
  if (gridsrd <= 0.0) error->all("Illegal fix srd command");
  if (temperature_srd <= 0.0) error->all("Illegal fix srd command");
  if (seed <= 0) error->all("Illegal fix srd command");
  if (radfactor <= 0.0) error->all("Illegal fix srd command");
  if (maxbounceallow < 0) error->all("Illegal fix srd command");
  if (lamdaflag && lamda <= 0.0) error->all("Illegal fix srd command");
  if (gridsearch <= 0.0) error->all("Illegal fix srd command");
  if (cubictol < 0.0 || cubictol > 1.0) error->all("Illegal fix srd command");
  if ((shiftuser == SHIFT_YES || shiftuser == SHIFT_POSSIBLE) && 
      shiftseed <= 0) error->all("Illegal fix srd command");

  // initialize Marsaglia RNG with processor-unique seed

  me = comm->me;
  nprocs = comm->nprocs;

  random = new RanMars(lmp,seed + me);

  // if requested, initialize shift RNG, same on every proc

  if (shiftuser == SHIFT_YES || shiftuser == SHIFT_POSSIBLE)
    randomshift = new RanPark(lmp,shiftseed);
  else randomshift = NULL;

  // initialize data structs and flags

  if (bigexist) biggroupbit = group->bitmask[biggroup];
  else biggroupbit = 0;

  nmax = 0;
  binhead = NULL;
  maxbin1 = 0;
  binnext = NULL;
  maxbuf = 0;
  sbuf1 = sbuf2 = rbuf1 = rbuf2 = NULL;

  shifts[0].maxvbin = shifts[1].maxvbin = 0;
  shifts[0].vbin = shifts[1].vbin = NULL;

  shifts[0].maxbinsq = shifts[1].maxbinsq = 0;
  for (int ishift = 0; ishift < 2; ishift++)
    for (int iswap = 0; iswap < 6; iswap++)
      shifts[ishift].bcomm[iswap].sendlist = 
	shifts[ishift].bcomm[iswap].recvlist = NULL;

  maxbin2 = 0;
  nbinbig = NULL;
  binbig = NULL;
  binsrd = NULL;

  nstencil = maxstencil = 0;
  stencil = NULL;

  maxbig = 0;
  biglist = NULL;

  stats_flag = 1;
  for (int i = 0; i < size_vector; i++) stats_all[i] = 0.0;

  initflag = 0;

  srd_bin_temp = 0.0;
  srd_bin_count = 0;

  // fix parameters

  if (collidestyle == SLIP) comm_reverse = 3;
  else comm_reverse = 6;
  force_reneighbor = 1;
}

/* ---------------------------------------------------------------------- */

FixSRD::~FixSRD()
{
  delete random;
  delete randomshift;

  memory->sfree(binhead);
  memory->sfree(binnext);
  memory->sfree(sbuf1);
  memory->sfree(sbuf2);
  memory->sfree(rbuf1);
  memory->sfree(rbuf2);

  memory->sfree(shifts[0].vbin);
  memory->sfree(shifts[1].vbin);
  for (int ishift = 0; ishift < 2; ishift++)
    for (int iswap = 0; iswap < 6; iswap++) {
      memory->sfree(shifts[ishift].bcomm[iswap].sendlist);
      memory->sfree(shifts[ishift].bcomm[iswap].recvlist);
    }

  memory->sfree(nbinbig);
  memory->destroy_2d_int_array(binbig);
  memory->sfree(binsrd);
  memory->destroy_2d_int_array(stencil);
  memory->sfree(biglist);
}

/* ---------------------------------------------------------------------- */

int FixSRD::setmask()
{
  int mask = 0;
  mask |= PRE_NEIGHBOR;
  mask |= POST_FORCE;
  return mask;
}

/* ---------------------------------------------------------------------- */

void FixSRD::init()
{
  // error checks

  if (force->newton_pair == 0) error->all("Fix srd requires newton pair on");
  if (domain->nonperiodic != 0)
    error->all("Fix srd simulation box must be periodic");
  if (bigexist && comm->ghost_velocity == 0)
    error->all("Fix srd requires ghost atoms store velocity");

  if (bigexist && !atom->radius_flag && !atom->avec->shape_type)
    error->all("Fix SRD requires atom attribute radius or shape");
  if (bigexist && !atom->angmom_flag && !atom->omega_flag)
    error->all("Fix SRD requires atom attribute angmom or omega");
  if (bigexist && atom->angmom_flag && atom->omega_flag)
    error->all("Fix SRD cannot have both atom attributes angmom and omega");
  if (bigexist && collidestyle == NOSLIP && !atom->torque_flag)
    error->all("Fix SRD no-slip requires atom attribute torque");

  if (initflag && update->dt != dt_big)
    error->all("Cannot change timestep once fix srd is setup");

  // orthogonal vs triclinic simulation box
  // could be static or shearing box

  triclinic = domain->triclinic;

  // set change_flags if size or shape changes due to other fixes

  change_size = change_shape = 0;

  if (domain->box_change) {
    for (int i = 0; i < modify->nfix; i++)
      if (modify->fix[i]->box_change) {
	if (modify->fix[i]->box_change_size) change_size = 1;
	if (modify->fix[i]->box_change_shape) change_shape = 1;
	if (strcmp(modify->fix[i]->style,"deform") == 0) {
	  FixDeform *deform = (FixDeform *) modify->fix[i];
	  if (deform->box_change_shape && deform->remapflag != V_REMAP)
	    error->all("Using fix srd with inconsistent "
		       "fix deform remap option");
	}
      }
  }

  // parameterize based on current box volume

  dimension = domain->dimension;
  parameterize();

  // limit initial SRD velocities if necessary

  double **v = atom->v;
  int *mask = atom->mask;
  int nlocal = atom->nlocal;

  double vsq;
  nrescale = 0;

  for (int i = 0; i < nlocal; i++)
    if (mask[i] & groupbit) {
      vsq = v[i][0]*v[i][0] + v[i][1]*v[i][1] + v[i][2]*v[i][2];
      if (vsq > vmaxsq) nrescale++;
    }

  int all;
  MPI_Allreduce(&nrescale,&all,1,MPI_INT,MPI_SUM,world);
  if (me == 0) {
    if (screen)
      fprintf(screen,"  # of rescaled SRD velocities = %d\n",all);
    if (logfile)
      fprintf(logfile,"  # of rescaled SRD velocities = %d\n",all);
  }

  velocity_stats(igroup);
  if (bigexist) velocity_stats(biggroup);

  // zero per-run stats

  bouncemaxnum = 0;
  bouncemax = 0;
  reneighcount = 0;
  initflag = 1;

  next_reneighbor = -1;
}

/* ---------------------------------------------------------------------- */

void FixSRD::setup(int vflag)
{
  // triclinic scale factors
  // convert a real distance (perpendicular to box face) to a lamda distance

  double length0,length1,length2;
  if (triclinic) {
    double *h_inv = domain->h_inv;
    length0 = sqrt(h_inv[0]*h_inv[0] + h_inv[5]*h_inv[5] + h_inv[4]*h_inv[4]);
    length1 = sqrt(h_inv[1]*h_inv[1] + h_inv[3]*h_inv[3]);
    length2 = h_inv[2];
  }

  // dist_bigghost = distance from sub-domain at which big ghost
  //   can be due to moving away from sub-domain before next reneigh
  // dist_srd = distance from sub-domain at which an SRD might overlap
  //   a big ghost I don't know about due to big moving towards sub-domain
  // dist_srd_reneigh = distance from sub-domain at which an SRD triggers
  //   a reneigh, b/c next SRD move might overlap with unknown big ghost
  // when no big particles:
  //   set dist_bigghost and dist_srd to 0.0, since not used
  //   set dist_srd_reneigh to limit SRD particles can move
  //     without being lost in comm = subsize - (max dist moved in 1 timestep)
  //   subsize = perp distance between sub-domain faces (orthog or triclinic)

  double dist_srd,dist_srd_reneigh;

  double cut = MAX(neighbor->cutneighmax,comm->cutghostuser);

  if (bigexist) {
    dist_bigghost = cut + 0.5*neighbor->skin;
    dist_srd = cut - 0.5*neighbor->skin - 0.5*maxbigdiam;
    dist_srd_reneigh = dist_srd - dt_big*vmax;
  } else {
    dist_bigghost = dist_srd = 0.0;
    double subsize;
    if (triclinic == 0) {
      subsize = domain->prd[0]/comm->procgrid[0];
      subsize = MIN(subsize,domain->prd[1]/comm->procgrid[1]);
      if (dimension == 3) 
	subsize = MIN(subsize,domain->prd[2]/comm->procgrid[2]);
    } else {
      subsize = 1.0/comm->procgrid[0]/length0;
      subsize = MIN(subsize,1.0/comm->procgrid[1]/length1);
      if (dimension == 3) 
	subsize = MIN(subsize,1.0/comm->procgrid[2]/length2);
    }
    dist_srd_reneigh = subsize - dt_big*vmax;
  }

  if (dist_srd_reneigh < nevery*dt_big*vmax && me == 0)
    error->warning("Fix srd SRD moves may trigger frequent reneighboring");

  // lo/hi = bbox on this proc which SRD particles must stay inside
  // lo/hi reneigh = bbox on this proc outside of which SRDs trigger a reneigh
  // for triclinic, these bbox are in lamda units

  if (triclinic == 0) {
    srdlo[0] = domain->sublo[0] - dist_srd;
    srdhi[0] = domain->subhi[0] + dist_srd;
    srdlo[1] = domain->sublo[1] - dist_srd;
    srdhi[1] = domain->subhi[1] + dist_srd;
    srdlo[2] = domain->sublo[2] - dist_srd;
    srdhi[2] = domain->subhi[2] + dist_srd;

    srdlo_reneigh[0] = domain->sublo[0] - dist_srd_reneigh;
    srdhi_reneigh[0] = domain->subhi[0] + dist_srd_reneigh;
    srdlo_reneigh[1] = domain->sublo[1] - dist_srd_reneigh;
    srdhi_reneigh[1] = domain->subhi[1] + dist_srd_reneigh;
    srdlo_reneigh[2] = domain->sublo[2] - dist_srd_reneigh;
    srdhi_reneigh[2] = domain->subhi[2] + dist_srd_reneigh;

  } else {
    srdlo[0] = domain->sublo_lamda[0] - dist_srd*length0;
    srdhi[0] = domain->subhi_lamda[0] + dist_srd*length0;
    srdlo[1] = domain->sublo_lamda[1] - dist_srd*length1;
    srdhi[1] = domain->subhi_lamda[1] + dist_srd*length1;
    srdlo[2] = domain->sublo_lamda[2] - dist_srd*length2;
    srdhi[2] = domain->subhi_lamda[2] + dist_srd*length2;

    srdlo_reneigh[0] = domain->sublo_lamda[0] - dist_srd_reneigh*length0;
    srdhi_reneigh[0] = domain->subhi_lamda[0] + dist_srd_reneigh*length0;
    srdlo_reneigh[1] = domain->sublo_lamda[1] - dist_srd_reneigh*length1;
    srdhi_reneigh[1] = domain->subhi_lamda[1] + dist_srd_reneigh*length1;
    srdlo_reneigh[2] = domain->sublo_lamda[2] - dist_srd_reneigh*length2;
    srdhi_reneigh[2] = domain->subhi_lamda[2] + dist_srd_reneigh*length2;
  }

  // setup search bins and search stencil based on these distances

  if (bigexist) {
    setup_search_bins();
    setup_search_stencil();
  } else nbins2 = 0;

  // perform first bining of SRD and big particles
  // set reneighflag to turn off SRD rotation
  // don't do SRD rotation in setup, only during timestepping

  reneighflag = BIG_MOVE;
  pre_neighbor();
}

/* ----------------------------------------------------------------------
   assign SRD particles to bins
   assign big particles to all bins they overlap
------------------------------------------------------------------------- */

void FixSRD::pre_neighbor()
{
  int i,j,ix,iy,iz,jx,jy,jz,ibin,jbin;
  double rsq,cutbinsq;
  double xlamda[3];

  // grow SRD per-atom bin arrays if necessary

  if (atom->nlocal > nmax) {
    nmax = atom->nmax;
    memory->sfree(binsrd);
    memory->sfree(binnext);
    binsrd = (int *) memory->smalloc(nmax*sizeof(int),"fix/srd:binsrd");
    binnext = (int *) memory->smalloc(nmax*sizeof(int),"fix/srd:binnext");
  }

  // setup BIG info list with index ptrs to BIG particles in atom arrays
  // grow BIG info list if necessary

  int *mask = atom->mask;
  double **x = atom->x;
  int nlocal = atom->nlocal;
  int nall = nlocal + atom->nghost;
  int nfirst = nlocal;
  if (bigexist && biggroup == atom->firstgroup) nfirst = atom->nfirst;

  if (bigexist) {
    nbig = atom->nfirst + atom->nghost;
    if (nbig > maxbig) {
      maxbig = nbig;
      memory->sfree(biglist);
      biglist = (Big *) memory->smalloc(maxbig*sizeof(Big),"fix/srd:biglist");
    }

    nbig = 0;
    for (i = 0; i < atom->nfirst; i++)
      if (mask[i] & biggroupbit) biglist[nbig++].index = i;
    for (i = nlocal; i < nall; i++)
      if (mask[i] & biggroupbit) biglist[nbig++].index = i;

    big_static();
  }

  // if simulation box size changes, reset velocity bins
  // if big particles exist, reset search bins if box size or shape changes

  if (change_size) setup_velocity_bins();
  if ((change_size || change_shape) && bigexist) {
    setup_search_bins();
    setup_search_stencil();
  }

  // compute search bin for each owned & ghost big particle
  // zero out bin counters for big particles
  // if firstgroup is defined, only loop over first and ghost particles
  // for each big particle: loop over stencil to find bins that it overlaps

  if (bigexist) {
    for (i = 0; i < nbins2; i++) nbinbig[i] = 0;

    i = nbig = 0;
    while (i < nall) {
      ix = static_cast<int> ((x[i][0]-xblo2)*bininv2x);
      iy = static_cast<int> ((x[i][1]-yblo2)*bininv2y);
      iz = static_cast<int> ((x[i][2]-zblo2)*bininv2z);
      ibin = iz*nbin2y*nbin2x + iy*nbin2x + ix;

      if (ix < 0 || ix >= nbin2x || iy < 0 || iy >= nbin2y || 
	  iz < 0 || iz >= nbin2z)
	error->one("Fix SRD: bad search bin assignment");
      
      if (mask[i] & biggroupbit) {
	cutbinsq = biglist[nbig].cutbinsq;
	for (j = 0; j < nstencil; j++) {
	  jx = ix + stencil[j][0];
	  jy = iy + stencil[j][1];
	  jz = iz + stencil[j][2];
	  
	  if (jx < 0 || jx >= nbin2x || jy < 0 || jy >= nbin2y || 
	      jz < 0 || jz >= nbin2z) {
	    printf("Big particle %d %d %g %g %g\n",
		   atom->tag[i],i,x[i][0],x[i][1],x[i][2]);
	    printf("Bin indices: %d %d %d, %d %d %d, %d %d %d\n",
		   ix,iy,iz,jx,jy,jz,nbin2x,nbin2y,nbin2z);
	    error->one("Fix SRD: bad stencil bin for big particle");
	  }
	  rsq = point_bin_distance(x[i],jx,jy,jz);
	  if (rsq < cutbinsq) {
	    jbin = ibin + stencil[j][3];
	    if (nbinbig[jbin] == ATOMPERBIN)
	      error->one("Fix SRD: too many big particles in bin");
	    binbig[jbin][nbinbig[jbin]++] = nbig;
	  }
	}
	nbig++;
      }

      i++;
      if (i == nfirst) i = nlocal;
    }
  }

  // rotate SRD velocities on SRD timestep
  // done now since all SRDs are currently inside my sub-domain

  if (reneighflag == SRD_ROTATE) reset_velocities();

  // log stats if reneighboring occurred b/c SRDs moved too far

  if (reneighflag == SRD_MOVE) reneighcount++;
  reneighflag = BIG_MOVE;
}

/* ----------------------------------------------------------------------
   advect SRD particles and detect collisions between SRD and BIG particles
   when collision occurs, change x,v of SRD, force,torque of BIG particle
------------------------------------------------------------------------- */

void FixSRD::post_force(int vflag)
{
  int ix,iy,iz;
  double xlamda[3];

  // zero per-timestep stats

  stats_flag = 0;
  ncheck = ncollide = nbounce = ninside = nrescale = 0;

  // zero ghost forces & torques on BIG particles

  double **f = atom->f;
  double **torque = atom->torque;
  int nlocal = atom->nlocal;
  int nall = nlocal + atom->nghost;
  if (bigexist == 0) nall = 0;

  for (int i = nlocal; i < nall; i++)
    f[i][0] = f[i][1] = f[i][2] = 0.0;

  if (collidestyle == NOSLIP)
    for (int i = nlocal; i < nall; i++)
      torque[i][0] = torque[i][1] = torque[i][2] = 0.0;

  // advect SRD particles
  // assign to search bins if big particles exist

  int *mask = atom->mask;
  double **x = atom->x;
  double **v = atom->v;

  if (bigexist) {
    for (int i = 0; i < nlocal; i++)
      if (mask[i] & groupbit) {
	x[i][0] += dt_big*v[i][0];
	x[i][1] += dt_big*v[i][1];
	x[i][2] += dt_big*v[i][2];
	
	ix = static_cast<int> ((x[i][0]-xblo2)*bininv2x);
	iy = static_cast<int> ((x[i][1]-yblo2)*bininv2y);
	iz = static_cast<int> ((x[i][2]-zblo2)*bininv2z);
	binsrd[i] = iz*nbin2y*nbin2x + iy*nbin2x + ix;
	
	if (ix < 0 || ix >= nbin2x || iy < 0 || iy >= nbin2y || 
	    iz < 0 || iz >= nbin2z) {
	  printf("SRD particle %d on step %d\n",
		 atom->tag[i],update->ntimestep);
	  printf("v = %g %g %g\n",v[i][0],v[i][1],v[i][2]);
	  printf("x = %g %g %g\n",x[i][0],x[i][1],x[i][2]);
	  printf("ix,iy,iz nx,ny,nz = %d %d %d %d %d %d\n",
		 ix,iy,iz,nbin2x,nbin2y,nbin2z);
	  error->one("Fix SRD: bad bin assignment for SRD advection");
	}
      }

  } else {
    for (int i = 0; i < nlocal; i++)
      if (mask[i] & groupbit) {
	x[i][0] += dt_big*v[i][0];
	x[i][1] += dt_big*v[i][1];
	x[i][2] += dt_big*v[i][2];
      }
  }

  // detect collision of BIG particles with SRDs

  if (bigexist) {
    if (collidestyle == NOSLIP || any_ellipsoids) big_dynamic();
    if (overlap) collisions_multi();
    else collisions_single();
  }

  // reverse communicate forces & torques on BIG particles

  if (bigexist) {
    flocal = f;
    tlocal = torque;
    comm->reverse_comm_fix(this);
  }

  // if any SRD particle has moved too far, trigger reneigh on next step
  // for triclinic, perform check in lamda units

  int flag = 0;

  if (triclinic) domain->x2lamda(nlocal);
  for (int i = 0; i < nlocal; i++)
    if (mask[i] & groupbit) {
      if (x[i][0] < srdlo_reneigh[0] || x[i][0] > srdhi_reneigh[0] ||
	  x[i][1] < srdlo_reneigh[1] || x[i][1] > srdhi_reneigh[1] ||
	  x[i][2] < srdlo_reneigh[2] || x[i][2] > srdhi_reneigh[2]) flag = 1;
    }
  if (triclinic) domain->lamda2x(nlocal);

  int flagall;
  MPI_Allreduce(&flag,&flagall,1,MPI_INT,MPI_SUM,world);
  if (flagall) {
    next_reneighbor = update->ntimestep + 1;
    reneighflag = SRD_MOVE;
  }

  // if next timestep is SRD timestep, trigger reneigh

  if ((update->ntimestep+1) % nevery == 0) {
    next_reneighbor = update->ntimestep + 1;
    reneighflag = SRD_ROTATE;
  }
}

/* ----------------------------------------------------------------------
   reset SRD velocities
   may perform random shifting by 1/2 bin in each dimension
   called at pre-neighbor stage when all SRDs are now inside my sub-domain
   for triclinic, will set mean velocity to box deformation velocity
------------------------------------------------------------------------- */

void FixSRD::reset_velocities()
{
  int i,j,n,ix,iy,iz,ibin,axis,sign,irandom;
  double u[3],vave[3];
  double vx,vy,vz,vsq;
  double *vold,*vnew,*xlamda;
  double vstream[3];

  // if requested, perform a dynamic shift

  if (shiftflag) {
    double *boxlo;
    if (triclinic == 0) boxlo = domain->boxlo;
    else boxlo = domain->boxlo_lamda;
    shifts[1].corner[0] = boxlo[0] - binsize1x*randomshift->uniform();
    shifts[1].corner[1] = boxlo[1] - binsize1y*randomshift->uniform();
    if (dimension == 3)
      shifts[1].corner[2] = boxlo[2] - binsize1z*randomshift->uniform();
    else shifts[1].corner[2] = boxlo[2];
    setup_velocity_shift(1,1);
  }

  double *corner = shifts[shiftflag].corner;
  int *binlo = shifts[shiftflag].binlo;
  int *binhi = shifts[shiftflag].binhi;
  int nbins = shifts[shiftflag].nbins;
  int nbinx = shifts[shiftflag].nbinx;
  int nbiny = shifts[shiftflag].nbiny;
  BinAve *vbin = shifts[shiftflag].vbin;

  // binhead = 1st SRD particle in each bin
  // binnext = index of next particle in bin
  // bin assignment is done in lamda units for triclinic

  int *mask = atom->mask;
  double **x = atom->x;
  double **v = atom->v;
  int nlocal = atom->nlocal;

  if (triclinic) domain->x2lamda(nlocal);

  for (i = 0; i < nbins; i++) binhead[i] = -1;

  for (i = 0; i < nlocal; i++)
    if (mask[i] & groupbit) {
      ix = static_cast<int> ((x[i][0]-corner[0])*bininv1x);
      ix = MAX(ix,binlo[0]);
      ix = MIN(ix,binhi[0]);
      iy = static_cast<int> ((x[i][1]-corner[1])*bininv1y);
      iy = MAX(iy,binlo[1]);
      iy = MIN(iy,binhi[1]);
      iz = static_cast<int> ((x[i][2]-corner[2])*bininv1z);
      iz = MAX(iz,binlo[2]);
      iz = MIN(iz,binhi[2]);

      ibin = (iz-binlo[2])*nbiny*nbinx + (iy-binlo[1])*nbinx + (ix-binlo[0]);
      binnext[i] = binhead[ibin];
      binhead[ibin] = i;
    }

  if (triclinic) domain->lamda2x(nlocal);

  if (triclinic && streamflag) {
    double *h_rate = domain->h_rate;
    double *h_ratelo = domain->h_ratelo;
    for (i = 0; i < nlocal; i++)
      if (mask[i] & groupbit) {
	xlamda = x[i];
	v[i][0] -= h_rate[0]*xlamda[0] + h_rate[5]*xlamda[1] + 
	  h_rate[4]*xlamda[2] + h_ratelo[0];
	v[i][1] -= h_rate[1]*xlamda[1] + h_rate[3]*xlamda[2] + h_ratelo[1];
	v[i][2] -= h_rate[2]*xlamda[2] + h_ratelo[2];
      }
  }

  // for each bin I have particles contributing to:
  // compute ave v and v^2 of particles in that bin
  // if I own the bin, set its random value, else set to 0.0

  for (i = 0; i < nbins; i++) {
    n = 0;
    vave[0] = vave[1] = vave[2] = 0.0;
    for (j = binhead[i]; j >= 0; j = binnext[j]) {
      vx = v[j][0];
      vy = v[j][1];
      vz = v[j][2];
      vave[0] += vx;
      vave[1] += vy;
      vave[2] += vz;
      n++;
    }

    vbin[i].vave[0] = vave[0];
    vbin[i].vave[1] = vave[1];
    vbin[i].vave[2] = vave[2];
    vbin[i].n = n;
    if (vbin[i].owner) vbin[i].random = random->uniform();
    else vbin[i].random = 0.0;
  }

  // communicate bin info for bins which more than 1 proc contribute to

  if (shifts[shiftflag].commflag) vbin_comm(shiftflag);

  // for each bin I have particles contributing to:
  // reassign particle velocity by rotation around a random axis
  // accumulate T_srd for each bin I own
  // for triclinic, replace mean velocity with stream velocity

  srd_bin_temp = 0.0;
  srd_bin_count = 0;

  if (dimension == 2) axis = 2;

  for (i = 0; i < nbins; i++) {
    n = vbin[i].n;
    if (n == 0) continue;
    vold = vbin[i].vave;
    vold[0] /= n;
    vold[1] /= n;
    vold[2] /= n;

    // if (triclinic && streamflag) {
    // xlamda = vbin[i].xctr;
    // vstream[0] = h_rate[0]*xlamda[0] + h_rate[5]*xlamda[1] + 
    //	h_rate[4]*xlamda[2] + h_ratelo[0];
    // vstream[1] = h_rate[1]*xlamda[1] + h_rate[3]*xlamda[2] + h_ratelo[1];
    //  vstream[2] = h_rate[2]*xlamda[2] + h_ratelo[2];
    // vnew = vstream;
    //} else vnew = vold;

    vnew = vold;

    //printf("BIN %d %d %d: %g %g: %g %g\n",i,nbin1x,nbin1y,
    //	   vold[0],vold[1],vnew[0],vnew[1]);

    irandom = static_cast<int> (6.0*vbin[i].random);
    sign = irandom % 2;
    if (dimension == 3) axis = irandom / 2;

    vsq = 0.0;
    for (j = binhead[i]; j >= 0; j = binnext[j]) {
      if (axis == 0) {
	u[0] = v[j][0]-vold[0];
	u[1] = sign ? v[j][2]-vold[2] : vold[2]-v[j][2];
	u[2] = sign ? vold[1]-v[j][1] : v[j][1]-vold[1];
      } else if (axis == 1) {
	u[1] = v[j][1]-vold[1];
	u[0] = sign ? v[j][2]-vold[2] : vold[2]-v[j][2];
	u[2] = sign ? vold[0]-v[j][0] : v[j][0]-vold[0];
      } else {
	u[2] = v[j][2]-vold[2];
	u[1] = sign ? v[j][0]-vold[0] : vold[0]-v[j][0];
	u[0] = sign ? vold[1]-v[j][1] : v[j][1]-vold[1];
      }
      vsq += u[0]*u[0] + u[1]*u[1] + u[2]*u[2];
      v[j][0] = u[0] + vnew[0];
      v[j][1] = u[1] + vnew[1];
      v[j][2] = u[2] + vnew[2];
    }

    // sum partial contribution of my particles to T even if I don't own bin
    // but only count bin if I own it, so that bin is counted exactly once

    if (n > 1) {
      srd_bin_temp += vsq / (n-1);
      if (vbin[i].owner) srd_bin_count++;
    }
  }

  srd_bin_temp *= force->mvv2e * mass_srd / (dimension * force->boltz);

  if (triclinic && streamflag) {
    double *h_rate = domain->h_rate;
    double *h_ratelo = domain->h_ratelo;
    for (i = 0; i < nlocal; i++)
      if (mask[i] & groupbit) {
	xlamda = x[i];
	v[i][0] += h_rate[0]*xlamda[0] + h_rate[5]*xlamda[1] + 
	  h_rate[4]*xlamda[2] + h_ratelo[0];
	v[i][1] += h_rate[1]*xlamda[1] + h_rate[3]*xlamda[2] + h_ratelo[1];
	v[i][2] += h_rate[2]*xlamda[2] + h_ratelo[2];
      }
  }

  // rescale any too-large velocities 

  for (i = 0; i < nlocal; i++)
    if (mask[i] & groupbit) {
      vsq = v[i][0]*v[i][0] + v[i][1]*v[i][1] + v[i][2]*v[i][2];
      if (vsq > vmaxsq) nrescale++;
    }
}

/* ----------------------------------------------------------------------
   communicate summed particle info for bins that overlap 1 or more procs
------------------------------------------------------------------------- */

void FixSRD::vbin_comm(int ishift)
{
  BinComm *bcomm1,*bcomm2;
  MPI_Request request1,request2;
  MPI_Status status;
  
  // send/recv bins in both directions in each dimension
  // only send/recv if bins overlap proc domains on that boundary
  // don't send if nsend = 0
  // copy to self if sendproc = me
  // MPI send to another proc if sendproc != me
  // don't recv if nrecv = 0
  // copy from self if recvproc = me
  // MPI recv from another proc if recvproc != me
  
  BinAve *vbin = shifts[ishift].vbin;
  int *procgrid = comm->procgrid;

  int iswap = 0;
  for (int idim = 0; idim < dimension; idim++) {
    bcomm1 = &shifts[ishift].bcomm[iswap++];
    bcomm2 = &shifts[ishift].bcomm[iswap++];
    
    if (procgrid[idim] == 1) {
      if (bcomm1->nsend)
	vbin_pack(vbin,bcomm1->nsend,bcomm1->sendlist,sbuf1);
      if (bcomm2->nsend)
	vbin_pack(vbin,bcomm2->nsend,bcomm2->sendlist,sbuf2);
      if (bcomm1->nrecv)
	vbin_unpack(sbuf1,vbin,bcomm1->nrecv,bcomm1->recvlist);
      if (bcomm2->nrecv)
	vbin_unpack(sbuf2,vbin,bcomm2->nrecv,bcomm2->recvlist);

    } else {
      if (bcomm1->nrecv)
	MPI_Irecv(rbuf1,bcomm1->nrecv*VBINSIZE,MPI_DOUBLE,bcomm1->recvproc,0,
		  world,&request1);
      if (bcomm2->nrecv)
	MPI_Irecv(rbuf2,bcomm2->nrecv*VBINSIZE,MPI_DOUBLE,bcomm2->recvproc,0,
		  world,&request2);
      if (bcomm1->nsend) {
	vbin_pack(vbin,bcomm1->nsend,bcomm1->sendlist,sbuf1);
	MPI_Send(sbuf1,bcomm1->nsend*VBINSIZE,MPI_DOUBLE,
		 bcomm1->sendproc,0,world);
      }
      if (bcomm2->nsend) {
	vbin_pack(vbin,bcomm2->nsend,bcomm2->sendlist,sbuf2);
	MPI_Send(sbuf2,bcomm2->nsend*VBINSIZE,MPI_DOUBLE,
		 bcomm2->sendproc,0,world);
      }
      if (bcomm1->nrecv) {
	MPI_Wait(&request1,&status);
	vbin_unpack(rbuf1,vbin,bcomm1->nrecv,bcomm1->recvlist);
      }
      if (bcomm2->nrecv) {
	MPI_Wait(&request2,&status);
	vbin_unpack(rbuf2,vbin,bcomm2->nrecv,bcomm2->recvlist);
      }
    }
  }
}

/* ----------------------------------------------------------------------
   pack velocity bin data into a message buffer for sending
------------------------------------------------------------------------- */

void FixSRD::vbin_pack(BinAve *vbin, int n, int *list, double *buf)
{
  int j;
  int m = 0;
  for (int i = 0; i < n; i++) {
    j = list[i];
    buf[m++] = vbin[j].n;
    buf[m++] = vbin[j].vave[0];
    buf[m++] = vbin[j].vave[1];
    buf[m++] = vbin[j].vave[2];
    buf[m++] = vbin[j].random;
  }
}

/* ----------------------------------------------------------------------
   unpack velocity bin data from a message buffer and sum values to my bins
------------------------------------------------------------------------- */

void FixSRD::vbin_unpack(double *buf, BinAve *vbin, int n, int *list)
{
  int j;
  int m = 0;
  for (int i = 0; i < n; i++) {
    j = list[i];
    vbin[j].n += static_cast<int> (buf[m++]);
    vbin[j].vave[0] += buf[m++];
    vbin[j].vave[1] += buf[m++];
    vbin[j].vave[2] += buf[m++];
    vbin[j].random += buf[m++];
  }
}

/* ----------------------------------------------------------------------
   detect all collisions between SRD and big particles
   assume SRD can be inside at most one big particle at a time
   unoverlap SRDs for each collision
------------------------------------------------------------------------- */

void FixSRD::collisions_single()
{
  int i,j,k,m,type,nbig,ibin,ibounce,inside,collide_flag;
  double dt,t_remain;
  double norm[3],xscoll[3],xbcoll[3],vsnew[3];
  Big *big;

  // outer loop over SRD particles
  // inner loop over big particles that overlap SRD particle bin
  // if overlap between SRD and big particle:
  //   for exact, compute collision pt in time
  //   for inexact, push SRD to surf of big particle
  // update x,v of SRD and f,torque on big particle
  // re-bin SRD particle after collision
  // iterate until the SRD particle has no overlaps with big particles

  double **x = atom->x;
  double **v = atom->v;
  double **f = atom->f;
  double **torque = atom->torque;
  int *mask = atom->mask;
  int nlocal = atom->nlocal;

  for (i = 0; i < nlocal; i++) {
    if (!(mask[i] & groupbit)) continue;

    ibin = binsrd[i];
    if (nbinbig[ibin] == 0) continue;

    ibounce = 0;
    collide_flag = 1;
    dt = dt_big;

    while (collide_flag) {
      nbig = nbinbig[ibin];
      if (ibounce == 0) ncheck += nbig;

      collide_flag = 0;
      for (m = 0; m < nbig; m++) {
	k = binbig[ibin][m];
	big = &biglist[k];
	j = big->index;
	type = big->type;

	if (type == SPHERE) inside = inside_sphere(x[i],x[j],big);
	else inside = inside_ellipsoid(x[i],x[j],big);

	if (inside) {
	  if (exactflag) {
	    if (type == SPHERE)
	      t_remain = collision_sphere_exact(x[i],x[j],v[i],v[j],big,
						xscoll,xbcoll,norm);
	    else
	      t_remain = collision_ellipsoid_exact(x[i],x[j],v[i],v[j],big,
						   xscoll,xbcoll,norm);
	  } else {
	    t_remain = 0.5*dt;
	    if (type == SPHERE)
	      collision_sphere_inexact(x[i],x[j],big,xscoll,xbcoll,norm);
	    else
	      collision_ellipsoid_inexact(x[i],x[j],big,xscoll,xbcoll,norm);
	  }

#ifdef SRD_DEBUG
	  if (update->ntimestep == SRD_DEBUG_TIMESTEP &&
	      atom->tag[i] == SRD_DEBUG_ATOMID)
	    print_collision(i,j,ibounce,t_remain,dt,xscoll,xbcoll,norm);
#endif

	  if (t_remain > dt) {
	    ninside++;
	    if (insideflag == INSIDE_ERROR) {
	      char str[128];
	      sprintf(str,"SRD particle %d started "
		      "inside big particle %d on step %d bounce %d\n",
		      atom->tag[i],atom->tag[j],update->ntimestep,ibounce+1);
	      error->one(str);
	    }
	    if (insideflag == INSIDE_WARN) {
	      char str[128];
	      sprintf(str,"SRD particle %d started "
		      "inside big particle %d on step %d bounce %d\n",
		      atom->tag[i],atom->tag[j],update->ntimestep,ibounce+1);
	      error->warning(str);
	    }
	    break;
	  }

	  if (collidestyle == NOSLIP)
	    noslip(v[i],v[j],x[j],big,xscoll,norm,vsnew);
	  else if (type == SPHERE)
	    slip_sphere(v[i],v[j],norm,vsnew);
	  else if (type == ELLIPSOID)
	    slip_ellipsoid(v[i],v[j],x[j],big,xscoll,norm,vsnew);

	  if (dimension == 2) vsnew[2] = 0.0;

	  // check on rescaling of vsnew

	  double vsq = vsnew[0]*vsnew[0] + vsnew[1]*vsnew[1] + 
	    vsnew[2]*vsnew[2];
	  if (vsq > vmaxsq) nrescale++;

	  // update BIG particle and SRD
	  // BIG particle is not torqued if sphere and SLIP collision

	  if (collidestyle == SLIP && type == SPHERE)
	    force_torque(v[i],vsnew,xscoll,xbcoll,f[j],NULL);
	  else
	    force_torque(v[i],vsnew,xscoll,xbcoll,f[j],torque[j]);
	  ibin = binsrd[i] = update_srd(i,t_remain,xscoll,vsnew,x[i],v[i]);

	  if (ibounce == 0) ncollide++;
	  ibounce++;
	  if (ibounce < maxbounceallow || maxbounceallow == 0)
	    collide_flag = 1;
	  dt = t_remain;
	  break;
	}
      }
    }

    nbounce += ibounce;
    if (maxbounceallow && ibounce >= maxbounceallow) bouncemaxnum++;
    if (ibounce > bouncemax) bouncemax = ibounce;
  }
}

/* ----------------------------------------------------------------------
   detect all collisions between SRD and big particles
   an SRD can be inside more than one big particle at a time
   requires finding which big particle SRD collided with first
   unoverlap SRDs for each collision
------------------------------------------------------------------------- */

void FixSRD::collisions_multi()
{
  int i,j,k,m,type,nbig,ibin,ibounce,inside,jfirst;
  double dt,t_remain,t_first;
  double norm[3],xscoll[3],xbcoll[3],vsnew[3];
  double normfirst[3],xscollfirst[3],xbcollfirst[3];
  Big *big;

  // outer loop over SRD particles
  // inner loop over big particles that overlap SRD particle bin
  // loop over all big particles to find which one SRD collided with first
  // if overlap between SRD and big particle, compute collision pt in time
  // update x,v of SRD and f,torque on big particle
  // re-bin SRD particle after collision
  // iterate until the SRD particle has no overlaps with big particles

  double **x = atom->x;
  double **v = atom->v;
  double **f = atom->f;
  double **torque = atom->torque;
  int *mask = atom->mask;
  int nlocal = atom->nlocal;

  for (i = 0; i < nlocal; i++) {
    if (!(mask[i] & groupbit)) continue;

    ibin = binsrd[i];
    if (nbinbig[ibin] == 0) continue;

    ibounce = 0;
    dt = dt_big;

    while (1) {
      nbig = nbinbig[ibin];
      if (ibounce == 0) ncheck += nbig;

      t_first = 0.0;
      for (m = 0; m < nbig; m++) {
	k = binbig[ibin][m];
	big = &biglist[k];
	j = big->index;
	type = big->type;

	if (type == SPHERE) inside = inside_sphere(x[i],x[j],big);
	else inside = inside_ellipsoid(x[i],x[j],big);

	if (inside) {
	  if (type == SPHERE)
	    t_remain = collision_sphere_exact(x[i],x[j],v[i],v[j],big,
					      xscoll,xbcoll,norm);
	  else
	    t_remain = collision_ellipsoid_exact(x[i],x[j],v[i],v[j],big,
						 xscoll,xbcoll,norm);

#ifdef SRD_DEBUG
	  if (update->ntimestep == SRD_DEBUG_TIMESTEP &&
	      atom->tag[i] == SRD_DEBUG_ATOMID)
	    print_collision(i,j,ibounce,t_remain,dt,xscoll,xbcoll,norm);
#endif

	  if (t_remain > dt || t_remain < 0.0) {
	    ninside++;
	    if (insideflag == INSIDE_ERROR) {
	      char str[128];
	      sprintf(str,"SRD particle %d started "
		      "inside big particle %d on step %d bounce %d\n",
		      atom->tag[i],atom->tag[j],update->ntimestep,ibounce+1);
	      error->one(str);
	    }
	    if (insideflag == INSIDE_WARN) {
	      char str[128];
	      sprintf(str,"SRD particle %d started "
		      "inside big particle %d on step %d bounce %d\n",
		      atom->tag[i],atom->tag[j],update->ntimestep,ibounce+1);
	      error->warning(str);
	    }
	    t_first = 0.0;
	    break;
	  }

	  if (t_remain > t_first) {
	    t_first = t_remain;
	    jfirst = j;
	    xscollfirst[0] = xscoll[0];
	    xscollfirst[1] = xscoll[1];
	    xscollfirst[2] = xscoll[2];
	    xbcollfirst[0] = xbcoll[0];
	    xbcollfirst[1] = xbcoll[1];
	    xbcollfirst[2] = xbcoll[2];
	    normfirst[0] = norm[0];
	    normfirst[1] = norm[1];
	    normfirst[2] = norm[2];
	  }
	}
      }

      if (t_first == 0.0) break;
      j = jfirst;
      xscoll[0] = xscollfirst[0];
      xscoll[1] = xscollfirst[1];
      xscoll[2] = xscollfirst[2];
      xbcoll[0] = xbcollfirst[0];
      xbcoll[1] = xbcollfirst[1];
      xbcoll[2] = xbcollfirst[2];
      norm[0] = normfirst[0];
      norm[1] = normfirst[1];
      norm[2] = normfirst[2];

      if (collidestyle == NOSLIP)
	noslip(v[i],v[j],x[j],big,xscoll,norm,vsnew);
      else if (type == SPHERE)
	slip_sphere(v[i],v[j],norm,vsnew);
      else if (type == ELLIPSOID)
	slip_ellipsoid(v[i],v[j],x[j],big,xscoll,norm,vsnew);

      if (dimension == 2) vsnew[2] = 0.0;

      // check on rescaling of vsnew

      double vsq = vsnew[0]*vsnew[0] + vsnew[1]*vsnew[1] + vsnew[2]*vsnew[2];
      if (vsq > vmaxsq) nrescale++;

      // update BIG particle and SRD
      // BIG particle is not torqued if sphere and SLIP collision

      if (collidestyle == SLIP && type == SPHERE)
	force_torque(v[i],vsnew,xscoll,xbcoll,f[j],NULL);
      else
	force_torque(v[i],vsnew,xscoll,xbcoll,f[j],torque[j]);
      ibin = binsrd[i] = update_srd(i,t_remain,xscoll,vsnew,x[i],v[i]);

      if (ibounce == 0) ncollide++;
      ibounce++;
      if (ibounce == maxbounceallow) break;
      dt = t_first;
    }

    nbounce += ibounce;
    if (maxbounceallow && ibounce >= maxbounceallow) bouncemaxnum++;
    if (ibounce > bouncemax) bouncemax = ibounce;
  }
}

/* ----------------------------------------------------------------------
   check if SRD particle S is inside spherical big particle B
------------------------------------------------------------------------- */

int FixSRD::inside_sphere(double *xs, double *xb, Big *big)
{
  double dx,dy,dz;

  dx = xs[0] - xb[0];
  dy = xs[1] - xb[1];
  dz = xs[2] - xb[2];

  if (dx*dx + dy*dy + dz*dz <= big->radsq) return 1;
  return 0;
}

/* ----------------------------------------------------------------------
   check if SRD particle S is inside ellipsoidal big particle B
------------------------------------------------------------------------- */

int FixSRD::inside_ellipsoid(double *xs, double *xb, Big *big)
{
  double x,y,z;

  double *ex = big->ex;
  double *ey = big->ey;
  double *ez = big->ez;

  double xs_xb[3];
  xs_xb[0] = xs[0] - xb[0];
  xs_xb[1] = xs[1] - xb[1];
  xs_xb[2] = xs[2] - xb[2];

  x = xs_xb[0]*ex[0] + xs_xb[1]*ex[1] + xs_xb[2]*ex[2];
  y = xs_xb[0]*ey[0] + xs_xb[1]*ey[1] + xs_xb[2]*ey[2];
  z = xs_xb[0]*ez[0] + xs_xb[1]*ez[1] + xs_xb[2]*ez[2];

  if (x*x*big->aradsqinv + y*y*big->bradsqinv + z*z*big->cradsqinv <= 1.0)
    return 1;
  return 0;
}

/* ----------------------------------------------------------------------
   collision of SRD particle S with surface of spherical big particle B
   exact because compute time of collision
   dt = time previous to now at which collision occurs
   xscoll = collision pt = position of SRD at time of collision
   xbcoll = position of big particle at time of collision
   norm = surface normal of collision pt at time of collision
------------------------------------------------------------------------- */

double FixSRD::collision_sphere_exact(double *xs, double *xb,
				      double *vs, double *vb, Big *big,
				      double *xscoll, double *xbcoll,
				      double *norm)
{
  double vs_dot_vs,vb_dot_vb,vs_dot_vb;
  double vs_dot_xb,vb_dot_xs,vs_dot_xs,vb_dot_xb;
  double xs_dot_xs,xb_dot_xb,xs_dot_xb;
  double a,b,c,scale,dt;

  vs_dot_vs = vs[0]*vs[0] + vs[1]*vs[1] + vs[2]*vs[2];
  vb_dot_vb = vb[0]*vb[0] + vb[1]*vb[1] + vb[2]*vb[2];
  vs_dot_vb = vs[0]*vb[0] + vs[1]*vb[1] + vs[2]*vb[2];

  vs_dot_xb = vs[0]*xb[0] + vs[1]*xb[1] + vs[2]*xb[2];
  vb_dot_xs = vb[0]*xs[0] + vb[1]*xs[1] + vb[2]*xs[2];
  vs_dot_xs = vs[0]*xs[0] + vs[1]*xs[1] + vs[2]*xs[2];
  vb_dot_xb = vb[0]*xb[0] + vb[1]*xb[1] + vb[2]*xb[2];

  xs_dot_xs = xs[0]*xs[0] + xs[1]*xs[1] + xs[2]*xs[2];
  xb_dot_xb = xb[0]*xb[0] + xb[1]*xb[1] + xb[2]*xb[2];
  xs_dot_xb = xs[0]*xb[0] + xs[1]*xb[1] + xs[2]*xb[2];

  a = vs_dot_vs + vb_dot_vb - 2.0*vs_dot_vb;
  b = 2.0 * (vs_dot_xb + vb_dot_xs - vs_dot_xs - vb_dot_xb);
  c = xs_dot_xs + xb_dot_xb - 2.0*xs_dot_xb - big->radsq;

  dt = (-b + sqrt(b*b - 4.0*a*c)) / (2.0*a);

  xscoll[0] = xs[0] - dt*vs[0];
  xscoll[1] = xs[1] - dt*vs[1];
  xscoll[2] = xs[2] - dt*vs[2];

  xbcoll[0] = xb[0] - dt*vb[0];
  xbcoll[1] = xb[1] - dt*vb[1];
  xbcoll[2] = xb[2] - dt*vb[2];

  norm[0] = xscoll[0] - xbcoll[0];
  norm[1] = xscoll[1] - xbcoll[1];
  norm[2] = xscoll[2] - xbcoll[2];
  scale = 1.0/sqrt(norm[0]*norm[0] + norm[1]*norm[1] + norm[2]*norm[2]);
  norm[0] *= scale;
  norm[1] *= scale;
  norm[2] *= scale;

  return dt;
}

/* ----------------------------------------------------------------------
   collision of SRD particle S with surface of spherical big particle B
   inexact because just push SRD to surface of big particle at end of step
   time of collision = end of step
   xscoll = collision pt = position of SRD at time of collision
   xbcoll = xb = position of big particle at time of collision
   norm = surface normal of collision pt at time of collision
------------------------------------------------------------------------- */

void FixSRD::collision_sphere_inexact(double *xs, double *xb,
				      Big *big,
				      double *xscoll, double *xbcoll,
				      double *norm)
{
  double scale;

  norm[0] = xs[0] - xb[0];
  norm[1] = xs[1] - xb[1];
  norm[2] = xs[2] - xb[2];
  scale = 1.0/sqrt(norm[0]*norm[0] + norm[1]*norm[1] + norm[2]*norm[2]);
  norm[0] *= scale;
  norm[1] *= scale;
  norm[2] *= scale;

  xscoll[0] = xb[0] + big->radius*norm[0];
  xscoll[1] = xb[1] + big->radius*norm[1];
  xscoll[2] = xb[2] + big->radius*norm[2];

  xbcoll[0] = xb[0];
  xbcoll[1] = xb[1];
  xbcoll[2] = xb[2];
}

/* ----------------------------------------------------------------------
   collision of SRD particle S with surface of ellipsoidal big particle B
   exact because compute time of collision
   dt = time previous to now at which collision occurs
   xscoll = collision pt = position of SRD at time of collision
   xbcoll = position of big particle at time of collision
   norm = surface normal of collision pt at time of collision
------------------------------------------------------------------------- */

double FixSRD::collision_ellipsoid_exact(double *xs, double *xb,
					 double *vs, double *vb, Big *big,
					 double *xscoll, double *xbcoll,
					 double *norm)
{
  double vs_vb[3],xs_xb[3],omega_ex[3],omega_ey[3],omega_ez[3];
  double excoll[3],eycoll[3],ezcoll[3],delta[3],xbody[3],nbody[3];
  double ax,bx,cx,ay,by,cy,az,bz,cz;
  double a,b,c,dt,scale;

  double *omega = big->omega;
  double *ex = big->ex;
  double *ey = big->ey;
  double *ez = big->ez;

  vs_vb[0] = vs[0]-vb[0]; vs_vb[1] = vs[1]-vb[1]; vs_vb[2] = vs[2]-vb[2];
  xs_xb[0] = xs[0]-xb[0]; xs_xb[1] = xs[1]-xb[1]; xs_xb[2] = xs[2]-xb[2];

  omega_ex[0] = omega[1]*ex[2] - omega[2]*ex[1];
  omega_ex[1] = omega[2]*ex[0] - omega[0]*ex[2];
  omega_ex[2] = omega[0]*ex[1] - omega[1]*ex[0];

  omega_ey[0] = omega[1]*ey[2] - omega[2]*ey[1];
  omega_ey[1] = omega[2]*ey[0] - omega[0]*ey[2];
  omega_ey[2] = omega[0]*ey[1] - omega[1]*ey[0];

  omega_ez[0] = omega[1]*ez[2] - omega[2]*ez[1];
  omega_ez[1] = omega[2]*ez[0] - omega[0]*ez[2];
  omega_ez[2] = omega[0]*ez[1] - omega[1]*ez[0];

  ax = vs_vb[0]*omega_ex[0] + vs_vb[1]*omega_ex[1] + vs_vb[2]*omega_ex[2];
  bx = -(vs_vb[0]*ex[0] + vs_vb[1]*ex[1] + vs_vb[2]*ex[2]);
  bx -= xs_xb[0]*omega_ex[0] + xs_xb[1]*omega_ex[1] + xs_xb[2]*omega_ex[2];
  cx = xs_xb[0]*ex[0] + xs_xb[1]*ex[1] + xs_xb[2]*ex[2];

  ay = vs_vb[0]*omega_ey[0] + vs_vb[1]*omega_ey[1] + vs_vb[2]*omega_ey[2];
  by = -(vs_vb[0]*ey[0] + vs_vb[1]*ey[1] + vs_vb[2]*ey[2]);
  by -= xs_xb[0]*omega_ey[0] + xs_xb[1]*omega_ey[1] + xs_xb[2]*omega_ey[2];
  cy = xs_xb[0]*ey[0] + xs_xb[1]*ey[1] + xs_xb[2]*ey[2];

  az = vs_vb[0]*omega_ez[0] + vs_vb[1]*omega_ez[1] + vs_vb[2]*omega_ez[2];
  bz = -(vs_vb[0]*ez[0] + vs_vb[1]*ez[1] + vs_vb[2]*ez[2]);
  bz -= xs_xb[0]*omega_ez[0] + xs_xb[1]*omega_ez[1] + xs_xb[2]*omega_ez[2];
  cz = xs_xb[0]*ez[0] + xs_xb[1]*ez[1] + xs_xb[2]*ez[2];

  a = (bx*bx + 2.0*ax*cx)*big->aradsqinv +
    (by*by + 2.0*ay*cy)*big->bradsqinv + 
    (bz*bz + 2.0*az*cz)*big->cradsqinv;
  b = 2.0 * (bx*cx*big->aradsqinv + by*cy*big->bradsqinv +
	     bz*cz*big->cradsqinv);
  c = cx*cx*big->aradsqinv + cy*cy*big->bradsqinv +
    cz*cz*big->cradsqinv - 1.0;

  dt = (-b + sqrt(b*b - 4.0*a*c)) / (2.0*a);

  xscoll[0] = xs[0] - dt*vs[0];
  xscoll[1] = xs[1] - dt*vs[1];
  xscoll[2] = xs[2] - dt*vs[2];

  xbcoll[0] = xb[0] - dt*vb[0];
  xbcoll[1] = xb[1] - dt*vb[1];
  xbcoll[2] = xb[2] - dt*vb[2];

  // calculate normal to ellipsoid at collision pt
  // Excoll,Eycoll,Ezcoll = orientation of ellipsoid at collision time
  // nbody = normal in body frame of ellipsoid (Excoll,Eycoll,Ezcoll)
  // norm = normal in space frame
  // only worry about normalizing final norm vector

  excoll[0] = ex[0] - dt * (omega[1]*ex[2] - omega[2]*ex[1]);
  excoll[1] = ex[1] - dt * (omega[2]*ex[0] - omega[0]*ex[2]);
  excoll[2] = ex[2] - dt * (omega[0]*ex[1] - omega[1]*ex[0]);

  eycoll[0] = ey[0] - dt * (omega[1]*ey[2] - omega[2]*ey[1]);
  eycoll[1] = ey[1] - dt * (omega[2]*ey[0] - omega[0]*ey[2]);
  eycoll[2] = ey[2] - dt * (omega[0]*ey[1] - omega[1]*ey[0]);

  ezcoll[0] = ez[0] - dt * (omega[1]*ez[2] - omega[2]*ez[1]);
  ezcoll[1] = ez[1] - dt * (omega[2]*ez[0] - omega[0]*ez[2]);
  ezcoll[2] = ez[2] - dt * (omega[0]*ez[1] - omega[1]*ez[0]);

  delta[0] = xscoll[0] - xbcoll[0];
  delta[1] = xscoll[1] - xbcoll[1];
  delta[2] = xscoll[2] - xbcoll[2];

  xbody[0] = delta[0]*excoll[0] + delta[1]*excoll[1] + delta[2]*excoll[2];
  xbody[1] = delta[0]*eycoll[0] + delta[1]*eycoll[1] + delta[2]*eycoll[2];
  xbody[2] = delta[0]*ezcoll[0] + delta[1]*ezcoll[1] + delta[2]*ezcoll[2];

  nbody[0] = xbody[0]*big->aradsqinv;
  nbody[1] = xbody[1]*big->bradsqinv;
  nbody[2] = xbody[2]*big->cradsqinv;

  norm[0] = excoll[0]*nbody[0] + eycoll[0]*nbody[1] + ezcoll[0]*nbody[2];
  norm[1] = excoll[1]*nbody[0] + eycoll[1]*nbody[1] + ezcoll[1]*nbody[2];
  norm[2] = excoll[2]*nbody[0] + eycoll[2]*nbody[1] + ezcoll[2]*nbody[2];
  scale = 1.0/sqrt(norm[0]*norm[0] + norm[1]*norm[1] + norm[2]*norm[2]);
  norm[0] *= scale;
  norm[1] *= scale;
  norm[2] *= scale;

  return dt;
}

/* ----------------------------------------------------------------------
   collision of SRD particle S with surface of ellipsoidal big particle B
   inexact because just push SRD to surface of big particle at end of step
------------------------------------------------------------------------- */

void FixSRD::collision_ellipsoid_inexact(double *xs, double *xb,
					 Big *big,
					 double *xscoll, double *xbcoll,
					 double *norm)
{
  double xs_xb[3],delta[3],xbody[3],nbody[3];
  double x,y,z,scale;

  double *ex = big->ex;
  double *ey = big->ey;
  double *ez = big->ez;

  xs_xb[0] = xs[0] - xb[0];
  xs_xb[1] = xs[1] - xb[1];
  xs_xb[2] = xs[2] - xb[2];

  x = xs_xb[0]*ex[0] + xs_xb[1]*ex[1] + xs_xb[2]*ex[2];
  y = xs_xb[0]*ey[0] + xs_xb[1]*ey[1] + xs_xb[2]*ey[2];
  z = xs_xb[0]*ez[0] + xs_xb[1]*ez[1] + xs_xb[2]*ez[2];

  scale = 1.0/sqrt(x*x*big->aradsqinv + y*y*big->bradsqinv +
		   z*z*big->cradsqinv);
  x *= scale;
  y *= scale;
  z *= scale;

  xscoll[0] = x*ex[0] + y*ey[0] + z*ez[0] + xb[0];
  xscoll[1] = x*ex[1] + y*ey[1] + z*ez[1] + xb[1];
  xscoll[2] = x*ex[2] + y*ey[2] + z*ez[2] + xb[2];

  xbcoll[0] = xb[0];
  xbcoll[1] = xb[1];
  xbcoll[2] = xb[2];

  // calculate normal to ellipsoid at collision pt
  // nbody = normal in body frame of ellipsoid
  // norm = normal in space frame
  // only worry about normalizing final norm vector

  delta[0] = xscoll[0] - xbcoll[0];
  delta[1] = xscoll[1] - xbcoll[1];
  delta[2] = xscoll[2] - xbcoll[2];

  xbody[0] = delta[0]*ex[0] + delta[1]*ex[1] + delta[2]*ex[2];
  xbody[1] = delta[0]*ey[0] + delta[1]*ey[1] + delta[2]*ey[2];
  xbody[2] = delta[0]*ez[0] + delta[1]*ez[1] + delta[2]*ez[2];

  nbody[0] = xbody[0]*big->aradsqinv;
  nbody[1] = xbody[1]*big->bradsqinv;
  nbody[2] = xbody[2]*big->cradsqinv;

  norm[0] = ex[0]*nbody[0] + ey[0]*nbody[1] + ez[0]*nbody[2];
  norm[1] = ex[1]*nbody[0] + ey[1]*nbody[1] + ez[1]*nbody[2];
  norm[2] = ex[2]*nbody[0] + ey[2]*nbody[1] + ez[2]*nbody[2];
  scale = 1.0/sqrt(norm[0]*norm[0] + norm[1]*norm[1] + norm[2]*norm[2]);
  norm[0] *= scale;
  norm[1] *= scale;
  norm[2] *= scale;
}

/* ----------------------------------------------------------------------
   SLIP collision with sphere
   vs = velocity of SRD, vb = velocity of BIG
   norm = unit normal from surface of BIG at collision pt
   v of BIG particle in direction of surf normal is added to v of SRD
   return vsnew of SRD
------------------------------------------------------------------------- */

void FixSRD::slip_sphere(double *vs, double *vb, double *norm, double *vsnew)
{
  double r1,r2,vnmag,vs_dot_n,vsurf_dot_n;
  double tangent[3];

  while (1) {
    r1 = sigma * random->gaussian();
    r2 = sigma * random->gaussian();
    vnmag = sqrt(r1*r1 + r2*r2);
    if (vnmag*vnmag <= vmaxsq) break;
  }

  vs_dot_n = vs[0]*norm[0] + vs[1]*norm[1] + vs[2]*norm[2];

  tangent[0] = vs[0] - vs_dot_n*norm[0];
  tangent[1] = vs[1] - vs_dot_n*norm[1];
  tangent[2] = vs[2] - vs_dot_n*norm[2];

  // vsurf = velocity of collision pt = translation/rotation of BIG particle
  // for sphere, only vb (translation) can contribute in normal direction

  vsurf_dot_n = vb[0]*norm[0] + vb[1]*norm[1] + vb[2]*norm[2];

  vsnew[0] = (vnmag+vsurf_dot_n)*norm[0] + tangent[0];
  vsnew[1] = (vnmag+vsurf_dot_n)*norm[1] + tangent[1];
  vsnew[2] = (vnmag+vsurf_dot_n)*norm[2] + tangent[2];
}

/* ----------------------------------------------------------------------
   SLIP collision with ellipsoid
   vs = velocity of SRD, vb = velocity of BIG
   xb = position of BIG, omega = rotation of BIG
   xsurf = collision pt on surf of BIG
   norm = unit normal from surface of BIG at collision pt
   v of BIG particle in direction of surf normal is added to v of SRD
   includes component due to rotation of ellipsoid
   return vsnew of SRD
------------------------------------------------------------------------- */

void FixSRD::slip_ellipsoid(double *vs, double *vb, double *xb, Big *big,
			    double *xsurf, double *norm, double *vsnew)
{
  double r1,r2,vnmag,vs_dot_n,vsurf_dot_n;
  double tangent[3],vsurf[3];
  double *omega = big->omega;

  while (1) {
    r1 = sigma * random->gaussian();
    r2 = sigma * random->gaussian();
    vnmag = sqrt(r1*r1 + r2*r2);
    if (vnmag*vnmag <= vmaxsq) break;
  }

  vs_dot_n = vs[0]*norm[0] + vs[1]*norm[1] + vs[2]*norm[2];

  tangent[0] = vs[0] - vs_dot_n*norm[0];
  tangent[1] = vs[1] - vs_dot_n*norm[1];
  tangent[2] = vs[2] - vs_dot_n*norm[2];

  // vsurf = velocity of collision pt = translation/rotation of BIG particle

  vsurf[0] = vb[0] + omega[1]*(xsurf[2]-xb[2]) - omega[2]*(xsurf[1]-xb[1]);
  vsurf[1] = vb[1] + omega[2]*(xsurf[0]-xb[0]) - omega[0]*(xsurf[2]-xb[2]);
  vsurf[2] = vb[2] + omega[0]*(xsurf[1]-xb[1]) - omega[1]*(xsurf[0]-xb[0]);

  vsurf_dot_n = vsurf[0]*norm[0] + vsurf[1]*norm[1] + vsurf[2]*norm[2];

  vsnew[0] = (vnmag+vsurf_dot_n)*norm[0] + tangent[0];
  vsnew[1] = (vnmag+vsurf_dot_n)*norm[1] + tangent[1];
  vsnew[2] = (vnmag+vsurf_dot_n)*norm[2] + tangent[2];
}

/* ----------------------------------------------------------------------
   NO-SLIP collision with sphere or ellipsoid
   vs = velocity of SRD, vb = velocity of BIG
   xb = position of BIG, omega = rotation of BIG
   xsurf = collision pt on surf of BIG
   norm = unit normal from surface of BIG at collision pt
   v of collision pt is added to v of SRD
   includes component due to rotation of BIG
   return vsnew of SRD
------------------------------------------------------------------------- */

void FixSRD::noslip(double *vs, double *vb, double *xb, Big *big,
		    double *xsurf, double *norm, double *vsnew)
{
  double vs_dot_n,scale,r1,r2,vnmag,vtmag1,vtmag2;
  double tangent1[3],tangent2[3];
  double *omega = big->omega;

  vs_dot_n = vs[0]*norm[0] + vs[1]*norm[1] + vs[2]*norm[2];

  tangent1[0] = vs[0] - vs_dot_n*norm[0];
  tangent1[1] = vs[1] - vs_dot_n*norm[1];
  tangent1[2] = vs[2] - vs_dot_n*norm[2];
  scale = 1.0/sqrt(tangent1[0]*tangent1[0] + tangent1[1]*tangent1[1] +
		   tangent1[2]*tangent1[2]);
  tangent1[0] *= scale;
  tangent1[1] *= scale;
  tangent1[2] *= scale;

  tangent2[0] = norm[1]*tangent1[2] - norm[2]*tangent1[1];
  tangent2[1] = norm[2]*tangent1[0] - norm[0]*tangent1[2];
  tangent2[2] = norm[0]*tangent1[1] - norm[1]*tangent1[0];

  while (1) {
    r1 = sigma * random->gaussian();
    r2 = sigma * random->gaussian();
    vnmag = sqrt(r1*r1 + r2*r2);
    vtmag1 = sigma * random->gaussian();
    vtmag2 = sigma * random->gaussian();
    if (vnmag*vnmag + vtmag1*vtmag1 + vtmag2*vtmag2 <= vmaxsq) break;
  }

  vsnew[0] = vnmag*norm[0] + vtmag1*tangent1[0] + vtmag2*tangent2[0];
  vsnew[1] = vnmag*norm[1] + vtmag1*tangent1[1] + vtmag2*tangent2[1];
  vsnew[2] = vnmag*norm[2] + vtmag1*tangent1[2] + vtmag2*tangent2[2];

  // add in velocity of collision pt = translation/rotation of BIG particle

  vsnew[0] += vb[0] + omega[1]*(xsurf[2]-xb[2]) - omega[2]*(xsurf[1]-xb[1]);
  vsnew[1] += vb[1] + omega[2]*(xsurf[0]-xb[0]) - omega[0]*(xsurf[2]-xb[2]);
  vsnew[2] += vb[2] + omega[0]*(xsurf[1]-xb[1]) - omega[1]*(xsurf[0]-xb[0]);
}

/* ----------------------------------------------------------------------
   impart force and torque to BIG particle
   force on big particle = -dp/dt of SRD particle
   torque on big particle = r cross (-dp/dt)
------------------------------------------------------------------------- */

void FixSRD::force_torque(double *vsold, double *vsnew,
			  double *xs, double *xb,
			  double *fb, double *tb)
{
  double dpdt[3],xs_xb[3];

  double factor = mass_srd / dt_big / force->ftm2v;
  dpdt[0] = factor * (vsnew[0] - vsold[0]);
  dpdt[1] = factor * (vsnew[1] - vsold[1]);
  dpdt[2] = factor * (vsnew[2] - vsold[2]);

  fb[0] -= dpdt[0];
  fb[1] -= dpdt[1];
  fb[2] -= dpdt[2];

  // no torque if SLIP collision and BIG is a sphere

  if (tb) {
    xs_xb[0] = xs[0]-xb[0];
    xs_xb[1] = xs[1]-xb[1];
    xs_xb[2] = xs[2]-xb[2];
    tb[0] -= xs_xb[1]*dpdt[2] - xs_xb[2]*dpdt[1];
    tb[1] -= xs_xb[2]*dpdt[0] - xs_xb[0]*dpdt[2];
    tb[2] -= xs_xb[0]*dpdt[1] - xs_xb[1]*dpdt[0];
  }
}

/* ----------------------------------------------------------------------
   update SRD particle position & velocity & search bin assignment
   check if SRD moved outside of valid region
   if so, may overlap off-processor BIG particle
------------------------------------------------------------------------- */

int FixSRD::update_srd(int i, double dt, double *xscoll, double *vsnew,
		       double *xs, double *vs)
{
  int ix,iy,iz;

  vs[0] = vsnew[0];
  vs[1] = vsnew[1];
  vs[2] = vsnew[2];

  xs[0] = xscoll[0] + dt*vsnew[0];
  xs[1] = xscoll[1] + dt*vsnew[1];
  xs[2] = xscoll[2] + dt*vsnew[2];

  if (triclinic) domain->x2lamda(xs,xs);

  if (xs[0] < srdlo[0] || xs[0] > srdhi[0] || 
      xs[1] < srdlo[1] || xs[1] > srdhi[1] || 
      xs[2] < srdlo[2] || xs[2] > srdhi[2]) {
    printf("Bad SRD particle move\n");
    printf("  particle %d on proc %d at timestep %d\n",
	   atom->tag[i],me,update->ntimestep);
    printf("  xnew %g %g %g\n",xs[0],xs[1],xs[2]);
    printf("  srdlo/hi x %g %g\n",srdlo[0],srdhi[0]);
    printf("  srdlo/hi y %g %g\n",srdlo[1],srdhi[1]);
    printf("  srdlo/hi z %g %g\n",srdlo[2],srdhi[2]);
    error->warning("Fix srd particle moved outside valid domain");
  }

  if (triclinic) domain->lamda2x(xs,xs);

  ix = static_cast<int> ((xs[0]-xblo2)*bininv2x);
  iy = static_cast<int> ((xs[1]-yblo2)*bininv2y);
  iz = static_cast<int> ((xs[2]-zblo2)*bininv2z);
  return iz*nbin2y*nbin2x + iy*nbin2x + ix;
}

/* ----------------------------------------------------------------------
   setup all SRD parameters with big particles
------------------------------------------------------------------------- */

void FixSRD::parameterize()
{
  double PI = 4.0*atan(1.0);

  // timesteps

  dt_big = update->dt;
  dt_srd = nevery * update->dt;

  // maxbigdiam,minbigdiam = max/min diameter of any big particle
  // big particle must either have radius > 0 or shape > 0 defined
  // apply radfactor at end

  double *radius = atom->radius;
  double **shape = atom->shape;
  int *type = atom->type;
  int *mask = atom->mask;
  int nlocal = atom->nlocal;

  any_ellipsoids = 0;
  maxbigdiam = 0.0;
  minbigdiam = BIG;

  for (int i = 0; i < nlocal; i++)
    if (mask[i] & biggroupbit) {
      if (radius) {
	if (radius[i] == 0.0)
	  error->one("Big particle in fix srd cannot be point particle");
	maxbigdiam = MAX(maxbigdiam,2.0*radius[i]);
	minbigdiam = MIN(minbigdiam,2.0*radius[i]);
      } else {
	if (shape[type[i]][0] == 0.0)
	  error->one("Big particle in fix srd cannot be point particle");
	maxbigdiam = MAX(maxbigdiam,2.0*shape[type[i]][0]);
	maxbigdiam = MAX(maxbigdiam,2.0*shape[type[i]][1]);
	maxbigdiam = MAX(maxbigdiam,2.0*shape[type[i]][2]);
	minbigdiam = MIN(minbigdiam,2.0*shape[type[i]][0]);
	minbigdiam = MIN(minbigdiam,2.0*shape[type[i]][1]);
	minbigdiam = MIN(minbigdiam,2.0*shape[type[i]][2]);
	if (shape[type[i]][0] != shape[type[i]][1] ||
	    shape[type[i]][0] != shape[type[i]][2])
	  any_ellipsoids = 1;
      }
    }

  double tmp = maxbigdiam;
  MPI_Allreduce(&tmp,&maxbigdiam,1,MPI_DOUBLE,MPI_MAX,world);
  tmp = minbigdiam;
  MPI_Allreduce(&tmp,&minbigdiam,1,MPI_DOUBLE,MPI_MIN,world);

  maxbigdiam *= radfactor;
  minbigdiam *= radfactor;

  int itmp = any_ellipsoids;
  MPI_Allreduce(&itmp,&any_ellipsoids,1,MPI_INT,MPI_MAX,world);

  // big particles are only torqued if are ellipsoids or NOSLIP collisions

  if (any_ellipsoids == 0 && collidestyle == SLIP) torqueflag = 0;
  else torqueflag = 1;

  // mass of SRD particles, require monodispersity

  double *rmass = atom->rmass;
  double *mass = atom->mass;

  int flag = 0;
  mass_srd = 0.0;

  for (int i = 0; i < nlocal; i++)
    if (mask[i] & groupbit) {
      if (rmass) {
	if (mass_srd == 0.0) mass_srd = rmass[i];
	else if (rmass[i] != mass_srd) flag = 1;
      } else {
	if (mass_srd == 0.0) mass_srd = mass[type[i]];
	else if (mass[type[i]] != mass_srd) flag = 1;
      }
    }

  int flagall;
  MPI_Allreduce(&flag,&flagall,1,MPI_INT,MPI_MAX,world);
  if (flagall) 
    error->all("Fix srd requires SRD particles all have same mass");

  // set temperature and lamda of SRD particles from each other
  // lamda = dt_srd * sqrt(boltz * temperature_srd / mass_srd);

  if (lamdaflag == 0)
    lamda = dt_srd * sqrt(force->boltz*temperature_srd/mass_srd/force->mvv2e);
  else
    temperature_srd = force->mvv2e * 
      (lamda/dt_srd)*(lamda/dt_srd) * mass_srd/force->boltz;

  // vmax = maximum velocity of an SRD particle
  // dmax = maximum distance an SRD can move = 4*lamda = vmax * dt_srd

  sigma = lamda/dt_srd;
  dmax = 4.0*lamda;
  vmax = dmax/dt_srd;
  vmaxsq = vmax*vmax;

  // volbig = total volume of all big particles
  // apply radfactor to reduce final volume

  double volbig = 0.0;

  if (dimension == 3) {
    for (int i = 0; i < nlocal; i++)
      if (mask[i] & biggroupbit) {
	if (radius)
	  volbig += 4.0/3.0*PI*radius[i]*radius[i]*radius[i];
	else
	  volbig += 4.0/3.0*PI *
	    shape[type[i]][0]*shape[type[i]][1]*shape[type[i]][2];
      }
  } else {
    for (int i = 0; i < nlocal; i++)
      if (mask[i] & biggroupbit) {
	if (radius)
	  volbig += PI*radius[i]*radius[i];
	else
	  volbig += PI*shape[type[i]][0]*shape[type[i]][1];
      }
  }

  tmp = volbig;
  MPI_Allreduce(&tmp,&volbig,1,MPI_DOUBLE,MPI_SUM,world);

  if (dimension == 3) volbig *= radfactor*radfactor*radfactor;
  else volbig *= radfactor*radfactor;

  // particle counts

  double nbig = 0.0;
  if (bigexist) nbig = group->count(biggroup);
  double nsrd = group->count(igroup);

  // mass_big = total mass of all big particles

  mass_big = 0.0;
  for (int i = 0; i < nlocal; i++)
    if (mask[i] & biggroupbit) {
      if (rmass) mass_big += rmass[i];
      else mass_big += mass[type[i]];
    }

  tmp = mass_big;
  MPI_Allreduce(&tmp,&mass_big,1,MPI_DOUBLE,MPI_SUM,world);

  // mass density ratio = big / SRD

  double density_big = 0.0;
  if (bigexist) density_big = mass_big / volbig;

  double volsrd,density_srd;
  if (dimension == 3) {
    volsrd = (domain->xprd * domain->yprd * domain->zprd) - volbig;
    density_srd = nsrd * mass_srd / 
      (domain->xprd*domain->yprd*domain->zprd - volbig);
  } else {
    volsrd = (domain->xprd * domain->yprd) - volbig;
    density_srd = nsrd * mass_srd /
      (domain->xprd*domain->yprd - volbig);
  }

  double mdratio = density_big/density_srd;

  // create grid for binning/rotating SRD particles from gridsrd

  setup_velocity_bins();

  // binsize3 = binsize1 in box units (not lamda units for triclinic)

  double binsize3x = binsize1x;
  double binsize3y = binsize1y;
  double binsize3z = binsize1z;
  if (triclinic) {
    binsize3x = binsize1x * domain->xprd;
    binsize3y = binsize1y * domain->yprd;
    binsize3z = binsize1z * domain->zprd;
  }

  // srd_per_grid = # of SRD particles per SRD grid cell

  double ncell;
  if (dimension == 3)
    ncell = volsrd / (binsize3x*binsize3y*binsize3z);
  else
    ncell = volsrd / (binsize3x*binsize3y);

  srd_per_cell = nsrd / ncell;

  // kinematic viscosity of SRD fluid
  // output in cm^2/sec units, converted by xxt2kmu

  double viscosity;
  if (dimension == 3)
    viscosity = gridsrd*gridsrd/(18.0*dt_srd) * 
      (1.0-(1.0-exp(-srd_per_cell))/srd_per_cell) + 
      (force->boltz*temperature_srd*dt_srd/(4.0*mass_srd*force->mvv2e)) *
      ((srd_per_cell+2.0)/(srd_per_cell-1.0));
  else
    viscosity = 
      (force->boltz*temperature_srd*dt_srd/(2.0*mass_srd*force->mvv2e)) *
      (srd_per_cell/(srd_per_cell-1.0 + exp(-srd_per_cell)) - 1.0) +
      (gridsrd*gridsrd)/(12.0*dt_srd) * 
      ((srd_per_cell-1.0 + exp(-srd_per_cell))/srd_per_cell);
  viscosity *= force->xxt2kmu;

  // print SRD parameters

  if (me == 0) {
    if (screen) {
      fprintf(screen,"SRD info:\n");
      fprintf(screen,"  SRD/big particles = %g %g\n",nsrd,nbig);
      fprintf(screen,"  big particle diameter max/min = %g %g\n",
	      maxbigdiam,minbigdiam);
      fprintf(screen,"  SRD temperature & lamda = %g %g\n",
	      temperature_srd,lamda);
      fprintf(screen,"  SRD max distance & max velocity = %g %g\n",dmax,vmax);
      fprintf(screen,"  SRD grid counts: %d %d %d\n",nbin1x,nbin1y,nbin1z);
      fprintf(screen,"  SRD grid size: request, actual (xyz) = %g, %g %g %g\n",
	      gridsrd,binsize3x,binsize3y,binsize3z);
      fprintf(screen,"  SRD per actual grid cell = %g\n",srd_per_cell);
      fprintf(screen,"  SRD viscosity = %g\n",viscosity);
      fprintf(screen,"  big/SRD mass density ratio = %g\n",mdratio);
    }
    if (logfile) {
      fprintf(logfile,"SRD info:\n");
      fprintf(logfile,"  SRD/big particles = %g %g\n",nsrd,nbig);
      fprintf(logfile,"  big particle diameter max/min = %g %g\n",
	      maxbigdiam,minbigdiam);
      fprintf(logfile,"  SRD temperature & lamda = %g %g\n",
	      temperature_srd,lamda);
      fprintf(logfile,"  SRD max distance & max velocity = %g %g\n",dmax,vmax);
      fprintf(logfile,"  SRD grid counts: %d %d %d\n",nbin1x,nbin1y,nbin1z);
      fprintf(logfile,"  SRD grid size: request, actual (xyz) = %g, %g %g %g\n",
	      gridsrd,binsize3x,binsize3y,binsize3z);
      fprintf(logfile,"  SRD per actual grid cell = %g\n",srd_per_cell);
      fprintf(logfile,"  SRD viscosity = %g\n",viscosity);
      fprintf(logfile,"  big/SRD mass density ratio = %g\n",mdratio);
    }
  }

  // error if less than 1 SRD bin per processor in some dim

  if (nbin1x < comm->procgrid[0] || nbin1y < comm->procgrid[1] || 
      nbin1z < comm->procgrid[2]) 
    error->all("Fewer SRD bins than processors in some dimension");

  // check if SRD bins are within tolerance for shape and size

  int tolflag = 0;
  if (binsize3y/binsize3x > 1.0+cubictol ||
      binsize3x/binsize3y > 1.0+cubictol) tolflag = 1;
  if (dimension == 3) {
    if (binsize3z/binsize3x > 1.0+cubictol ||
	binsize3x/binsize3z > 1.0+cubictol) tolflag = 1;
  }

  if (tolflag) {
    if (cubicflag == CUBIC_ERROR)
      error->all("SRD bins for fix srd are not cubic enough");
    if (me == 0)
      error->warning("SRD bins for fix srd are not cubic enough");
  }

  tolflag = 0;
  if (binsize3x/gridsrd > 1.0+cubictol || gridsrd/binsize3x > 1.0+cubictol)
    tolflag = 1;
  if (binsize3y/gridsrd > 1.0+cubictol || gridsrd/binsize3y > 1.0+cubictol)
    tolflag = 1;
  if (dimension == 3) {
    if (binsize3z/gridsrd > 1.0+cubictol || gridsrd/binsize3z > 1.0+cubictol)
      tolflag = 1;
  }

  if (tolflag) {
    if (cubicflag == CUBIC_ERROR)
      error->all("SRD bin size for fix srd differs from user request");
    if (me == 0)
      error->warning("SRD bin size for fix srd differs from user request");
  }

  // error if lamda < 0.6 of SRD grid size and no shifting allowed
  // turn on shifting in this case if allowed

  double maxgridsrd = MAX(binsize3x,binsize3y);
  if (dimension == 3) maxgridsrd = MAX(maxgridsrd,binsize3z);

  shiftflag = 0;
  if (lamda < 0.6*maxgridsrd && shiftuser == SHIFT_NO)
    error->all("Fix srd lamda must be >= 0.6 of SRD grid size");
  else if (lamda < 0.6*maxgridsrd && shiftuser == SHIFT_POSSIBLE) {
    shiftflag = 1;
    if (me == 0) 
      error->warning("SRD bin shifting turned on due to small lamda");
  } else if (shiftuser == SHIFT_YES)
    shiftflag = 1;

  // warnings

  if (bigexist && maxgridsrd > 0.25 * minbigdiam && me == 0)
    error->warning("Fix srd grid size > 1/4 of big particle diameter");
  if (viscosity < 0.0 && me == 0)
    error->warning("Fix srd viscosity < 0.0 due to low SRD density");
  if (bigexist && dt_big*vmax > minbigdiam && me == 0)
    error->warning("Fix srd particles may move > big particle diameter");
}

/* ----------------------------------------------------------------------
   set static parameters of each big particle, owned and ghost
   called each reneighboring
   use radfactor in distance parameters
------------------------------------------------------------------------- */

void FixSRD::big_static()
{
  int i;
  double rad,arad,brad,crad;

  double *radius = atom->radius;
  double **shape = atom->shape;
  int *type = atom->type;

  int omega_flag = atom->omega_flag;

  double skinhalf = 0.5 * neighbor->skin;

  for (int k = 0; k < nbig; k++) {
    i = biglist[k].index;
    if (radius) {
      biglist[k].type = SPHERE;
      biglist[k].typesphere = SPHERE_RADIUS;
      biglist[k].typeangular = ANGULAR_OMEGA;
      rad = radfactor*radius[i];
      biglist[k].radius = rad;
      biglist[k].radsq = rad*rad;
      biglist[k].cutbinsq = (rad+skinhalf) * (rad+skinhalf);
    } else if (shape[type[i]][0] == shape[type[i]][1] && 
	       shape[type[i]][0] == shape[type[i]][2]) {
      biglist[k].type = SPHERE;
      biglist[k].typesphere = SPHERE_SHAPE;

      // either atom->omega is defined or atom->angmom, cannot both be defined

      if (omega_flag) biglist[k].typeangular = ANGULAR_OMEGA;
      else biglist[k].typeangular = ANGULAR_ANGMOM;

      rad = radfactor*shape[type[i]][0];
      biglist[k].radsq = rad*rad;
      biglist[k].cutbinsq = (rad+skinhalf) * (rad+skinhalf);
    } else {
      biglist[k].type = ELLIPSOID;
      biglist[k].typeangular = ANGULAR_ANGMOM;
      arad = radfactor*shape[type[i]][0];
      brad = radfactor*shape[type[i]][1];
      crad = radfactor*shape[type[i]][2];
      biglist[k].aradsqinv = 1.0/(arad*arad);
      biglist[k].bradsqinv = 1.0/(brad*brad);
      biglist[k].cradsqinv = 1.0/(crad*crad);
      rad = MAX(arad,brad);
      rad = MAX(rad,crad);
      biglist[k].cutbinsq = (rad+skinhalf) * (rad+skinhalf);
    }
  }
}

/* ----------------------------------------------------------------------
   set dynamics parameters of each big particle, owned and ghost
   for ELLIPSOID, need current omega and ex,ey,ez
   for SPHERE, need current omega from atom->omega or atom->angmom
   called each timestep
------------------------------------------------------------------------- */

void FixSRD::big_dynamic()
{
  int i,itype;
  double inertia;

  double **omega = atom->omega;
  double **angmom = atom->angmom;
  double **quat = atom->quat;
  double **shape = atom->shape;
  double *mass = atom->mass;
  int *type = atom->type;

  for (int k = 0; k < nbig; k++) {
    i = biglist[k].index;

    // ellipsoid with angmom
    // calculate ex,ey,ez and omega from quaternion and angmom

    if (biglist[k].type == ELLIPSOID) {
      exyz_from_q(quat[i],biglist[k].ex,biglist[k].ey,biglist[k].ez);
      omega_from_mq(angmom[i],biglist[k].ex,biglist[k].ey,biglist[k].ez,
		    mass[type[i]],shape[type[i]],biglist[k].omega);

    // sphere with omega and shape or radius
    // set omega from atom->omega directly

    } else if (biglist[k].typeangular == ANGULAR_OMEGA) {
      biglist[k].omega[0] = omega[i][0];
      biglist[k].omega[1] = omega[i][1];
      biglist[k].omega[2] = omega[i][2];

    // sphere with angmom and shape
    // calculate omega from angmom

    } else if (biglist[k].typeangular == ANGULAR_ANGMOM) {
      itype = type[i];
      inertia = INERTIA * mass[itype]*shape[itype][0]*shape[itype][0];
      biglist[k].omega[0] = angmom[i][0] / inertia;
      biglist[k].omega[1] = angmom[i][1] / inertia;
      biglist[k].omega[2] = angmom[i][2] / inertia;
    }
  }
}

/* ----------------------------------------------------------------------
   setup bins used for binning SRD particles for velocity reset
   gridsrd = desired bin size
   also setup bin shifting parameters
   also setup comm of bins that straddle processor boundaries
   called at beginning of each run
   called every reneighbor if box size changes, but not if box shape changes
------------------------------------------------------------------------- */

void FixSRD::setup_velocity_bins()
{
  // require integer # of bins across global domain

  nbin1x = static_cast<int> (domain->xprd/gridsrd + 0.5);
  nbin1y = static_cast<int> (domain->yprd/gridsrd + 0.5);
  nbin1z = static_cast<int> (domain->zprd/gridsrd + 0.5);
  if (dimension == 2) nbin1z = 1;

  if (nbin1x == 0) nbin1x = 1;
  if (nbin1y == 0) nbin1y = 1;
  if (nbin1z == 0) nbin1z = 1;

  if (triclinic == 0) {
    binsize1x = domain->xprd / nbin1x;
    binsize1y = domain->yprd / nbin1y;
    binsize1z = domain->zprd / nbin1z;
    bininv1x = 1.0/binsize1x;
    bininv1y = 1.0/binsize1y;
    bininv1z = 1.0/binsize1z;
  } else {
    binsize1x = 1.0 / nbin1x;
    binsize1y = 1.0 / nbin1y;
    binsize1z = 1.0 / nbin1z;
    bininv1x = nbin1x;
    bininv1y = nbin1y;
    bininv1z = nbin1z;
  }

  nbins1 = nbin1x*nbin1y*nbin1z;

  // setup two shifts, 0 = no shift, 1 = shift
  // initialize no shift case since static
  // shift case is dynamic, has to be initialized each time shift occurs
  // setup_velocity_shift allocates memory for vbin and sendlist/recvlist

  double *boxlo;
  if (triclinic == 0) boxlo = domain->boxlo;
  else boxlo = domain->boxlo_lamda;

  shifts[0].corner[0] = boxlo[0];
  shifts[0].corner[1] = boxlo[1];
  shifts[0].corner[2] = boxlo[2];
  setup_velocity_shift(0,0);

  shifts[1].corner[0] = boxlo[0];
  shifts[1].corner[1] = boxlo[1];
  shifts[1].corner[2] = boxlo[2];
  setup_velocity_shift(1,0);

  // allocate binhead based on max # of bins in either shift

  int max = shifts[0].nbins;
  max = MAX(max,shifts[1].nbins);

  if (max > maxbin1) {
    memory->sfree(binhead);
    maxbin1 = max;
    binhead = (int *) memory->smalloc(max*sizeof(int),"fix/srd:binhead");
  }

  // allocate sbuf,rbuf based on biggest bin message

  max = 0;
  for (int ishift = 0; ishift < 2; ishift++)
    for (int iswap = 0; iswap < 2*dimension; iswap++) {
      max = MAX(max,shifts[ishift].bcomm[iswap].nsend);
      max = MAX(max,shifts[ishift].bcomm[iswap].nrecv);
    }

  if (max > maxbuf) {
    memory->sfree(sbuf1);
    memory->sfree(sbuf2);
    memory->sfree(rbuf1);
    memory->sfree(rbuf2);
    maxbuf = max;
    sbuf1 = (double *) 
      memory->smalloc(max*VBINSIZE*sizeof(double),"fix/srd:sbuf");
    sbuf2 = (double *) 
      memory->smalloc(max*VBINSIZE*sizeof(double),"fix/srd:sbuf");
    rbuf1 = (double *) 
      memory->smalloc(max*VBINSIZE*sizeof(double),"fix/srd:rbuf");
    rbuf2 = (double *) 
      memory->smalloc(max*VBINSIZE*sizeof(double),"fix/srd:rbuf");
  }

  // commflag = 1 if any comm required due to bins overlapping proc boundaries

  shifts[0].commflag = 0;
  if (nbin1x % comm->procgrid[0]) shifts[0].commflag = 1;
  if (nbin1y % comm->procgrid[1]) shifts[0].commflag = 1;
  if (nbin1z % comm->procgrid[2]) shifts[0].commflag = 1;
  shifts[1].commflag = 1;
}

/* ----------------------------------------------------------------------
   setup velocity shift parameters
   set binlo[]/binhi[] and nbins,nbinx,nbiny,nbinz for this proc
   set bcomm[6] params based on bin overlaps with proc boundaries
   set vbin owner flags for bins I am owner of
   ishift = 0, dynamic = 0:
     set all settings since are static
     allocate and set bcomm params and vbins
     carefully check for bins that align with proc boundaries
   ishift = 1, dynamic = 0:
     set max bounds on bin counts and message sizes
     allocate and set bcomm params and vbins based on max bounds
     (settings will later change dynamically)
   ishift = 1, dynamic = 1:
     set actual bin bounds and counts for specific shift
     set bcomm params and vbins (already allocated)
   called by setup_velocity_bins() and reset_velocities()
------------------------------------------------------------------------- */

void FixSRD::setup_velocity_shift(int ishift, int dynamic)
{
  int i,j,k,m,id,nsend;
  int *sendlist;
  BinComm *first,*second;
  BinAve *vbin;

  double *sublo,*subhi;
  if (triclinic == 0) {
    sublo = domain->sublo;
    subhi = domain->subhi;
  } else {
    sublo = domain->sublo_lamda;
    subhi = domain->subhi_lamda;
  }

  int *binlo = shifts[ishift].binlo;
  int *binhi = shifts[ishift].binhi;
  double *corner = shifts[ishift].corner;
  int *procgrid = comm->procgrid;
  int *myloc = comm->myloc;
  
  binlo[0] = static_cast<int> ((sublo[0]-corner[0])*bininv1x);
  binlo[1] = static_cast<int> ((sublo[1]-corner[1])*bininv1y);
  binlo[2] = static_cast<int> ((sublo[2]-corner[2])*bininv1z);
  if (dimension == 2) shifts[ishift].binlo[2] = 0;
  
  binhi[0] = static_cast<int> ((subhi[0]-corner[0])*bininv1x);
  binhi[1] = static_cast<int> ((subhi[1]-corner[1])*bininv1y);
  binhi[2] = static_cast<int> ((subhi[2]-corner[2])*bininv1z);
  if (dimension == 2) shifts[ishift].binhi[2] = 0;

  if (ishift == 0) {
    if (myloc[0]*nbin1x % procgrid[0] == 0)
      binlo[0] = myloc[0]*nbin1x/procgrid[0];
    if (myloc[1]*nbin1y % procgrid[1] == 0)
      binlo[1] = myloc[1]*nbin1y/procgrid[1];
    if (myloc[2]*nbin1z % procgrid[2] == 0)
      binlo[2] = myloc[2]*nbin1z/procgrid[2];
    
    if ((myloc[0]+1)*nbin1x % procgrid[0] == 0)
      binhi[0] = (myloc[0]+1)*nbin1x/procgrid[0] - 1;
    if ((myloc[1]+1)*nbin1y % procgrid[1] == 0)
      binhi[1] = (myloc[1]+1)*nbin1y/procgrid[1] - 1;
    if ((myloc[2]+1)*nbin1z % procgrid[2] == 0)
      binhi[2] = (myloc[2]+1)*nbin1z/procgrid[2] - 1;
  }
  
  int nbinx = binhi[0] - binlo[0] + 1;
  int nbiny = binhi[1] - binlo[1] + 1;
  int nbinz = binhi[2] - binlo[2] + 1;

  // allow for one extra bin if shifting will occur
  
  if (ishift == 1 && dynamic == 0) {
    nbinx++;
    nbiny++;
    if (dimension == 3) nbinz++;
  }
  
  int nbins = nbinx*nbiny*nbinz;
  int nbinxy = nbinx*nbiny;
  int nbinsq = nbinx*nbiny;
  nbinsq = MAX(nbiny*nbinz,nbinsq);
  nbinsq = MAX(nbinx*nbinz,nbinsq);
  
  shifts[ishift].nbins = nbins;
  shifts[ishift].nbinx = nbinx;
  shifts[ishift].nbiny = nbiny;
  shifts[ishift].nbinz = nbinz;

  int reallocflag = 0;
  if (dynamic == 0 && nbinsq > shifts[ishift].maxbinsq) {
    shifts[ishift].maxbinsq = nbinsq;
    reallocflag = 1;
  }

  // bcomm neighbors
  // first = send in lo direction, recv from hi direction
  // second = send in hi direction, recv from lo direction
  
  if (dynamic == 0) {
    shifts[ishift].bcomm[0].sendproc = comm->procneigh[0][0];
    shifts[ishift].bcomm[0].recvproc = comm->procneigh[0][1];
    shifts[ishift].bcomm[1].sendproc = comm->procneigh[0][1];
    shifts[ishift].bcomm[1].recvproc = comm->procneigh[0][0];
    shifts[ishift].bcomm[2].sendproc = comm->procneigh[1][0];
    shifts[ishift].bcomm[2].recvproc = comm->procneigh[1][1];
    shifts[ishift].bcomm[3].sendproc = comm->procneigh[1][1];
    shifts[ishift].bcomm[3].recvproc = comm->procneigh[1][0];
    shifts[ishift].bcomm[4].sendproc = comm->procneigh[2][0];
    shifts[ishift].bcomm[4].recvproc = comm->procneigh[2][1];
    shifts[ishift].bcomm[5].sendproc = comm->procneigh[2][1];
    shifts[ishift].bcomm[5].recvproc = comm->procneigh[2][0];
  }
  
  // set nsend,nrecv and sendlist,recvlist for each swap in x,y,z
  // allocate sendlist,recvlist only for dynamic = 0
  
  first = &shifts[ishift].bcomm[0];
  second = &shifts[ishift].bcomm[1];
  
  first->nsend = first->nrecv = second->nsend = second->nrecv = nbiny*nbinz;
  if (ishift == 0) {
    if (myloc[0]*nbin1x % procgrid[0] == 0)
      first->nsend = second->nrecv = 0;
    if ((myloc[0]+1)*nbin1x % procgrid[0] == 0)
      second->nsend = first->nrecv = 0;
  }
  
  if (reallocflag) {
    memory->sfree(first->sendlist);
    memory->sfree(first->recvlist);
    memory->sfree(second->sendlist);
    memory->sfree(second->recvlist);
    first->sendlist = (int *) 
      memory->smalloc(nbinsq*sizeof(int),"fix/srd:sendlist");
    first->recvlist = (int *) 
      memory->smalloc(nbinsq*sizeof(int),"fix/srd:sendlist");
    second->sendlist = (int *) 
      memory->smalloc(nbinsq*sizeof(int),"fix/srd:sendlist");
    second->recvlist = (int *) 
      memory->smalloc(nbinsq*sizeof(int),"fix/srd:sendlist");
  }

  m = 0;
  i = 0;
  for (j = 0; j < nbiny; j++) 
    for (k = 0; k < nbinz; k++) {
      id = k*nbinxy + j*nbinx + i;
      first->sendlist[m] = second->recvlist[m] = id;
      m++;
    }
  m = 0;
  i = nbinx-1;
  for (j = 0; j < nbiny; j++) 
    for (k = 0; k < nbinz; k++) {
      id = k*nbinxy + j*nbinx + i;
      second->sendlist[m] = first->recvlist[m] = id;
      m++;
    }
  
  first = &shifts[ishift].bcomm[2];
  second = &shifts[ishift].bcomm[3];
  
  first->nsend = first->nrecv = second->nsend = second->nrecv = nbinx*nbinz;
  if (ishift == 0) {
    if (myloc[1]*nbin1y % procgrid[1] == 0)
      first->nsend = second->nrecv = 0;
    if ((myloc[1]+1)*nbin1y % procgrid[1] == 0)
      second->nsend = first->nrecv = 0;
  }
  
  if (reallocflag) {
    memory->sfree(first->sendlist);
    memory->sfree(first->recvlist);
    memory->sfree(second->sendlist);
    memory->sfree(second->recvlist);
    first->sendlist = (int *) 
      memory->smalloc(nbinsq*sizeof(int),"fix/srd:sendlist");
    first->recvlist = (int *) 
      memory->smalloc(nbinsq*sizeof(int),"fix/srd:sendlist");
    second->sendlist = (int *) 
      memory->smalloc(nbinsq*sizeof(int),"fix/srd:sendlist");
    second->recvlist = (int *) 
      memory->smalloc(nbinsq*sizeof(int),"fix/srd:sendlist");
  }
  
  m = 0;
  j = 0;
  for (i = 0; i < nbinx; i++) 
    for (k = 0; k < nbinz; k++) {
      id = k*nbinxy + j*nbinx + i;
      first->sendlist[m] = second->recvlist[m] = id;
      m++;
    }
  m = 0;
  j = nbiny-1;
  for (i = 0; i < nbinx; i++) 
    for (k = 0; k < nbinz; k++) {
      id = k*nbinxy + j*nbinx + i;
      second->sendlist[m] = first->recvlist[m] = id;
      m++;
    }
  
  if (dimension == 3) {
    first = &shifts[ishift].bcomm[4];
    second = &shifts[ishift].bcomm[5];
    
    first->nsend = first->nrecv = second->nsend = second->nrecv = nbinx*nbiny;
    if (ishift == 0) {
      if (myloc[2]*nbin1z % procgrid[2] == 0)
	first->nsend = second->nrecv = 0;
      if ((myloc[2]+1)*nbin1z % procgrid[2] == 0)
	second->nsend = first->nrecv = 0;
    }
    
    if (reallocflag) {
      memory->sfree(first->sendlist);
      memory->sfree(first->recvlist);
      memory->sfree(second->sendlist);
      memory->sfree(second->recvlist);
      first->sendlist = (int *) 
	memory->smalloc(nbinx*nbiny*sizeof(int),"fix/srd:sendlist");
      first->recvlist = (int *) 
	memory->smalloc(nbinx*nbiny*sizeof(int),"fix/srd:sendlist");
      second->sendlist = (int *) 
	memory->smalloc(nbinx*nbiny*sizeof(int),"fix/srd:sendlist");
      second->recvlist = (int *) 
	memory->smalloc(nbinx*nbiny*sizeof(int),"fix/srd:sendlist");
    }
    
    m = 0;
    k = 0;
    for (i = 0; i < nbinx; i++) 
      for (j = 0; j < nbiny; j++) {
	id = k*nbinxy + j*nbinx + i;
	first->sendlist[m] = second->recvlist[m] = id;
	m++;
      }
    m = 0;
    k = nbinz-1;
    for (i = 0; i < nbinx; i++) 
      for (j = 0; j < nbiny; j++) {
	id = k*nbinxy + j*nbinx + i;
	second->sendlist[m] = first->recvlist[m] = id;
	m++;
      }
  }
  
  // allocate vbins, only for dynamic = 0
  
  if (dynamic == 0 && nbins > shifts[ishift].maxvbin) {
    memory->sfree(shifts[ishift].vbin);
    shifts[ishift].maxvbin = nbins;
    shifts[ishift].vbin = (BinAve *) 
      memory->smalloc(nbins*sizeof(BinAve),"fix/srd:vbin");
  }
  
  // for vbins I own, set owner = 1
  // if bin never sent to anyone, I own it
  // if bin sent to lower numbered proc, I do not own it
  // if bin sent to self, I do not own it on even swap (avoids double counting)
  
  vbin = shifts[ishift].vbin;
  for (i = 0; i < nbins; i++) vbin[i].owner = 1;
  for (int iswap = 0; iswap < 2*dimension; iswap++) {
    if (shifts[ishift].bcomm[iswap].sendproc > me) continue;
    if (shifts[ishift].bcomm[iswap].sendproc == me && iswap % 2 == 0) continue;
    nsend = shifts[ishift].bcomm[iswap].nsend;
    sendlist = shifts[ishift].bcomm[iswap].sendlist;
    for (m = 0; m < nsend; m++) vbin[sendlist[m]].owner = 0;
  }

  // if triclinic and streamflag:
  // set xctr (in lamda units) for all nbins so can compute bin vstream

  if (triclinic && streamflag) {
    m = 0;
    for (k = 0; k < nbinz; k++)
      for (j = 0; j < nbiny; j++)
	for (i = 0; i < nbinx; i++) {
	  vbin[m].xctr[0] = corner[0] + (i+binlo[0]+0.5)/nbin1x;
	  vbin[m].xctr[1] = corner[1] + (j+binlo[1]+0.5)/nbin1y;
	  vbin[m].xctr[2] = corner[2] + (k+binlo[2]+0.5)/nbin1z;
	  m++;
	}
  }
}

/* ----------------------------------------------------------------------
   setup bins used for big and SRD particle searching
   gridsearch = desired bin size
   bins are orthogonal whether simulation box is orthogonal or triclinic
   for orthogonal boxes, called at each setup since cutghost may change
   for triclinic boxes, called at every reneigh, since tilt may change
   sets the following:
     nbin2 xyz = # of bins in each dim
     binsize2 and bininv2 xyz = size of bins in each dim
     xyz blo2 = origin of bins
------------------------------------------------------------------------- */

void FixSRD::setup_search_bins()
{
  // subboxlo/hi = real space bbox which owned/ghost big particles can be in
  // start with bounding box for my sub-domain
  // add dist_bigghost = ghost cutoff + skin/2
  // for triclinic, need to:
  //   a) convert dist_bigghost to lamda space via length012
  //   b) lo/hi = sub-domain big particle bbox in lamda space
  //   c) convert lo/hi to real space bounding box via domain->bbox()
  //   similar to neighbor::setup_bins() and comm::cutghost[] calculation

  double subboxlo[3],subboxhi[3];

  if (triclinic == 0) {
    subboxlo[0] = domain->sublo[0] - dist_bigghost;
    subboxlo[1] = domain->sublo[1] - dist_bigghost;
    subboxlo[2] = domain->sublo[2] - dist_bigghost;
    subboxhi[0] = domain->subhi[0] + dist_bigghost;
    subboxhi[1] = domain->subhi[1] + dist_bigghost;
    subboxhi[2] = domain->subhi[2] + dist_bigghost;
  } else {
    double *h_inv = domain->h_inv;
    double length0,length1,length2;
    length0 = sqrt(h_inv[0]*h_inv[0] + h_inv[5]*h_inv[5] + h_inv[4]*h_inv[4]);
    length1 = sqrt(h_inv[1]*h_inv[1] + h_inv[3]*h_inv[3]);
    length2 = h_inv[2];
    double lo[3],hi[3];
    lo[0] = domain->sublo_lamda[0] - dist_bigghost*length0;
    lo[1] = domain->sublo_lamda[1] - dist_bigghost*length1;
    lo[2] = domain->sublo_lamda[2] - dist_bigghost*length2;
    hi[0] = domain->subhi_lamda[0] + dist_bigghost*length0;
    hi[1] = domain->subhi_lamda[1] + dist_bigghost*length1;
    hi[2] = domain->subhi_lamda[2] + dist_bigghost*length2;
    domain->bbox(lo,hi,subboxlo,subboxhi);
  }

  // require integer # of bins for that volume

  nbin2x = static_cast<int> ((subboxhi[0] - subboxlo[0]) / gridsearch);
  nbin2y = static_cast<int> ((subboxhi[1] - subboxlo[1]) / gridsearch);
  nbin2z = static_cast<int> ((subboxhi[2] - subboxlo[2]) / gridsearch);
  if (dimension == 2) nbin2z = 1;

  if (nbin2x == 0) nbin2x = 1;
  if (nbin2y == 0) nbin2y = 1;
  if (nbin2z == 0) nbin2z = 1;

  binsize2x = (subboxhi[0] - subboxlo[0]) / nbin2x;
  binsize2y = (subboxhi[1] - subboxlo[1]) / nbin2y;
  binsize2z = (subboxhi[2] - subboxlo[2]) / nbin2z;
  bininv2x = 1.0/binsize2x;
  bininv2y = 1.0/binsize2y;
  bininv2z = 1.0/binsize2z;

  // add bins on either end due to extent of big particles
  // radmax = max distance from central bin that biggest particle overlaps
  // includes skin movement
  // nx,ny,nz = max # of bins to search away from central bin

  double radmax = 0.5*maxbigdiam + 0.5*neighbor->skin;

  int nx = static_cast<int> (radmax/binsize2x) + 1;
  int ny = static_cast<int> (radmax/binsize2y) + 1;
  int nz = static_cast<int> (radmax/binsize2z) + 1;
  if (dimension == 2) nz = 0;

  nbin2x += 2*nx;
  nbin2y += 2*ny;
  nbin2z += 2*nz;

  xblo2 = subboxlo[0] - nx*binsize2x;
  yblo2 = subboxlo[1] - ny*binsize2y;
  zblo2 = subboxlo[2] - nz*binsize2z;
  if (dimension == 2) zblo2 = domain->boxlo[2];

  // allocate bins
  // first deallocate previous bins if necessary

  nbins2 = nbin2x*nbin2y*nbin2z;
  if (nbins2 > maxbin2) {
    memory->sfree(nbinbig);
    memory->destroy_2d_int_array(binbig);
    maxbin2 = nbins2;
    nbinbig = (int *) memory->smalloc(nbins2*sizeof(int),"fix/srd:nbinbig");
    binbig = memory->create_2d_int_array(nbins2,ATOMPERBIN,"fix/srd:binbig");
  }
}

/* ----------------------------------------------------------------------
   compute stencil of bin offsets for a big particle overlapping search bins
------------------------------------------------------------------------- */

void FixSRD::setup_search_stencil()
{
  // radmax = max distance from central bin that any big particle overlaps
  // includes skin movement
  // nx,ny,nz = max # of bins to search away from central bin

  double radmax = 0.5*maxbigdiam + 0.5*neighbor->skin;
  double radsq = radmax*radmax;

  int nx = static_cast<int> (radmax/binsize2x) + 1;
  int ny = static_cast<int> (radmax/binsize2y) + 1;
  int nz = static_cast<int> (radmax/binsize2z) + 1;
  if (dimension == 2) nz = 0;

  // allocate stencil array
  // deallocate previous stencil if necessary

  int max = (2*nx+1) * (2*ny+1) * (2*nz+1);
  if (max > maxstencil) {
    memory->destroy_2d_int_array(stencil);
    maxstencil = max;
    stencil = memory->create_2d_int_array(max,4,"fix/srd:stencil");
  }

  // loop over all bins
  // add bin to stencil:
  // if distance of closest corners of bin and central bin is within cutoff

  nstencil = 0;
  for (int k = -nz; k <= nz; k++)
    for (int j = -ny; j <= ny; j++)
      for (int i = -nx; i <= nx; i++)
	if (bin_bin_distance(i,j,k) < radsq) {
	  stencil[nstencil][0] = i;
	  stencil[nstencil][1] = j;
	  stencil[nstencil][2] = k;
	  stencil[nstencil][3] = k*nbin2y*nbin2x + j*nbin2x + i;
	  nstencil++;
	}
}

/* ----------------------------------------------------------------------
   compute closest squared distance between point x and bin ibin
------------------------------------------------------------------------- */

double FixSRD::point_bin_distance(double *x, int i, int j, int k)
{
  double delx,dely,delz;

  double xlo = xblo2 + i*binsize2x;
  double xhi = xlo + binsize2x;
  double ylo = yblo2 + j*binsize2y;
  double yhi = ylo + binsize2y;
  double zlo = zblo2 + k*binsize2z;
  double zhi = zlo + binsize2z;

  if (x[0] < xlo) delx = xlo - x[0];
  else if (x[0] > xhi) delx = x[0] - xhi;
  else delx = 0.0;

  if (x[1] < ylo) dely = ylo - x[1];
  else if (x[1] > yhi) dely = x[1] - yhi;
  else dely = 0.0;

  if (x[2] < zlo) delz = zlo - x[2];
  else if (x[2] > zhi) delz = x[2] - zhi;
  else delz = 0.0;

  return (delx*delx + dely*dely + delz*delz);
}

/* ----------------------------------------------------------------------
   compute closest squared distance between 2 bins
   central bin (0,0,0) and bin (i,j,k)
------------------------------------------------------------------------- */

double FixSRD::bin_bin_distance(int i, int j, int k)
{
  double delx,dely,delz;

  if (i > 0) delx = (i-1)*binsize2x;
  else if (i == 0) delx = 0.0;
  else delx = (i+1)*binsize2x;

  if (j > 0) dely = (j-1)*binsize2y;
  else if (j == 0) dely = 0.0;
  else dely = (j+1)*binsize2y;

  if (k > 0) delz = (k-1)*binsize2z;
  else if (k == 0) delz = 0.0;
  else delz = (k+1)*binsize2z;

  return (delx*delx + dely*dely + delz*delz);
}

/* ----------------------------------------------------------------------
   compute space-frame ex,ey,ez from current quaternion q
   ex,ey,ez = space-frame coords of 1st,2nd,3rd principal axis
   operation is ex = q' d q = Q d, where d is (1,0,0) = 1st axis in body frame
------------------------------------------------------------------------- */

void FixSRD::exyz_from_q(double *q, double *ex, double *ey, double *ez)
{
  ex[0] = q[0]*q[0] + q[1]*q[1] - q[2]*q[2] - q[3]*q[3];
  ex[1] = 2.0 * (q[1]*q[2] + q[0]*q[3]);
  ex[2] = 2.0 * (q[1]*q[3] - q[0]*q[2]);

  ey[0] = 2.0 * (q[1]*q[2] - q[0]*q[3]);
  ey[1] = q[0]*q[0] - q[1]*q[1] + q[2]*q[2] - q[3]*q[3];
  ey[2] = 2.0 * (q[2]*q[3] + q[0]*q[1]);

  ez[0] = 2.0 * (q[1]*q[3] + q[0]*q[2]);
  ez[1] = 2.0 * (q[2]*q[3] - q[0]*q[1]);
  ez[2] = q[0]*q[0] - q[1]*q[1] - q[2]*q[2] + q[3]*q[3];
}

/* ----------------------------------------------------------------------
   compute omega from angular momentum
   w = omega = angular velocity in space frame
   wbody = angular velocity in body frame
   set wbody component to 0.0 if inertia component is 0.0
   otherwise body can spin easily around that axis
   project space-frame angular momentum onto body axes
   and divide by principal moments
------------------------------------------------------------------------- */

void FixSRD::omega_from_mq(double *m, double *ex, double *ey, double *ez,
			   double mass, double *shape, double *w)
{
  double inertia[3],wbody[3];

  inertia[0] = 0.2*mass * (shape[1]*shape[1]+shape[2]*shape[2]);
  inertia[1] = 0.2*mass * (shape[0]*shape[0]+shape[2]*shape[2]);
  inertia[2] = 0.2*mass * (shape[0]*shape[0]+shape[1]*shape[1]);

  wbody[0] = (m[0]*ex[0] + m[1]*ex[1] + m[2]*ex[2]) / inertia[0];
  wbody[1] = (m[0]*ey[0] + m[1]*ey[1] + m[2]*ey[2]) / inertia[1];
  wbody[2] = (m[0]*ez[0] + m[1]*ez[1] + m[2]*ez[2]) / inertia[2];

  w[0] = wbody[0]*ex[0] + wbody[1]*ey[0] + wbody[2]*ez[0];
  w[1] = wbody[0]*ex[1] + wbody[1]*ey[1] + wbody[2]*ez[1];
  w[2] = wbody[0]*ex[2] + wbody[1]*ey[2] + wbody[2]*ez[2];
}

/* ---------------------------------------------------------------------- */

int FixSRD::pack_reverse_comm(int n, int first, double *buf)
{
  int i,m,last;

  m = 0;
  last = first + n;

  if (torqueflag) {
    for (i = first; i < last; i++) {
      buf[m++] = flocal[i][0];
      buf[m++] = flocal[i][1];
      buf[m++] = flocal[i][2];
      buf[m++] = tlocal[i][0];
      buf[m++] = tlocal[i][1];
      buf[m++] = tlocal[i][2];
    }

  } else {
    for (i = first; i < last; i++) {
      buf[m++] = flocal[i][0];
      buf[m++] = flocal[i][1];
      buf[m++] = flocal[i][2];
    }
  }

  return comm_reverse;
}

/* ---------------------------------------------------------------------- */

void FixSRD::unpack_reverse_comm(int n, int *list, double *buf)
{
  int i,j,m;

  m = 0;

  if (torqueflag) {
    for (i = 0; i < n; i++) {
      j = list[i];
      flocal[j][0] += buf[m++];
      flocal[j][1] += buf[m++];
      flocal[j][2] += buf[m++];
      tlocal[j][0] += buf[m++];
      tlocal[j][1] += buf[m++];
      tlocal[j][2] += buf[m++];
    }

  } else {
    for (i = 0; i < n; i++) {
      j = list[i];
      flocal[j][0] += buf[m++];
      flocal[j][1] += buf[m++];
      flocal[j][2] += buf[m++];
    }
  }
}

/* ----------------------------------------------------------------------
   SRD stats
------------------------------------------------------------------------- */

double FixSRD::compute_vector(int n)
{
  // only sum across procs one time

  if (stats_flag == 0) {
    stats[0] = ncheck;
    stats[1] = ncollide;
    stats[2] = nbounce;
    stats[3] = ninside;
    stats[4] = nrescale;
    stats[5] = nbins2;
    stats[6] = nbins1;
    stats[7] = srd_bin_count;
    stats[8] = srd_bin_temp;
    stats[9] = bouncemaxnum;
    stats[10] = bouncemax;
    stats[11] = reneighcount;

    MPI_Allreduce(stats,stats_all,10,MPI_DOUBLE,MPI_SUM,world);
    MPI_Allreduce(&stats[10],&stats_all[10],1,MPI_DOUBLE,MPI_MAX,world);
    if (stats_all[7] != 0.0) stats_all[8] /= stats_all[7];
    stats_all[6] /= nprocs;

    stats_flag = 1;
  }

  return stats_all[n];
}

/* ---------------------------------------------------------------------- */

void FixSRD::velocity_stats(int groupnum)
{
  int bitmask = group->bitmask[groupnum];

  double **v = atom->v;
  int *mask = atom->mask;
  int nlocal = atom->nlocal;

  double vone;
  double vave = 0.0;
  double vmax = 0.0;

  for (int i = 0; i < nlocal; i++)
    if (mask[i] & bitmask) {
      vone = sqrt(v[i][0]*v[i][0] + v[i][1]*v[i][1] + v[i][2]*v[i][2]);
      vave += vone;
      if (vone > vmax) vmax = vone;
    }

  double all;
  MPI_Allreduce(&vave,&all,1,MPI_DOUBLE,MPI_SUM,world);
  double count = group->count(groupnum);
  if (count != 0.0) vave = all/count;
  else vave = 0.0;

  MPI_Allreduce(&vmax,&all,1,MPI_DOUBLE,MPI_MAX,world);
  vmax = all;

  if (me == 0) {
    if (screen)
      fprintf(screen,"  ave/max %s velocity = %g %g\n",
	      group->names[groupnum],vave,vmax);
    if (logfile)
      fprintf(logfile,"  ave/max %s velocity = %g %g\n",
	      group->names[groupnum],vave,vmax);
  }
}

/* ---------------------------------------------------------------------- */

double FixSRD::memory_usage()
{
  double bytes = 0.0;
  bytes += (shifts[0].nbins + shifts[1].nbins) * sizeof(BinAve);
  bytes += nmax * sizeof(int);
  if (bigexist) {
    bytes += nbins2 * sizeof(int);
    bytes += nbins2*ATOMPERBIN * sizeof(int);
  }
  bytes += nmax * sizeof(int);
  return bytes;
}

/* ----------------------------------------------------------------------
   useful debugging functions
------------------------------------------------------------------------- */

double FixSRD::distance(int i, int j)
{
  double dx = atom->x[i][0] - atom->x[j][0];
  double dy = atom->x[i][1] - atom->x[j][1];
  double dz = atom->x[i][2] - atom->x[j][2];
  return sqrt(dx*dx + dy*dy + dz*dz);
}

/* ---------------------------------------------------------------------- */

void FixSRD::print_collision(int i, int j, int ibounce,
			     double t_remain, double dt,
			     double *xscoll, double *xbcoll, double *norm)
{
  double xsstart[3],xbstart[3];
  double **x = atom->x;
  double **v = atom->v;

  printf("COLLISION between SRD %d and BIG %d\n",atom->tag[i],atom->tag[j]);
  printf("  bounce # = %d\n",ibounce+1);
  printf("  local indices: %d %d\n",i,j);
  printf("  timestep = %g\n",dt);
  printf("  time remaining post-collision = %g\n",t_remain);

  xsstart[0] = x[i][0] - dt*v[i][0];
  xsstart[1] = x[i][1] - dt*v[i][1];
  xsstart[2] = x[i][2] - dt*v[i][2];
  xbstart[0] = x[j][0] - dt*v[j][0];
  xbstart[1] = x[j][1] - dt*v[j][1];
  xbstart[2] = x[j][2] - dt*v[j][2];

  printf("  SRD start position = %g %g %g\n",
	 xsstart[0],xsstart[1],xsstart[2]);
  printf("  BIG start position = %g %g %g\n",
	 xbstart[0],xbstart[1],xbstart[2]);
  printf("  SRD coll  position = %g %g %g\n",
	 xscoll[0],xscoll[1],xscoll[2]);
  printf("  BIG coll  position = %g %g %g\n",
	 xbcoll[0],xbcoll[1],xbcoll[2]);
  printf("  SRD end   position = %g %g %g\n",x[i][0],x[i][1],x[i][2]);
  printf("  BIG end   position = %g %g %g\n",x[j][0],x[j][1],x[j][2]);

  printf("  SRD vel = %g %g %g\n",v[i][0],v[i][1],v[i][2]);
  printf("  BIG vel = %g %g %g\n",v[j][0],v[j][1],v[j][2]);
  printf("  surf norm = %g %g %g\n",norm[0],norm[1],norm[2]);

  double rstart = sqrt((xsstart[0]-xbstart[0])*(xsstart[0]-xbstart[0]) +
		       (xsstart[1]-xbstart[1])*(xsstart[1]-xbstart[1]) +
		       (xsstart[2]-xbstart[2])*(xsstart[2]-xbstart[2]));
  double rcoll = sqrt((xscoll[0]-xbcoll[0])*(xscoll[0]-xbcoll[0]) +
		      (xscoll[1]-xbcoll[1])*(xscoll[1]-xbcoll[1]) +
		      (xscoll[2]-xbcoll[2])*(xscoll[2]-xbcoll[2]));
  double rend = sqrt((x[i][0]-x[j][0])*(x[i][0]-x[j][0]) +
		     (x[i][1]-x[j][1])*(x[i][1]-x[j][1]) +
		     (x[i][2]-x[j][2])*(x[i][2]-x[j][2]));

  printf("  separation at start = %g\n",rstart);
  printf("  separation at coll  = %g\n",rcoll);
  printf("  separation at end   = %g\n",rend);
}
