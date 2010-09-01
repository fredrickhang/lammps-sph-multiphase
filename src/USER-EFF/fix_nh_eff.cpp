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
 Contributing author: Andres Jaramillo-Botero (Caltech)
------------------------------------------------------------------------- */

#include "math.h"
#include "fix_nh_eff.h"
#include "atom.h"
#include "atom_vec.h"
#include "group.h"
#include "error.h"

using namespace LAMMPS_NS;

enum{NOBIAS,BIAS};

/* ---------------------------------------------------------------------- */

FixNHEff::FixNHEff(LAMMPS *lmp, int narg, char **arg) : FixNH(lmp, narg, arg)
{
  if (!atom->spin_flag || !atom->eradius_flag || 
      !atom->ervel_flag || !atom->erforce_flag) 
    error->all("Fix nvt/nph/npt eff requires atom attributes "
	       "spin, eradius, ervel, erforce");
}

/* ----------------------------------------------------------------------
   perform half-step update of electron radial velocities 
-----------------------------------------------------------------------*/

void FixNHEff::nve_v()
{
  // standard nve_v velocity update

  FixNH::nve_v();

  double *erforce = atom->erforce;
  double *ervel = atom->ervel;
  double *rmass = atom->rmass;
  double *mass = atom->mass;
  int *spin = atom->spin;
  int *type = atom->type;
  int *mask = atom->mask;
  int nlocal = atom->nlocal;
  if (igroup == atom->firstgroup) nlocal = atom->nfirst;

  // 2 cases depending on rmass vs mass

  int itype;
  double dtfm;

  if (rmass) {
    for (int i = 0; i < nlocal; i++) {
      if (mask[i] & groupbit) {
	if (spin[i]) {
	  dtfm = dtf / rmass[i];
	  ervel[i] = dtfm * erforce[i] / 0.75;
	}
      }
    }
  } else {
    for (int i = 0; i < nlocal; i++) {    
      if (mask[i] & groupbit) {
	if (spin[i]) {
	  dtfm = dtf / mass[type[i]];
	  ervel[i] = dtfm * erforce[i] / 0.75;
	}
      }
    }
  }
}

/* ----------------------------------------------------------------------
   perform full-step update of electron radii
-----------------------------------------------------------------------*/

void FixNHEff::nve_x()
{
  // standard nve_x position update

  FixNH::nve_x();

  double *eradius = atom->eradius;
  double *ervel = atom->ervel;
  int *spin = atom->spin;
  int *mask = atom->mask;
  int nlocal = atom->nlocal;
  if (igroup == atom->firstgroup) nlocal = atom->nfirst;

  for (int i = 0; i < nlocal; i++)
    if (mask[i] & groupbit)
      if (spin[i]) eradius[i] += dtv * ervel[i];
}

/* ----------------------------------------------------------------------
   perform half-step scaling of electron radial velocities
-----------------------------------------------------------------------*/

void FixNHEff::nh_v_temp()
{
  // standard nh_v_temp velocity scaling

  FixNH::nh_v_temp();

  double *ervel = atom->ervel;
  int *spin = atom->spin;
  int *mask = atom->mask;
  int nlocal = atom->nlocal;
  if (igroup == atom->firstgroup) nlocal = atom->nfirst;

  for (int i = 0; i < nlocal; i++)
    if (mask[i] & groupbit)
      if (spin[i]) ervel[i] *= factor_eta;
}
