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
   Contributing author: Loukas D. Peristeras (Scienomics SARL)
   [ based on dihedral_helix.cpp Paul Crozier (SNL) ]
------------------------------------------------------------------------- */

#include <cmath>
#include <cstdlib>
#include "dihedral_gaussian_ljlambda.h"
#include "atom.h"
#include "neighbor.h"
#include "domain.h"
#include "comm.h"
#include "force.h"
#include "pair.h"
#include "update.h"
#include "math_const.h"
#include "memory.h"
#include "error.h"
#include "utils.h"

using namespace LAMMPS_NS;
using namespace MathConst;

#define TOLERANCE 0.05
#define SMALL     0.001
#define SMALLER   0.00001

/* ---------------------------------------------------------------------- */

DihedralGaussianLJLambda::DihedralGaussianLJLambda(LAMMPS *lmp) : Dihedral(lmp) {}

/* ---------------------------------------------------------------------- */

DihedralGaussianLJLambda::~DihedralGaussianLJLambda()
{
  if (allocated) {
    memory->destroy(setflag);
    memory->destroy(epsilon);
    memory->destroy(sigma);
    memory->destroy(lambda);
    memory->destroy(epsdihed);
    memory->destroy(ljm1);
    memory->destroy(ljm2);
    memory->destroy(ljm3);
    memory->destroy(ljm4);
    memory->destroy(ljn1);
    memory->destroy(ljn2);
    memory->destroy(ljn3);
    memory->destroy(ljn4);
  }
}

/* ---------------------------------------------------------------------- */

void DihedralGaussianLJLambda::compute(int eflag, int vflag)
{
  int i1,i2,i3,i4,n,type;
  double vb1x,vb1y,vb1z,vb2x,vb2y,vb2z,vb3x,vb3y,vb3z,vb2xm,vb2ym,vb2zm;
  double edihedral,f1[3],f2[3],f3[3],f4[3];
  double sb1,sb2,sb3,rb1,rb3,c0,b1mag2,b1mag,b2mag2;
  double b2mag,b3mag2,b3mag,ctmp,r12c1,c1mag,r12c2;
  double c2mag,sc1,sc2,s1,s12,c,pp,ppd,a,a11,a22;
  double a33,a12,a13,a23,sx2,sy2,sz2;
  double s2,cx,cy,cz,cmag,dx,phi,si,siinv,sin2;
  double dphia,dphib,dphib2,dphic,dphic2,dphid,dphid2;
  double pa,pb,pb2,pc,pc2,pd,pd2,ea,eb,eb2,ec,ec2,ed,ed2;
  double fea,feb,feb2,fec,fec2,fed,fed2;
  double delx,dely,delz,rsq,r2inv,r6inv;
  double forcelj,fpair,ecoul,evdwl;

  edihedral = evdwl = ecoul = 0.0;
  ev_init(eflag,vflag);

  double **x = atom->x;
  double **f = atom->f;
  int **dihedrallist = neighbor->dihedrallist;
  int ndihedrallist = neighbor->ndihedrallist;
  int nlocal = atom->nlocal;
  int newton_bond = force->newton_bond;

  double ka = 11.4;
  double kb = 0.15;
  double kc = 1.8;
  double kd = 0.65;

  double fa = 0.9;
  double fb = 1.02;
  double fc = -1.55;
  double fd = -2.5;

  double eb0 = 0.27;
  double ec0 = 0.14;
  double ed0 = 0.26;

  for (n = 0; n < ndihedrallist; n++) {
    i1 = dihedrallist[n][0];
    i2 = dihedrallist[n][1];
    i3 = dihedrallist[n][2];
    i4 = dihedrallist[n][3];
    type = dihedrallist[n][4];

    // 1st bond

    vb1x = x[i1][0] - x[i2][0];
    vb1y = x[i1][1] - x[i2][1];
    vb1z = x[i1][2] - x[i2][2];

    // 2nd bond

    vb2x = x[i3][0] - x[i2][0];
    vb2y = x[i3][1] - x[i2][1];
    vb2z = x[i3][2] - x[i2][2];

    vb2xm = -vb2x;
    vb2ym = -vb2y;
    vb2zm = -vb2z;

    // 3rd bond

    vb3x = x[i4][0] - x[i3][0];
    vb3y = x[i4][1] - x[i3][1];
    vb3z = x[i4][2] - x[i3][2];

    // c0 calculation

    sb1 = 1.0 / (vb1x*vb1x + vb1y*vb1y + vb1z*vb1z);
    sb2 = 1.0 / (vb2x*vb2x + vb2y*vb2y + vb2z*vb2z);
    sb3 = 1.0 / (vb3x*vb3x + vb3y*vb3y + vb3z*vb3z);

    rb1 = sqrt(sb1);
    rb3 = sqrt(sb3);

    c0 = (vb1x*vb3x + vb1y*vb3y + vb1z*vb3z) * rb1*rb3;

    // 1st and 2nd angle

    b1mag2 = vb1x*vb1x + vb1y*vb1y + vb1z*vb1z;
    b1mag = sqrt(b1mag2);
    b2mag2 = vb2x*vb2x + vb2y*vb2y + vb2z*vb2z;
    b2mag = sqrt(b2mag2);
    b3mag2 = vb3x*vb3x + vb3y*vb3y + vb3z*vb3z;
    b3mag = sqrt(b3mag2);

    ctmp = vb1x*vb2x + vb1y*vb2y + vb1z*vb2z;
    r12c1 = 1.0 / (b1mag*b2mag);
    c1mag = ctmp * r12c1;

    ctmp = vb2xm*vb3x + vb2ym*vb3y + vb2zm*vb3z;
    r12c2 = 1.0 / (b2mag*b3mag);
    c2mag = ctmp * r12c2;

    // cos and sin of 2 angles and final c

    sin2 = MAX(1.0 - c1mag*c1mag,0.0);
    sc1 = sqrt(sin2);
    if (sc1 < SMALL) sc1 = SMALL;
    sc1 = 1.0/sc1;

    sin2 = MAX(1.0 - c2mag*c2mag,0.0);
    sc2 = sqrt(sin2);
    if (sc2 < SMALL) sc2 = SMALL;
    sc2 = 1.0/sc2;

    s1 = sc1 * sc1;
    s2 = sc2 * sc2;
    s12 = sc1 * sc2;
    c = (c0 + c1mag*c2mag) * s12;

    cx = vb1y*vb2z - vb1z*vb2y;
    cy = vb1z*vb2x - vb1x*vb2z;
    cz = vb1x*vb2y - vb1y*vb2x;
    cmag = sqrt(cx*cx + cy*cy + cz*cz);
    dx = (cx*vb3x + cy*vb3y + cz*vb3z)/cmag/b3mag;

    // error check

    if (c > 1.0 + TOLERANCE || c < (-1.0 - TOLERANCE)) {
      int me;
      MPI_Comm_rank(world,&me);
      if (screen) {
        char str[128];
        sprintf(str,"Dihedral problem: %d " BIGINT_FORMAT " "
                TAGINT_FORMAT " " TAGINT_FORMAT " "
                TAGINT_FORMAT " " TAGINT_FORMAT,
                me,update->ntimestep,
                atom->tag[i1],atom->tag[i2],atom->tag[i3],atom->tag[i4]);
        error->warning(FLERR,str,0);
        fprintf(screen,"  1st atom: %d %g %g %g\n",
                me,x[i1][0],x[i1][1],x[i1][2]);
        fprintf(screen,"  2nd atom: %d %g %g %g\n",
                me,x[i2][0],x[i2][1],x[i2][2]);
        fprintf(screen,"  3rd atom: %d %g %g %g\n",
                me,x[i3][0],x[i3][1],x[i3][2]);
        fprintf(screen,"  4th atom: %d %g %g %g\n",
                me,x[i4][0],x[i4][1],x[i4][2]);
      }
    }

    if (c > 1.0) c = 1.0;
    if (c < -1.0) c = -1.0;

    // force & energy
    // p = k ( phi- phi0)^2
    // pd = dp/dc

    phi = acos(c);
    if (dx > 0.0) phi *= -1.0;
    si = sin(phi);
    if (fabs(si) < SMALLER) si = SMALLER;
    siinv = 1.0/si;

    dphia = phi - fa;
    dphib = phi - fb;
    dphib2 = dphib + 2.0*MY_PI;
    dphic = phi - fc;
    dphic2 = dphic - 2.0*MY_PI;
    dphid = phi - fd;
    dphid2 = dphid - 2.0*MY_PI;

    pa = -ka * dphia;
    pb = -kb * dphib * dphib * dphib;;
    pb2 = -kb * dphib2 * dphib2 * dphib2;
    pc = -kc * dphic;
    pc2 = -kc * dphic2;
    pd = -kd * dphid * dphid * dphid;
    pd2 = -kd * dphid2 * dphid2 * dphid2;

    ea = pa * dphia - epsdihed[type];
    eb = pb * dphib + eb0;
    eb2 = pb2 * dphib2 + eb0;
    ec = pc * dphic + epsdihed[type] + ec0;
    ec2 = pc2 * dphic2 + epsdihed[type] + ec0; 
    ed = pd * dphid + ed0 + ec0;
    ed2 = pd2 * dphid2 + ed0 + ec0;

    fea = exp(ea);
    feb = exp(eb);
    feb2 = exp(eb2);
    fec = exp(ec);
    fec2 = exp(ec2);
    fed = exp(ed);
    fed2 = exp(ed2);

    pp = fea + feb + feb2 + fec +fec2 + fed + fed2;
    ppd = 2.0*pa*fea + 4.0*pb*feb + 4.0*pb2*feb2 + 2.0*pc*fec
        + 2.0*pc2*fec2 + 4.0*pd*fed + 4.0*pd2*fed2;

    ppd /= pp;
    ppd *= siinv;

    if (eflag) edihedral = -log(pp);

    a = ppd;
    c = c * a;
    s12 = s12 * a;
    a11 = c*sb1*s1;
    a22 = -sb2 * (2.0*c0*s12 - c*(s1+s2));
    a33 = c*sb3*s2;
    a12 = -r12c1 * (c1mag*c*s1 + c2mag*s12);
    a13 = -rb1*rb3*s12;
    a23 = r12c2 * (c2mag*c*s2 + c1mag*s12);

    sx2  = a12*vb1x + a22*vb2x + a23*vb3x;
    sy2  = a12*vb1y + a22*vb2y + a23*vb3y;
    sz2  = a12*vb1z + a22*vb2z + a23*vb3z;

    f1[0] = a11*vb1x + a12*vb2x + a13*vb3x;
    f1[1] = a11*vb1y + a12*vb2y + a13*vb3y;
    f1[2] = a11*vb1z + a12*vb2z + a13*vb3z;

    f2[0] = -sx2 - f1[0];
    f2[1] = -sy2 - f1[1];
    f2[2] = -sz2 - f1[2];

    f4[0] = a13*vb1x + a23*vb2x + a33*vb3x;
    f4[1] = a13*vb1y + a23*vb2y + a33*vb3y;
    f4[2] = a13*vb1z + a23*vb2z + a33*vb3z;

    f3[0] = sx2 - f4[0];
    f3[1] = sy2 - f4[1];
    f3[2] = sz2 - f4[2];

    // apply force to each of 4 atoms

    if (newton_bond || i1 < nlocal) {
      f[i1][0] += f1[0];
      f[i1][1] += f1[1];
      f[i1][2] += f1[2];
    }

    if (newton_bond || i2 < nlocal) {
      f[i2][0] += f2[0];
      f[i2][1] += f2[1];
      f[i2][2] += f2[2];
    }

    if (newton_bond || i3 < nlocal) {
      f[i3][0] += f3[0];
      f[i3][1] += f3[1];
      f[i3][2] += f3[2];
    }

    if (newton_bond || i4 < nlocal) {
      f[i4][0] += f4[0];
      f[i4][1] += f4[1];
      f[i4][2] += f4[2];
    }

    if (evflag)
      ev_tally(i1,i2,i3,i4,nlocal,newton_bond,edihedral,f1,f3,f4,
               vb1x,vb1y,vb1z,vb2x,vb2y,vb2z,vb3x,vb3y,vb3z);

     // 1-4 LJ interactions
    delx = x[i1][0] - x[i4][0];
    dely = x[i1][1] - x[i4][1];
    delz = x[i1][2] - x[i4][2];
    rsq = delx*delx + dely*dely + delz*delz;
    r2inv = 1.0/rsq;
    r6inv = r2inv*r2inv*r2inv;

    if (rsq < sigma[type]*sigma[type])
      forcelj = r6inv * (ljm1[type]*r6inv - ljm2[type]);
    else
      forcelj = r6inv*(ljn1[type]*r6inv - ljn2[type]);
    fpair = forcelj * r2inv;

    if (eflag) {
      if (rsq < sigma[type]*sigma[type])
        evdwl = r6inv * (ljm3[type]*r6inv - ljm4[type]) +
                epsilon[type] + lambda[type];
      else
        evdwl = r6inv*(ljn3[type]*r6inv - ljn4[type]);
    } 

    if (newton_bond || i1 < nlocal) {
      f[i1][0] += delx*fpair;
      f[i1][1] += dely*fpair;
      f[i1][2] += delz*fpair;
    }
    if (newton_bond || i4 < nlocal) {
      f[i4][0] -= delx*fpair;
      f[i4][1] -= dely*fpair;
      f[i4][2] -= delz*fpair;
    }

    if (evflag) force->pair->ev_tally(i1,i4,nlocal,newton_bond,
                                        evdwl,ecoul,fpair,delx,dely,delz);
  }
}

/* ---------------------------------------------------------------------- */

void DihedralGaussianLJLambda::allocate()
{
  allocated = 1;
  int n = atom->ndihedraltypes;

  memory->create(epsilon,n+1,"dihedral:epsilon");
  memory->create(sigma,n+1,"dihedral:sigma");
  memory->create(lambda,n+1,"dihedral:lambda");
  memory->create(epsdihed,n+1,"dihedral:epsdihed");
  memory->create(ljm1,n+1,"dihedral:ljm1");
  memory->create(ljm2,n+1,"dihedral:ljm2");
  memory->create(ljm3,n+1,"dihedral:ljm3");
  memory->create(ljm4,n+1,"dihedral:ljm4");
  memory->create(ljn1,n+1,"dihedral:ljn1");
  memory->create(ljn2,n+1,"dihedral:ljn2");
  memory->create(ljn3,n+1,"dihedral:ljn3");
  memory->create(ljn4,n+1,"dihedral:ljn4");

  memory->create(setflag,n+1,"dihedral:setflag");
  for (int i = 1; i <= n; i++) setflag[i] = 0;
}

/* ----------------------------------------------------------------------
   set coeffs for one type
------------------------------------------------------------------------- */

void DihedralGaussianLJLambda::coeff(int narg, char **arg)
{
  if (narg < 4 || narg > 5) error->all(FLERR,"Incorrect args for dihedral coefficients");
  if (!allocated) allocate();

  int ilo,ihi;
  force->bounds(FLERR,arg[0],atom->ndihedraltypes,ilo,ihi);

  double epsilon_one = force->numeric(FLERR,arg[1]);
  double sigma_one = force->numeric(FLERR,arg[2]);
  double lambda_one = force->numeric(FLERR,arg[3]);
  double epsdihed_one = 0.0;
  if (narg > 4) epsdihed_one = force->numeric(FLERR,arg[4]);

  int count = 0;
  for (int i = ilo; i <= ihi; i++) {
    epsilon[i] = epsilon_one;
    sigma[i] = sigma_one;
    lambda[i] = lambda_one;
    epsdihed[i] = epsdihed_one;
    ljm1[i] = 12.0 * epsilon[i] * pow(sigma[i],12.0);
    ljm2[i] = 12.0 * epsilon[i] * pow(sigma[i],6.0);
    ljm3[i] = epsilon[i] * pow(sigma[i],12.0);
    ljm4[i] = 2.0 * epsilon[i] * pow(sigma[i],6.0);
    ljn1[i] = -12.0 * lambda[i] * pow(sigma[i],12.0);
    ljn2[i] = -12.0 * lambda[i] * pow(sigma[i],6.0);
    ljn3[i] = -lambda[i] * pow(sigma[i],12.0);
    ljn4[i] = -2.0 * lambda[i] * pow(sigma[i],6.0);
    setflag[i] = 1;
    count++;
  }

  if (count == 0) error->all(FLERR,"Incorrect args for dihedral coefficients");
}

/* ----------------------------------------------------------------------
   proc 0 writes out coeffs to restart file
------------------------------------------------------------------------- */

void DihedralGaussianLJLambda::write_restart(FILE *fp)
{
  fwrite(&epsilon[1],sizeof(double),atom->ndihedraltypes,fp);
  fwrite(&sigma[1],sizeof(double),atom->ndihedraltypes,fp);
  fwrite(&lambda[1],sizeof(double),atom->ndihedraltypes,fp);
  fwrite(&epsdihed[1],sizeof(double),atom->ndihedraltypes,fp);
}

/* ----------------------------------------------------------------------
   proc 0 reads coeffs from restart file, bcasts them
------------------------------------------------------------------------- */

void DihedralGaussianLJLambda::read_restart(FILE *fp)
{
  allocate();

  if (comm->me == 0) {
    fread(&epsilon[1],sizeof(double),atom->ndihedraltypes,fp);
    fread(&sigma[1],sizeof(double),atom->ndihedraltypes,fp);
    fread(&lambda[1],sizeof(double),atom->ndihedraltypes,fp);
    fread(&epsdihed[1],sizeof(double),atom->ndihedraltypes,fp);
  }
  MPI_Bcast(&epsilon[1],atom->ndihedraltypes,MPI_DOUBLE,0,world);
  MPI_Bcast(&sigma[1],atom->ndihedraltypes,MPI_DOUBLE,0,world);
  MPI_Bcast(&lambda[1],atom->ndihedraltypes,MPI_DOUBLE,0,world);
  MPI_Bcast(&epsdihed[1],atom->ndihedraltypes,MPI_DOUBLE,0,world);

  for (int i = 1; i <= atom->ndihedraltypes; i++) {
    setflag[i] = 1;
    ljm1[i] = 12.0 * epsilon[i] * pow(sigma[i],12.0);
    ljm2[i] = 12.0 * epsilon[i] * pow(sigma[i],6.0);
    ljm3[i] = epsilon[i] * pow(sigma[i],12.0);
    ljm4[i] = 2.0 * epsilon[i] * pow(sigma[i],6.0);
    ljn1[i] = -12.0 * lambda[i] * pow(sigma[i],12.0);
    ljn2[i] = -12.0 * lambda[i] * pow(sigma[i],6.0);
    ljn3[i] = -lambda[i] * pow(sigma[i],12.0);
    ljn4[i] = -2.0 * lambda[i] * pow(sigma[i],6.0);
  }
}

/* ----------------------------------------------------------------------
   proc 0 writes to data file
------------------------------------------------------------------------- */

void DihedralGaussianLJLambda::write_data(FILE *fp)
{
  for (int i = 1; i <= atom->ndihedraltypes; i++)
    fprintf(fp,"%d %g %g %g %g\n",i,epsilon[i],sigma[i],lambda[i],epsdihed[i]);
}
