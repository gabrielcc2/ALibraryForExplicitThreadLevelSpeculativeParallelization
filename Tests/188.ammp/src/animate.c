/*  animate.c
*
* routines for performing molecular dynamics
*
* v_maxwell( float Temperature, driftx,drifty,driftz)  initialize velocities
*                           with maxwell distribution, assuming
*                           simple kinetic theory relating T and v
*                           driftx,drifty,driftz allow the use of a constant 
*                           drift velocity.
*
* int v_rescale( float Temperature)
*  rescale velocities so that ke is 3RT/2M
*
* int verlet(forces,nforces, nstep,dtime)
*           perform verlet (forward Euler) dynamics
*                           
* int pac( forces,nforces, nstep,dtime)
*            predict and correct dynamics
*
* int pacpac( forces,nforces,nstep,dtime)
*             iterated pac dynamics
*
*
* int tpac( forces,nforces, nstep,dtime, T)
*  nose constrained dynamics
* int ppac( forces,nforces, nstep,dtime, P)
*   pressure only constrained
* int ptpac( forces,nforces, nstep,dtime,P, T)
*   pressure and temperature constrained
* int hpac( forces,nforces, nstep,dtime, H)
*  total energy  constrained dynamics
*/
/*
*  copyright 1992 Robert W. Harrison
*  
*  This notice may not be removed
*  This program may be copied for scientific use
*  It may not be sold for profit without explicit
*  permission of the author(s) who retain any
*  commercial rights including the right to modify 
*  this notice
*/

#define ANSI 1
/* misc includes - ANSI and some are just to be safe */
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#ifdef ANSI
#include <stdlib.h>
#endif
#include "parallel_loop_in_batches.h"
#include <iostream>
using namespace std;

parallel_loop_in_batches parallel_loop9;

//#include "ammp.h"
/* ATOM structure contains a serial number for indexing into
* arrays and the like (a Hessian)
* but otherwise is self-contained. Note the hooks for Non-bonded potentials
*/

typedef struct {
  float xc,yc,zc;
  float sqp;
  /* dipole appx for r^-6 */
  float sa,xa,ya,za;
  float q100,q010,q001;
  float q200,q020,q002,q110,q101,q011;
  float q300,q030,q003,q210,q201,q120,q021,q102,q012,q111;
#ifdef FOURTH
  float q400,q040,q004,q310,q301,q130,q031,q103,q013,q220,q202,q022,q211,q121,q112;
#endif
#ifdef FIFTH
  float q500,q050,q005,q410,q401,q140,q041,q104,q014,q320,q230,q302,q203,q032,q023,q311,q131,q113,q221,q212,q122;
#endif
  int first,last,innode; } MMNODE;
typedef struct {
 ATOM *who;
 int next,which; } MMATOM;


int v_maxwell(float T, float dx, float dy, float dz)
{
	float randg();
	/* void rand3();   SPEC declare proto in ammp.h jh/9/22/99   */
	ATOM *ap,*a_next(int),*bonded[10];
	int iflag,inbond;
	float vmag;
	float R;
	R = 1.987 ; /* kcal/mol/K */

	iflag = -1;
	while( (ap= a_next(iflag++)) != NULL)
	{
	iflag = 1;
	if( ap->mass > 0.)
	{
/* convert from kcal to mks */
/* 4.184 to joules */
/* 1000 grams to kg */
	vmag = sqrt( 3.*R*T/ap->mass*4.184*1000.)* randg();
	rand3( &ap->vx,&ap->vy,&ap->vz);
	if( ap->active ){
	ap->vx = ap->vx*vmag + dx;
	ap->vy = ap->vy*vmag + dy;
	ap->vz = ap->vz*vmag + dz;
	}else{ 
	ap->vx = 0.;
	ap->vy = 0.;
	ap->vz = 0.;
	}
	}
	}
/* now check those who are zero mass */
/* and give them the velocity of the first bonded atom */
	iflag = -1;
	while( (ap= a_next(iflag)) != NULL)
	{
	iflag = 1;
		if( ap->mass <= 0.)
		{
		get_bond(ap,bonded,10,&inbond);
			if( inbond >= 0)
			{
			ap->vx = bonded[0]->vx;
			ap->vy = bonded[0]->vy;
			ap->vz = bonded[0]->vz;
			}
		}
	}
	return 1;
}
/* v_rescale(T)
*  rescale the velocities for constant KE  == Temperature 
*/
int v_rescale(float T)
{
	ATOM *ap,*a_next(int);
	int iflag,a_number();
	float vmag,KE,target;
	float R;
	R = 1.987 ; /* kcal/mol/K */

	target = 0.;
	target += .5*(3.*R*T)*4.184*1000*a_number();
	KE = 0.; 
	iflag = -1;
	while( (ap= a_next(iflag++)) != NULL)
	{
	iflag = 1;
	if( ap->mass > 0.)
	{
	vmag = ap->vx*ap->vx+ap->vy*ap->vy+ap->vz*ap->vz;
	KE += ap->mass*vmag;	
	}}
	KE = KE *.5;
	if( KE == 0.)
	{ aaerror(" Cannot rescale a zero velocity field -use v_maxwell");
		return 0;
		}
	vmag = sqrt(target/KE);

	iflag = -1;
	while( (ap= a_next(iflag++)) != NULL)
	{
	iflag = 1;
	ap->vx = ap->vx*vmag;
	ap->vy = ap->vy*vmag;
	ap->vz = ap->vz*vmag;
	}
	return 1;
}
/* routine verlet( nstep,dtime)
*int verlet(forces,nforces, nstep,dtime)
*
* perform nstep leapfrogging dynamics with dtime
*/
int verlet(int (*forces[])(float), int nforces, int nstep, float dtime)
{
	ATOM *bp,*ap,*a_next(int),*bonded[10];
	int inbond,iflag;
	int a_f_zero(),a_inc_v(float);
	int istep,iforces;
	int i,imax,a_number();
	for( istep = 0.; istep< nstep; istep++)
	{

/*  find the force at the midpoint */
	a_f_zero();
	for( iforces=0;iforces<nforces; iforces++)
		(*forces[iforces])( 0.);
/* update velocities */        
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
	if( ap->mass > 0.) 
	{
/* the magic number takes kcal/molA to mks */
		ap->vx += ap->fx/ap->mass*dtime*4.184e6;
		ap->vy += ap->fy/ap->mass*dtime*4.184e6;
		ap->vz += ap->fz/ap->mass*dtime*4.184e6;
	}
	}
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
	if( ap->mass <= 0.) 
		{
                get_bond(ap,bonded,10,&inbond);
                        if( inbond >= 0)
                        {
                        ap->vx = bonded[0]->vx;
                        ap->vy = bonded[0]->vy;
                        ap->vz = bonded[0]->vz;
                        }
		}
	}
/* update positions */
	a_inc_v(dtime);
	}/* end of istep loop */
	return 1;
}
/* routine pac( nstep,dtime)
*int pac(forces,nforces, nstep,dtime)
*
* perform nstep pac dynamics with dtime
*
* predict the path given current velocity
* integrate the force (simpson's rule)
*  predict the final velocity
*  update the position using trapezoidal correction
*  
*  ideally several cycles are good
*/
int pac(int (*forces[])(float), int nforces, int nstep, float dtime)
{
	ATOM *ap,*bp,*a_next(int),*bonded[10];
	int inbond,iflag;
	int a_f_zero(),a_inc_v(float);
	int istep,iforces;
	int i,imax,a_number();
	for( istep = 0.; istep< nstep; istep++)
	{

/*  move the velocity vector into the displacment slot */
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
	ap->dx = ap->vx;
	ap->dy = ap->vy;
	ap->dz = ap->vz;
	}

/*  find the force at the midpoint */
	a_f_zero();
	for( iforces=0;iforces<nforces; iforces++)
		(*forces[iforces])( dtime/2.);
/* update velocities */        
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
	if( ap->mass > 0.) 
	{
/* the magic number takes kcal/molA to mks */
/*		ap->vx += ap->fx/ap->mass*dtime*4.184e6/6.;
*		ap->vy += ap->fy/ap->mass*dtime*4.184e6/6.;
*		ap->vz += ap->fz/ap->mass*dtime*4.184e6/6.;
*/
		ap->vx =  ap->dx +  ap->fx/ap->mass*dtime*4.184e6;
		ap->vy = ap->dy  + ap->fy/ap->mass*dtime*4.184e6;
		ap->vz = ap->dz  + ap->fz/ap->mass*dtime*4.184e6;
	}
	}
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
	if( ap->mass <= 0.) 
		{
                get_bond(ap,bonded,10,&inbond);
                        if( inbond >= 0)
                        {
                        ap->vx = bonded[0]->vx;
                        ap->vy = bonded[0]->vy;
                        ap->vz = bonded[0]->vz;
                        }
		}
	}
/* update positions */
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
		iflag = 1;
		ap->x += .5*(ap->vx + ap->dx)*dtime;
		ap->y += .5*(ap->vy + ap->dy)*dtime;
		ap->z += .5*(ap->vz + ap->dz)*dtime;
	}
		
	}/* end of istep loop */
	return 1;
}
/* routine tpac( nstep,dtime)
*int tpac(forces,nforces, nstep,dtime,T)
*
* perform nstep pac dynamics with dtime
* kinetic energy constraint to (3*natom-1) kT/2
*
* predict the path given current velocity
* integrate the force (simpson's rule)
*  predict the final velocity
*  update the position using trapezoidal correction
*  
*  ideally several cycles are good
*
* adaptive steps (6/19/96)
*  if the rescale is too large (i.e. > 2) do two half steps
*
*/
int tpac(int (*forces[])(float), int nforces, int nstep, float dtime_real, float T)
{
	ATOM *ap,*bp,*a_next(int),*bonded[10];
	float ke,Tke,R;
	float alpha;
	float dtime;
	int inbond,iflag;
	int a_f_zero(),a_inc_v(float);
	int istep,iforces;
	int i,imax,a_number();
	R = 1.987; /* kcal/mol/K */

	for( istep = 0.; istep< nstep; istep++)
	{

/*  move the velocity vector into the displacment slot */
	ke = 0.;
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
	ke += ap->mass*(
	ap->vx*ap->vx + ap->vy*ap->vy + ap->vz*ap->vz);
	ap->dx = ap->vx;
	ap->dy = ap->vy;
	ap->dz = ap->vz;
	}
	Tke = 3*(imax)*R*4.184*1000;  /* converted into MKS */
	Tke = ke/Tke;  /* Tke is now the current temperature */ 
/* scale the current velocities */
	dtime = dtime_real;
	if( Tke > 1.e-6)
	{	
	ke = sqrt(T/Tke); /* ke is the scaled shift value */
	dtime = dtime_real/ke;
/* 0.00002 is 2fs, this is near the limit so don't use it */
	if( dtime > 0.000020 ){
		tpac(forces,nforces,1,dtime_real*0.5,T); 
		tpac(forces,nforces,1,dtime_real*0.5,T); 
		//goto SKIP;
			}
	ap = a_next(-1);
	bp =  ap;
	for( i=0; i< imax;  i++, ap = bp)
	{
	bp = a_next(1);
	ap->dx *= ke;
	ap->dy *= ke;
	ap->dz *= ke;
	}
	}

/*  find the force at the midpoint */
	a_f_zero();

//	parallel_mm_fv_update.run(input_functions, input_vars);
  //      parallel_mm_fv_update.commit();
	for( iforces=0;iforces<nforces; iforces++)
		(*forces[iforces])( dtime/2.); //Esta es la invocación que consume la mayor parte del tiempo.
	//parallel_mm_fv_update.get_results();
/* update velocities */        
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
	if( ap->mass > 0.) 
	{
		ap->vx = ap->dx  + ap->fx/ap->mass*dtime*4.184e6;
		ap->vy = ap->dy  + ap->fy/ap->mass*dtime*4.184e6;
		ap->vz = ap->dz  + ap->fz/ap->mass*dtime*4.184e6;
	}
	}
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
	if( ap->mass <= 0.) 
		{
                get_bond(ap,bonded,10,&inbond);
                        if( inbond >= 0)
                        {
                        ap->vx = bonded[0]->vx;
                        ap->vy = bonded[0]->vy;
                        ap->vz = bonded[0]->vz;
                        }
		}
	}
/* update positions */
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
		iflag = 1;
		ap->x += .5*(ap->vx + ap->dx)*dtime;
		ap->y += .5*(ap->vy + ap->dy)*dtime;
		ap->z += .5*(ap->vz + ap->dz)*dtime;
	}
SKIP: ; /* if we are here from goto we have done two half steps (or more)*/
		
	}/* end of istep loop */
	return 1;
}
/* routine pacpac( nstep,dtime)
*int pacpac(forces,nforces, nstep,dtime)
*
* perform nstep pac dynamics with dtime
*
* predict the path given current velocity
* integrate the force (simpson's rule)
*  predict the final velocity
*  update the position using trapezoidal correction
*  
*  ideally several cycles are good
*/
int pacpac(int (*forces[])(float), int nforces, int nstep, float dtime)
{
	ATOM *ap,*a_next(int),*bp,*bonded[10];
	int inbond,iflag;
	int a_f_zero(),a_inc_v(float);
	int istep,iforces,icorrect;
	int i,imax,a_number();
	for( istep = 0.; istep< nstep; istep++)
	{

/*  move the velocity vector into the displacment slot */
	iflag = -1;
	while( (ap=a_next(iflag)) != NULL)
	{
	iflag = 1;
	ap->dx = ap->vx;
	ap->dy = ap->vy;
	ap->dz = ap->vz;
	}

/*  find the force at the midpoint */
	a_f_zero();
	for( iforces=0;iforces<nforces; iforces++)
		(*forces[iforces])( dtime/2.);
/* update velocities */        
	iflag = -1;
	while( (ap=a_next(iflag)) != NULL)
	{
	iflag = 1;
	if( ap->mass > 0.) 
	{
/* the magic number takes kcal/molA to mks */
		ap->gx = ap->vx;	
		ap->gy = ap->vy;	
		ap->gz = ap->vz;	
		ap->vx += ap->fx/ap->mass*dtime*4.184e6;
		ap->vy += ap->fy/ap->mass*dtime*4.184e6;
		ap->vz += ap->fz/ap->mass*dtime*4.184e6;
	}
	}
	iflag = -1;
	while( (ap=a_next(iflag)) != NULL)
	{
	iflag = 1;
	if( ap->mass <= 0.) 
		{
		ap->gx = ap->vx;	
		ap->gy = ap->vy;	
		ap->gz = ap->vz;	
                get_bond(ap,bonded,10,&inbond);
                        if( inbond >= 0)
                        {
                        ap->vx = bonded[0]->vx;
                        ap->vy = bonded[0]->vy;
                        ap->vz = bonded[0]->vz;
                        }
		} /* end of mass check */
	}
/* make up the new prediction direction */
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
		bp = a_next(1);
		ap->dx = ap->vx + ap->gx;
		ap->dy = ap->vy + ap->gy;
		ap->dz = ap->vz + ap->gz;
	}
	for( icorrect = 0;icorrect < 2; icorrect ++)
	{
	a_f_zero();
	for( iforces=0;iforces<nforces; iforces++)
		(*forces[iforces])( dtime/4.);
/* update velocities */        
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
	if( ap->mass > 0.) 
	{
/* the magic number takes kcal/molA to mks */
		ap->vx = ap->gx + ap->fx/ap->mass*dtime*4.184e6;
		ap->vy = ap->gy + ap->fy/ap->mass*dtime*4.184e6;
		ap->vz = ap->gz + ap->fz/ap->mass*dtime*4.184e6;
	}
	}
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
	if( ap->mass <= 0.) 
		{
                get_bond(ap,bonded,10,&inbond);
                        if( inbond >= 0)
                        {
                        ap->vx = bonded[0]->vx;
                        ap->vy = bonded[0]->vy;
                        ap->vz = bonded[0]->vz;
                        }
		} /* end of mass check */
	}
/* make up the new prediction direction */
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
		ap->dx = ap->vx + ap->gx;
		ap->dy = ap->vy + ap->gy;
		ap->dz = ap->vz + ap->gz;
	}
	}
/* update positions */
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
		ap->x += .5*(ap->vx + ap->gx)*dtime;
		ap->y += .5*(ap->vy + ap->gy)*dtime;
		ap->z += .5*(ap->vz + ap->gz)*dtime;
	}
		
	}/* end of istep loop */
	return 1;
}
/* routine hpac( nstep,dtime)
*int hpac(forces,nforces, nstep,dtime,H)
*
* perform nstep pac dynamics with dtime
* kinetic energy adusted for constant H
*
* predict the path given current velocity
* integrate the force (simpson's rule)
*  predict the final velocity
*  update the position using trapezoidal correction
*  
*  ideally several cycles are good
*/
int hpac(int (*forces[])(float), int (*poten[])(float*, float), int nforces, int nstep, float dtime_real, float H)
{
	ATOM *ap,*bp,*a_next(int),*bonded[10];
	float ke,Tke;
	float alpha;
	float dtime;
	int inbond,iflag;
	int a_f_zero(),a_inc_v(float);
	int istep,iforces;
	int i,imax,a_number();


	for( istep = 0.; istep< nstep; istep++)
	{

/*  move the velocity vector into the displacment slot */
	ke = 0.;
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
	ke += ap->mass*(
	ap->vx*ap->vx + ap->vy*ap->vy + ap->vz*ap->vz);
	ap->dx = ap->vx;
	ap->dy = ap->vy;
	ap->dz = ap->vz;
	}
	ke = ke*.5/4.184/1000/1000;  /* ke in kcal/mol */
/* get the current potential */
	Tke = 0.;
	for(i=0; i< nforces; i++)
		(*poten[i])(&Tke,0.);
/* scale the current velocities */
	dtime = dtime_real;
	if( Tke < H )
	{	
	ke = sqrt((H-Tke)/ke); /* ke is the scaled shift value */
	dtime = dtime_real/ke;
/* 0.00002 is 2fs, this is near the limit so don't use it */
	if( dtime > 0.000020 ){
                /* SPEC: fix # of arguments - jh/9/21/99 */
		hpac(forces,poten,nforces,1,dtime_real*0.5,H); 
		hpac(forces,poten,nforces,1,dtime_real*0.5,H); 
		goto SKIP;
			}
	ap = a_next(-1);
	bp =  ap;
	for( i=0; i< imax;  i++, ap = bp)
	{
	bp = a_next(1);
	ap->dx *= ke;
	ap->dy *= ke;
	ap->dz *= ke;
	}
	} else { 
	aaerror("Warning in Hpac, Potential energy higher than target\n");
	a_v_zero();
	a_d_zero();
	}

/*  find the force at the midpoint */
	a_f_zero();
	for( iforces=0;iforces<nforces; iforces++)
		(*forces[iforces])( dtime/2.);
/* update velocities */        
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
	if( ap->mass > 0.) 
	{
		ap->vx = ap->dx  + ap->fx/ap->mass*dtime*4.184e6;
		ap->vy = ap->dy  + ap->fy/ap->mass*dtime*4.184e6;
		ap->vz = ap->dz  + ap->fz/ap->mass*dtime*4.184e6;
	}
	}
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
	if( ap->mass <= 0.) 
		{
                get_bond(ap,bonded,10,&inbond);
                        if( inbond >= 0)
                        {
                        ap->vx = bonded[0]->vx;
                        ap->vy = bonded[0]->vy;
                        ap->vz = bonded[0]->vz;
                        }
		}
	}
/* update positions */
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
		iflag = 1;
		ap->x += .5*(ap->vx + ap->dx)*dtime;
		ap->y += .5*(ap->vy + ap->dy)*dtime;
		ap->z += .5*(ap->vz + ap->dz)*dtime;
	}
SKIP: ; /* if goto here we've had too large a step and used half steps */
		
	}/* end of istep loop */
	return 1;
}
/* routine ppac( nstep,dtime)
*int ppac(forces,nforces, nstep,dtime,P)
*
* force the pressure to be constant
* use P = integral ( f . r )dV as 	
* the basis for a diffeomorphism
*   P => kP or Integral( kf.r)dV
*         to enforce pressure
*   r => r/k to enforce physical reality
*   may need to damp this.
*  
*
* perform nstep pac dynamics with dtime
*
* predict the path given current velocity
* integrate the force (simpson's rule)
*  predict the final velocity
*  update the position using trapezoidal correction
*  
*  ideally several cycles are good
*/
int ppac(int (*forces[])(float), int nforces, int nstep, float dtime_real, float P)
{
	ATOM *ap,*bp,*a_next(int),*bonded[10];
	float p,Tp,R;
	float dtime,cx,cy,cz;
	float alpha;
	int inbond,iflag;
	int a_f_zero(),a_inc_v(float);
	int istep,iforces;
	int i,imax,a_number();
	R = 1.987; /* kcal/mol/K */

	imax = a_number();
	if( imax <= 0 )return 0;
	for( istep = 0.; istep< nstep; istep++)
	{

	cx = 0.; cy = 0.; cz = 0.;	
/*  move the velocity vector into the displacment slot */
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
	ap->dx = ap->vx;
	ap->dy = ap->vy;
	ap->dz = ap->vz;
	cx += ap->x;
	cy += ap->y;
	cz += ap->z;
	}
	cx /= imax;
	cy /= imax;
	cz /= imax;

/* calculate the pressure */

	p = 0.;
	Tp = 0.;
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
		bp = a_next(1);
		p += ap->vx*ap->vx*ap->mass;
		p += ap->vy*ap->vy*ap->mass;
		p += ap->vz*ap->vz*ap->mass;
		Tp += (ap->x-cx)*(ap->x-cx);
		Tp += (ap->y-cy)*(ap->y-cy);
		Tp += (ap->z-cz)*(ap->z-cz);
	}
	Tp = sqrt(Tp/imax);
	Tp = 4*PI/3*Tp*Tp*Tp;
  	p = p/imax/Tp*.5; /* now mks molar */
	printf("P %f p %f Tp %f\n",P,p,Tp);
/* moment shift
	p = sqrt( P/p);
	dtime = dtime_real/p;
*/
	dtime = dtime_real;
/* this is about the steepest volume correction which works !! 
  1. + .2/1.2 and 1 + .5/1.5 fail
*/
	p = (1.+.1*pow( p/P, 1./3.))/1.1;
	
/* temporary kludge to understand problem */
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
		bp = a_next(1);
/*
		ap->vx *= p;	
		ap->vy *= p;	
		ap->vz *= p;	
		ap->dx *= p;	
		ap->dy *= p;	
		ap->dz *= p;	
*/

		ap->x *= p;
		ap->y *= p;
		ap->z *= p;
	}
/*  find the force at the midpoint */
	a_f_zero();
	for( iforces=0;iforces<nforces; iforces++)
		(*forces[iforces])( dtime/2.);

/* update velocities */        
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
	if( ap->mass > 0.) 
	{
		ap->vx = ap->dx  + ap->fx/ap->mass*dtime*4.184e6;
		ap->vy = ap->dy  + ap->fy/ap->mass*dtime*4.184e6;
		ap->vz = ap->dz  + ap->fz/ap->mass*dtime*4.184e6;
	}
	}
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
	if( ap->mass <= 0.) 
		{
                get_bond(ap,bonded,10,&inbond);
                        if( inbond >= 0)
                        {
                        ap->vx = bonded[0]->vx;
                        ap->vy = bonded[0]->vy;
                        ap->vz = bonded[0]->vz;
                        }
		}
	}
/* update positions */
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
		iflag = 1;
		ap->x += .5*(ap->vx + ap->dx)*dtime;
		ap->y += .5*(ap->vy + ap->dy)*dtime;
		ap->z += .5*(ap->vz + ap->dz)*dtime;
	}
		
	}/* end of istep loop */
	return 1;
}
/* routine ptpac( nstep,dtime)
*int ptpac(forces,nforces, nstep,dtime,P,T)
*
* force the pressure to be constant
*  
*
* perform nstep pac dynamics with dtime
*
* predict the path given current velocity
* integrate the force (simpson's rule)
*  predict the final velocity
*  update the position using trapezoidal correction
*  
*  ideally several cycles are good
*/
int ptpac(int (*forces[])(float), int nforces, int nstep, float dtime_real, float P, float T)
{
	ATOM *ap,*bp,*a_next(int),*bonded[10];
	float p,Tp,R;
	float Tk;
	float dtime,cx,cy,cz;
	float alpha;
	int inbond,iflag;
	int a_f_zero(),a_inc_v(float);
	int istep,iforces;
	int i,imax,a_number();
	R = 1.987; /* kcal/mol/K */

	imax = a_number();
	if( imax <= 0 )return 0;
	for( istep = 0.; istep< nstep; istep++)
	{

	cx = 0.; cy = 0.; cz = 0.;	
/*  move the velocity vector into the displacment slot */
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
	ap->dx = ap->vx;
	ap->dy = ap->vy;
	ap->dz = ap->vz;
	cx += ap->x;
	cy += ap->y;
	cz += ap->z;
	}
	cx /= imax;
	cy /= imax;
	cz /= imax;

/* calculate the pressure */

	p = 0.;
	Tp = 0.;
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
		bp = a_next(1);
		p += ap->vx*ap->vx*ap->mass;
		p += ap->vy*ap->vy*ap->mass;
		p += ap->vz*ap->vz*ap->mass;
		Tp += (ap->x-cx)*(ap->x-cx);
		Tp += (ap->y-cy)*(ap->y-cy);
		Tp += (ap->z-cz)*(ap->z-cz);
	}
	Tp = sqrt(Tp/imax);
	Tp = 4*PI/3*Tp*Tp*Tp;
	Tk = 3*imax*R*4.184*1000;
	Tk = p/Tk;  /* Tk is now the temperature */
	if( Tk < 1.e-5) Tk = 1.;
  	p = p/imax/Tp*.5; /* now mks molar  ( kilopascal's because of grams)*/
	printf("P %f p %f Tp %f\n",P,p,Tp);
/* momentum shift */
	Tk = sqrt(T/Tk);
	dtime = dtime_real/Tk;
/* 0.00002 is 2fs, this is near the limit so don't use it */
	if( dtime > 0.000020 ){
		ptpac(forces,nforces,1,dtime_real*0.5,P,T); 
		ptpac(forces,nforces,1,dtime_real*0.5,P,T); 
		goto SKIP;
			}
/* this is about the steepest volume correction which works !! 
  1. + .2/1.2 and 1 + .5/1.5 fail
also checked that the current 'pressure' is the best to use
for stable running  
*/
	p = (1.+.1*pow( p/P, 1./3.))/1.1;
	
/* temporary kludge to understand problem */
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
		bp = a_next(1);
		ap->vx *= Tk;	
		ap->vy *= Tk;	
		ap->vz *= Tk;	
		ap->dx *= Tk;	
		ap->dy *= Tk;	
		ap->dz *= Tk;	

		ap->x *= p;
		ap->y *= p;
		ap->z *= p;
	}
/*  find the force at the midpoint */
	a_f_zero();
	for( iforces=0;iforces<nforces; iforces++)
		(*forces[iforces])( dtime/2.);

/* update velocities */        
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
	if( ap->mass > 0.) 
	{
		ap->vx = ap->dx  + ap->fx/ap->mass*dtime*4.184e6;
		ap->vy = ap->dy  + ap->fy/ap->mass*dtime*4.184e6;
		ap->vz = ap->dz  + ap->fz/ap->mass*dtime*4.184e6;
	}
	}
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
	if( ap->mass <= 0.) 
		{
                get_bond(ap,bonded,10,&inbond);
                        if( inbond >= 0)
                        {
                        ap->vx = bonded[0]->vx;
                        ap->vy = bonded[0]->vy;
                        ap->vz = bonded[0]->vz;
                        }
		}
	}
/* update positions */
	imax = a_number();
	ap = a_next(-1);
	bp = ap;
	for( i=0; i< imax; i++,ap = bp)
	{
	bp = a_next(1);
		iflag = 1;
		ap->x += .5*(ap->vx + ap->dx)*dtime;
		ap->y += .5*(ap->vy + ap->dy)*dtime;
		ap->z += .5*(ap->vz + ap->dz)*dtime;
	}
		
SKIP: ; /* if goto here we've had too large a step and used half steps */
	}/* end of istep loop */
	return 1;
}



/* rectmm.c
*
* integrated multipole method with the amortized
* standard nonbonded program.
*  rectangular multipole expansion to the r^-6 order (5th order expansion)
*
*  from Eyges "The Classical Electromagnetic Field"
*  note that we use the oposite convention for sign of
*  expansion so use + for all the cumulants, while he uses
*   -1^n.   This is solely due to choice of origin and for
*   other applications (rxn feild ) the -1^n is correct.
*
*
* collection of routines to service nonbonded potentials
*
* POOP (Poor-mans Object Oriented Programming) using scope rules
*
* the routines for potential value, force and (eventually) second
* derivatives are here also
*
* force and 2nd derivative routines assume zero'd arrays for output
* this allows for parralellization if needed (on a PC?)
*
* forces are symmetric - so we don't have to mess around with
* s matrices and the like.
*
* note that the non-bonded information is in the ATOM structures 
*
*
* attempts at vectorization
*/
/*
*  copyright 1992, 1993, 1994, 1995 Robert W. Harrison
*  
*  This notice may not be removed
*  This program may be copied for scientific use
*  It may not be sold for profit without explicit
*  permission of the author(s) who retain any
*  commercial rights including the right to modify 
*  this notice
*/
/* ATOM structure contains a serial number for indexing into
* arrays and the like (a Hessian)
* but otherwise is self-contained. Note the hooks for Non-nonboned potentials
*/

/*
#define FOURTH
#define  FIFTH
#ifdef FIFTH
#define FOURTH
#endif
*/

int mm_fv_update_nonbon(float lambda)
{
	float r,r0,xt,yt,zt;
	float xt2,xt3,xt4,yt2,yt3,yt4,zt2,zt3,zt4;
	float k,k1,k2,k3,k4,k5;
	float ka2,ka3;
	float kb2,kb3;
	float c1,c2,c3,c4,c5; /* constants for the mm expansion */
	float get_f_variable(char*);
	int inbond,inangle,i;
	ATOM *ap,*a1,*a2,*bonded[10],*angled[10];
	ATOM *a_next(int); /* returns first ATOM when called with -1 */
	int a_number();
	int ii,j,jj,imax,inclose;
	float (*vector)[];
/* */
	ATOM *close[NCLOSE],*(*atomall)[];
	float mxdq,dielectric,mxcut; 
	float mmbox;
	float xmax,xmin,ymax,ymin,zmax,zmin;
	int nx,ny,nz;
	int ix,iy,iz,inode;
	int naybor[27];
	char line[80];
	MMNODE (*nodelist)[];
	MMATOM (*atomlist)[];


	mmbox = get_f_variable("mmbox");
	mxcut = get_f_variable("mxcut");
	if( mxcut < 0.) mxcut= 5.;

	dielectric = get_f_variable("dielec");
	if( dielectric <= 0.) dielectric = 1.;
	dielectric = 332.17752/dielectric;
	
/*  get the number of atoms and allocate the memory for the array space */
	i = a_number();
	vector = (float (*)[])  malloc( 4*i*sizeof(float) );
	if( vector == NULL) 
	{ aaerror("cannot allocate memory in mm_fv_update\n"); return 0;}
	atomall = (ATOM*(*)[]) malloc( i*sizeof(ATOM *) );
	if( atomall == NULL)
	{aaerror("cannot allocate memory in mm_fv_update\n"); return 0;}
	atomlist = (MMATOM (*)[]) malloc( i * sizeof( MMATOM ));
	if( atomlist == NULL)
	{ aaerror("cannot allocate memory in mm_fv_update\n"); return 0;}

	imax = a_number();
	jj = imax;

	for( i=0; i< imax; i++)
	{
		(*atomall)[i] = a_next(i);
		(*atomlist)[i].who = (*atomall)[i];
	}
/* first check if anyone's moved and update the lists */
/* note that this must be a look-ahead rather than
*  look back search because
* we cannot update ->px until we've used that atom !!! */
	for( ii=0; ii< imax; ii++)
	{
        a1 = (*atomall)[ii];
	j = ii*4;
        (*vector)[j] = a1->dx*lambda +a1->x ;
        (*vector)[j+1] = a1->dy*lambda +a1->y;
        (*vector)[j+2] = a1->dz*lambda +a1->z;
	}
/* determine the bounds of box which surrounds all of the atoms */
	xmax = -10e10;
	ymax = -10e10;
	zmax = -10e10;
	xmin =  10e10;
	ymin =  10e10;
	zmin =  10e10;
	for( ii= 0; ii< imax; ii++)
	{
		j = ii*4;
	  if( xmax < (*vector)[j] ) xmax = (*vector)[j];
	  if( ymax < (*vector)[j+1] ) ymax = (*vector)[j+1];
	  if( zmax < (*vector)[j+2] ) zmax = (*vector)[j+2];
	  if( xmin > (*vector)[j] ) xmin = (*vector)[j];
	  if( ymin > (*vector)[j+1] ) ymin = (*vector)[j+1];
	  if( zmin > (*vector)[j+2] ) zmin = (*vector)[j+2];
	  }
	nx = (xmax - xmin)/mmbox + 1 ;
	ny = (ymax - ymin)/mmbox + 1 ;
	nz = (zmax - zmin)/mmbox + 1 ;

/* DEBUG	
	sprintf(line,"before allocation mmbox %f nx %d ny %d nz %d \n",mmbox,nx,ny,nz);
	aaerror( line);
	sprintf(line," xmin xmax %f %f ymin ymax %f %f zmin zmax %f %f\n",
	xmin,xmax,ymin,ymax,zmin,zmax);
	aaerror( line);
end of DEBUG */

/* now try to malloc the mmnodes */
	nodelist = (MMNODE (*)[]) malloc( nx*ny*nz * sizeof( MMNODE ));
	if( nodelist == NULL)
	{ aaerror("cannot allocate node memory in mm_fv_update (doubling grid )\n"); 
	sprintf(line,"mmbox %f nx %d ny %d nz %d ",mmbox,nx,ny,nz);
	aaerror( line);
	sprintf(line," xmin xmax %f %f ymin ymax %f %f zmin zmax %f %f",
	xmin,xmax,ymin,ymax,zmin,zmax);
	aaerror( line);
	mmbox = mmbox *2;
	set_f_variable( "mmbox",mmbox);
	nx = (xmax - xmin)/mmbox + 1;
	ny = (ymax - ymin)/mmbox + 1;
	nz = (zmax - zmin)/mmbox + 1;
	nodelist = (MMNODE(*)[])malloc( nx*ny*nz * sizeof( MMNODE ));
	if( nodelist == NULL)
	{ aaerror("cannot allocate node memory in mm_fv_update (cannot do it)\n"); return 0; }
	}
	for( ix=0; ix< nx; ix++)
	for( iy=0; iy< ny; iy++)
	for( iz=0; iz< nz; iz++)
	{
	inode = ((iz*ny)+iy)*nx + ix;
	(*nodelist)[inode].xc = ix*mmbox + .5*mmbox + xmin;
	(*nodelist)[inode].yc = iy*mmbox + .5*mmbox + ymin;
	(*nodelist)[inode].zc = iz*mmbox + .5*mmbox + zmin;
	}
	for( ii=0; ii < nx*ny*nz; ii++)
	{
	(*nodelist)[ii].sqp = 0.;
	(*nodelist)[ii].sa = 0.;
	(*nodelist)[ii].xa = 0.;
	(*nodelist)[ii].ya = 0.;
	(*nodelist)[ii].za = 0.;
	(*nodelist)[ii].q100 = 0.;
	(*nodelist)[ii].q010 = 0.;
	(*nodelist)[ii].q001 = 0.;
	(*nodelist)[ii].q200 = 0.;
	(*nodelist)[ii].q020 = 0.;
	(*nodelist)[ii].q002 = 0.;
	(*nodelist)[ii].q101 = 0.;
	(*nodelist)[ii].q110 = 0.;
	(*nodelist)[ii].q011 = 0.;
	(*nodelist)[ii].q300 = 0.;
	(*nodelist)[ii].q030 = 0.;
	(*nodelist)[ii].q003 = 0.;
	(*nodelist)[ii].q210 = 0.;
	(*nodelist)[ii].q120 = 0.;
	(*nodelist)[ii].q201 = 0.;
	(*nodelist)[ii].q102 = 0.;
	(*nodelist)[ii].q021 = 0.;
	(*nodelist)[ii].q012 = 0.;
	(*nodelist)[ii].q111 = 0.;
#ifdef FOURTH
	(*nodelist)[ii].q400 = 0.;
	(*nodelist)[ii].q040 = 0.;
	(*nodelist)[ii].q004 = 0.;
	(*nodelist)[ii].q310 = 0.;
	(*nodelist)[ii].q130 = 0.;
	(*nodelist)[ii].q301 = 0.;
	(*nodelist)[ii].q103 = 0.;
	(*nodelist)[ii].q031 = 0.;
	(*nodelist)[ii].q013 = 0.;
	(*nodelist)[ii].q220 = 0.;
	(*nodelist)[ii].q202 = 0.;
	(*nodelist)[ii].q022 = 0.;
	(*nodelist)[ii].q211 = 0.;
	(*nodelist)[ii].q121 = 0.;
	(*nodelist)[ii].q112 = 0.;
#endif
#ifdef FIFTH
	(*nodelist)[ii].q500 = 0.;
	(*nodelist)[ii].q050 = 0.;
	(*nodelist)[ii].q005 = 0.;
	(*nodelist)[ii].q410 = 0.;
	(*nodelist)[ii].q140 = 0.;
	(*nodelist)[ii].q401 = 0.;
	(*nodelist)[ii].q104 = 0.;
	(*nodelist)[ii].q041 = 0.;
	(*nodelist)[ii].q014 = 0.;
	(*nodelist)[ii].q320 = 0.;
	(*nodelist)[ii].q230 = 0.;
	(*nodelist)[ii].q302 = 0.;
	(*nodelist)[ii].q203 = 0.;
	(*nodelist)[ii].q032 = 0.;
	(*nodelist)[ii].q023 = 0.;
	(*nodelist)[ii].q221 = 0.;
	(*nodelist)[ii].q212 = 0.;
	(*nodelist)[ii].q122 = 0.;
	(*nodelist)[ii].q311 = 0.;
	(*nodelist)[ii].q131 = 0.;
	(*nodelist)[ii].q113 = 0.;
#endif
	(*nodelist)[ii].first = -1;
	(*nodelist)[ii].last = -1;
	(*nodelist)[ii].innode = 0;
	}
/* now decide for each atom who he belongs to */	
	for( ii=0; ii< imax; ii++)
	{
	j = ii*4;
	ix = ((*vector)[j] - xmin )/mmbox;
	iy = ((*vector)[j+1] - ymin )/mmbox;
	iz = ((*vector)[j+2] - zmin )/mmbox;
	inode = ((iz*ny)+iy)*nx + ix;
	(*atomlist)[ii].which = inode;
/* DEBUG
	printf(" error %f %f %f %d %d %d %d\n",
		(*vector)[j],(*vector)[j+1],(*vector)[j+2],ix,iy,iz,inode);

ENDDEBUG */
	}
/* and generate the links */ 
	for( inode = 0; inode < nx*ny*nz; inode++)
	{
	/* first find the first atom which belongs to me */
	for( ii = 0; ii< imax; ii++)
	{
		if( (*atomlist)[ii].which == inode)
		{
		(*nodelist)[inode].first = ii;
		(*nodelist)[inode].last = ii;
		(*nodelist)[inode].innode += 1;
		ap = (*atomlist)[ii].who;
		break;
		}
	}
	/* only if i'm not null */
	if( ii != imax )
	{
	for( ii= (*nodelist)[inode].first; ii < imax; ii++)
	{
		if( (*atomlist)[ii].which == inode)
		{
		(*atomlist)[(*nodelist)[inode].last].next  = ii;
		(*nodelist)[inode].last = ii;
		(*nodelist)[inode].innode += 1;
		ap = (*atomlist)[ii].who;
		xt = ap->x + lambda*ap->dx - (*nodelist)[inode].xc;
		yt = ap->y + lambda*ap->dy - (*nodelist)[inode].yc;
		zt = ap->z + lambda*ap->dz - (*nodelist)[inode].zc;
		(*nodelist)[inode].sqp +=  ap->q;
		(*nodelist)[inode].sa +=  ap->a;
		(*nodelist)[inode].xa +=  ap->a*xt;
		(*nodelist)[inode].ya +=  ap->a*yt;
		(*nodelist)[inode].za +=  ap->a*zt;
		xt2 = xt*xt;
		xt3 = xt2*xt;
		xt4 = xt3*xt;
		yt2 = yt*yt;
		yt3 = yt2*yt;
		yt4 = yt3*yt;
		zt2 = zt*zt;
		zt3 = zt2*zt;
		zt4 = zt3*zt;
	(*nodelist)[inode].q100 += ap->q*xt;
	(*nodelist)[inode].q010 += ap->q*yt;
	(*nodelist)[inode].q001 += ap->q*zt;
	(*nodelist)[inode].q200 += ap->q*xt2;
	(*nodelist)[inode].q020 += ap->q*yt2;
	(*nodelist)[inode].q002 += ap->q*zt2;
	(*nodelist)[inode].q101 += ap->q*xt*zt;
	(*nodelist)[inode].q110 += ap->q*xt*yt;
	(*nodelist)[inode].q011 += ap->q*yt*zt;
	(*nodelist)[inode].q300 += ap->q*xt3;
	(*nodelist)[inode].q030 += ap->q*yt3;
	(*nodelist)[inode].q003 += ap->q*zt3;
	(*nodelist)[inode].q210 += ap->q*xt2*yt;
	(*nodelist)[inode].q120 += ap->q*xt*yt2;
	(*nodelist)[inode].q201 += ap->q*xt2*zt;
	(*nodelist)[inode].q102 += ap->q*xt*zt2;
	(*nodelist)[inode].q021 += ap->q*yt2*zt;
	(*nodelist)[inode].q012 += ap->q*yt*zt2;
	(*nodelist)[inode].q111 += ap->q*xt*yt*zt;
#ifdef FOURTH
	(*nodelist)[inode].q400 += ap->q*xt4;
	(*nodelist)[inode].q040 += ap->q*yt4;
	(*nodelist)[inode].q004 += ap->q*zt4;
	(*nodelist)[inode].q310 += ap->q*xt3*yt;
	(*nodelist)[inode].q130 += ap->q*xt*yt3;
	(*nodelist)[inode].q301 += ap->q*xt3*zt;
	(*nodelist)[inode].q103 += ap->q*xt*zt3;
	(*nodelist)[inode].q031 += ap->q*yt3*zt;
	(*nodelist)[inode].q013 += ap->q*yt*zt3;
	(*nodelist)[inode].q220 += ap->q*xt2*yt2;
	(*nodelist)[inode].q202 += ap->q*xt2*zt2;
	(*nodelist)[inode].q022 += ap->q*yt2*zt2;
	(*nodelist)[inode].q211 += ap->q*xt2*yt*zt;
	(*nodelist)[inode].q121 += ap->q*xt*yt2*zt;
	(*nodelist)[inode].q112 += ap->q*xt*yt*zt2;
#endif
#ifdef FIFTH
	(*nodelist)[inode].q500 += ap->q*xt4*xt;
	(*nodelist)[inode].q050 += ap->q*yt4*yt;
	(*nodelist)[inode].q005 += ap->q*zt4*zt;
	(*nodelist)[inode].q410 += ap->q*xt4*yt;
	(*nodelist)[inode].q140 += ap->q*yt4*xt;
	(*nodelist)[inode].q401 += ap->q*xt4*zt;
	(*nodelist)[inode].q104 += ap->q*zt4*xt;
	(*nodelist)[inode].q041 += ap->q*yt4*zt;
	(*nodelist)[inode].q014 += ap->q*zt4*yt;
	(*nodelist)[inode].q320 += ap->q*xt3*yt2;
	(*nodelist)[inode].q230 += ap->q*yt3*xt2;
	(*nodelist)[inode].q302 += ap->q*xt3*zt2;
	(*nodelist)[inode].q203 += ap->q*zt3*xt2;
	(*nodelist)[inode].q032 += ap->q*yt3*zt2;
	(*nodelist)[inode].q023 += ap->q*zt3*yt2;
	(*nodelist)[inode].q221 += ap->q*xt2*yt2*zt;
	(*nodelist)[inode].q212 += ap->q*xt2*yt*zt2;
	(*nodelist)[inode].q122 += ap->q*xt*yt2*zt2;
	(*nodelist)[inode].q311 += ap->q*xt3*yt*zt;
	(*nodelist)[inode].q131 += ap->q*xt*yt3*zt;
	(*nodelist)[inode].q113 += ap->q*xt*yt*zt3;
#endif
		}
	}/* ii */
	}/* checking if ii != imax */
	}/* inode */
/* and now (almost done with the MM setup)
* normalize the accumulated nodal data */
	/* multiplied by .5 to correct for double counting */
	k = dielectric *.5;
	xt = .5/3.;
	yt = xt/4.;
	zt = yt/5.;
	for( ii = 0; ii < nx*ny*nz; ii ++)
	{
	(*nodelist)[ii].sqp *= k;
	(*nodelist)[ii].q100 *= k;
	(*nodelist)[ii].q010 *= k;
	(*nodelist)[ii].q001 *= k;
	(*nodelist)[ii].q200 *= .5*k;
	(*nodelist)[ii].q020 *= .5*k;
	(*nodelist)[ii].q002 *= .5*k;
	(*nodelist)[ii].q101 *= k;
	(*nodelist)[ii].q110 *= k;
	(*nodelist)[ii].q011 *= k;
	(*nodelist)[ii].q300 *= xt*k;
	(*nodelist)[ii].q030 *= xt*k;
	(*nodelist)[ii].q003 *= xt*k;
	(*nodelist)[ii].q210 *= 0.5*k;
	(*nodelist)[ii].q120 *= 0.5*k;
	(*nodelist)[ii].q201 *= 0.5*k;
	(*nodelist)[ii].q102 *= 0.5*k;
	(*nodelist)[ii].q021 *= 0.5*k;
	(*nodelist)[ii].q012 *= 0.5*k;
	(*nodelist)[ii].q111 *= k;
#ifdef FOURTH
	(*nodelist)[ii].q400 *= yt*k;
	(*nodelist)[ii].q040 *= yt*k;
	(*nodelist)[ii].q004 *= yt*k;
	(*nodelist)[ii].q310 *= xt*k;
	(*nodelist)[ii].q130 *= xt*k;
	(*nodelist)[ii].q301 *= xt*k;
	(*nodelist)[ii].q103 *= xt*k;
	(*nodelist)[ii].q031 *= xt*k;
	(*nodelist)[ii].q013 *= xt*k;
	(*nodelist)[ii].q220 *= .25*k;
	(*nodelist)[ii].q202 *= .25*k;
	(*nodelist)[ii].q022 *= .25*k;
	(*nodelist)[ii].q211 *= .5*k;
	(*nodelist)[ii].q121 *= .5*k;
	(*nodelist)[ii].q112 *= .5*k;
#endif
#ifdef FIFTH
	(*nodelist)[ii].q500 *= zt*k;
	(*nodelist)[ii].q050 *= zt*k;
	(*nodelist)[ii].q005 *= zt*k;
	(*nodelist)[ii].q410 *= yt*k;
	(*nodelist)[ii].q140 *= yt*k;
	(*nodelist)[ii].q401 *= yt*k;
	(*nodelist)[ii].q104 *= yt*k;
	(*nodelist)[ii].q041 *= yt*k;
	(*nodelist)[ii].q014 *= yt*k;
	(*nodelist)[ii].q320 *= .5*xt*k;
	(*nodelist)[ii].q230 *= .5*xt*k;
	(*nodelist)[ii].q302 *= .5*xt*k;
	(*nodelist)[ii].q203 *= .5*xt*k;
	(*nodelist)[ii].q032 *= .5*xt*k;
	(*nodelist)[ii].q023 *= .5*xt*k;
	(*nodelist)[ii].q221 *= .25*k;
	(*nodelist)[ii].q212 *= .25*k;
	(*nodelist)[ii].q122 *= .25*k;
	(*nodelist)[ii].q311 *= xt*k;
	(*nodelist)[ii].q131 *= xt*k;
	(*nodelist)[ii].q113 *= xt*k;
#endif
	/*debug
	printf("%d %f %f\n",ii,(*nodelist)[ii].sqp,(*nodelist)[ii].q100);
	*/
	if( (*nodelist)[ii].sa != 0.)
	{
	(*nodelist)[ii].xa = (*nodelist)[ii].xa/(*nodelist)[ii].sa;
	(*nodelist)[ii].ya = (*nodelist)[ii].ya/(*nodelist)[ii].sa;
	(*nodelist)[ii].za = (*nodelist)[ii].za/(*nodelist)[ii].sa;
	}
	(*nodelist)[ii].xa += (*nodelist)[ii].xc;
	(*nodelist)[ii].ya += (*nodelist)[ii].yc;
	(*nodelist)[ii].za += (*nodelist)[ii].zc;
/* correct for double counting */
	(*nodelist)[ii].sa  *= .5;
	}

/* initiallization of the mmnodes is done !!! */

/*  initialize the data for every atom */
	for( ii=0; ii< jj; ii++)
	{
	a1 = (*atomall)[ii];
	a1-> px = a1->x + lambda*a1->dx;
	a1-> py = a1->y + lambda*a1->dy;
	a1-> pz = a1->z + lambda*a1->dz;
	a1 -> VP = 0.;
	a1 -> dpx = 0.;
	a1 -> dpy = 0.;
	a1 -> dpz = 0.;
	a1 -> qxx = 0.;
	a1 -> qxy = 0.;
	a1 -> qxz = 0.;
	a1 -> qyy = 0.;
	a1 -> qyz = 0.;
	a1 -> qzz = 0.;
#ifdef CUBIC
	a1 -> qxxx = 0.;
	a1 -> qxxy = 0.;
	a1 -> qxxz = 0.;
	a1 -> qxyy = 0.;
	a1 -> qxyz = 0.;
	a1 -> qxzz = 0.;
	a1 -> qyyy = 0.;
	a1 -> qyyz = 0.;
	a1 -> qyzz = 0.;
	a1 -> qzzz = 0.;
#endif
	for( j=0; j< NCLOSE; j++) 
		a1->close[j] = NULL;

	}/* end of initializations */


	for( ii=0; ii<  jj; ii++)
	{ /* if this is met we update the expansion for this atom */
/*	a1 = (*atomall)[ii];
	atomall will be reused in this loop so we refer to atomlist
	*/
	a1 = (*atomlist)[ii].who;
	inclose = 0;
/* loop over the nodes 
   if the node is mine or a neighbor then use an
   explicit summation
   otherwise use the MM node */
	ix = (a1->px  - xmin )/mmbox ;
	iy = (a1->py  - ymin )/mmbox ;
	iz = (a1->pz  - zmin )/mmbox ;
	naybor[0] = ((iz*ny)+iy)*nx + ix;
	naybor[1] = ((iz*ny)+iy)*nx + ix+1;
	naybor[2] = ((iz*ny)+iy)*nx + ix-1;
	naybor[3] = ((iz*ny)+iy)*nx+nx + ix;
	naybor[4] = ((iz*ny)+iy)*nx-nx + ix;
	naybor[5] = ((iz*ny)+iy)*nx+nx + ix+1;
	naybor[6] = ((iz*ny)+iy)*nx+nx + ix-1;
	naybor[7] = ((iz*ny)+iy)*nx-nx + ix+1;
	naybor[8] = ((iz*ny)+iy)*nx-nx + ix-1;
	naybor[9] = ((iz*ny)+ny+iy)*nx + ix;
	naybor[10] = ((iz*ny)+ny+iy)*nx + ix+1;
	naybor[11] = ((iz*ny)+ny+iy)*nx + ix-1;
	naybor[12] = ((iz*ny)+ny+iy)*nx+nx + ix;
	naybor[13] = ((iz*ny)+ny+iy)*nx-nx + ix;
	naybor[14] = ((iz*ny)+ny+iy)*nx+nx + ix+1;
	naybor[15] = ((iz*ny)+ny+iy)*nx+nx + ix-1;
	naybor[16] = ((iz*ny)+ny+iy)*nx-nx + ix+1;
	naybor[17] = ((iz*ny)+ny+iy)*nx-nx + ix-1;
	naybor[18] = ((iz*ny)-ny+iy)*nx + ix;
	naybor[19] = ((iz*ny)-ny+iy)*nx + ix+1;
	naybor[20] = ((iz*ny)-ny+iy)*nx + ix-1;
	naybor[21] = ((iz*ny)-ny+iy)*nx+nx + ix;
	naybor[22] = ((iz*ny)-ny+iy)*nx-nx + ix;
	naybor[23] = ((iz*ny)-ny+iy)*nx+nx + ix+1;
	naybor[24] = ((iz*ny)-ny+iy)*nx+nx + ix-1;
	naybor[25] = ((iz*ny)-ny+iy)*nx-nx + ix+1;
	naybor[26] = ((iz*ny)-ny+iy)*nx-nx + ix-1;

	for( inode = 0; inode < nx*ny*nz; inode ++)
	{/* loop over all mm nodes */
	/* check the origin */
	for(j=0; j< 27; j++)
	{  
	if( inode == naybor[j]) break; }
	if( j == 27  )
	{ /* then use mm */
	if( (*nodelist)[inode].innode > 0 )
	{
/* $%$%$%$%$  the expansion for f(r) goes here */
/* first the a terms */
	/*
	xt = (*nodelist)[inode].xa - a1->px;
	yt = (*nodelist)[inode].ya - a1->py;
	zt = (*nodelist)[inode].za - a1->pz;
	r = one/(xt*xt + yt*yt + zt*zt);
	r0 = sqrt(r);
	r = r*r*r;
	k = -(*nodelist)[inode].sa *a1->a*r;
	a1->VP  += k/r;
	k *= six*r0; 
	xt *= r0;
	yt *= r0;
	zt *= r0;
	a1->dpx += k*xt;
	a1->dpy += k*yt;
	a1->dpz += k*zt;
	k *= eight*r0;
	a1->qxx -= k *(xt*xt -eightth);
	a1->qxy -= k*xt*yt;
	a1->qxz -= k*xt*zt;
	a1->qyy -= k *(yt*yt -eightth);
	a1->qyz -= k*yt*zt;
	a1->qzz -= k *(zt*zt -eightth);
	*/
/* now do the multipole expansion for the electrostatic terms */
/* note that dielectric is included in the multipole expansion */
	xt = (*nodelist)[inode].xc - a1->px;
	yt = (*nodelist)[inode].yc - a1->py;
	zt = (*nodelist)[inode].zc - a1->pz;
	r = one/(xt*xt + yt*yt + zt*zt);
	r0 = sqrt(r);
	c1 =  -r*r0;
	c2 = -three*c1*r;
	c3 = -five*c2*r;
	c4 = -seven*c3*r;
	c5 = -nine*c4*r;
	xt2 = xt*xt;
	xt3 = xt2*xt;
	xt4 = xt3*xt;
	yt2 = yt*yt;
	yt3 = yt2*yt;
	yt4 = yt3*yt;
	zt2 = zt*zt;
	zt3 = zt2*zt;
	zt4 = zt3*zt;
	a1->VP += (*nodelist)[inode].sqp*a1->q*r0;
	k = c1*a1->q*xt;
	a1->VP += k*(*nodelist)[inode].q100;
	a1->dpx += k*(*nodelist)[inode].sqp;
	k = c1*a1->q*yt;
	a1->VP += k*(*nodelist)[inode].q010;
	a1->dpy += k*(*nodelist)[inode].sqp;
	k = c1*a1->q*zt;
	a1->VP += k*(*nodelist)[inode].q001;
	a1->dpz += k*(*nodelist)[inode].sqp;
/* n=2 */
	k = (c2*xt2 +c1)*a1->q;
	a1->VP += k*(*nodelist)[inode].q200;
	a1->dpx += k*(*nodelist)[inode].q100;
	a1->qxx += k*(*nodelist)[inode].sqp;
	k = (c2*yt2 +c1)*a1->q;
	a1->VP += k*(*nodelist)[inode].q020;
	a1->dpy += k*(*nodelist)[inode].q010;
	a1->qyy += k*(*nodelist)[inode].sqp;
	k = (c2*zt2 +c1)*a1->q;
	a1->VP += k*(*nodelist)[inode].q002;
	a1->dpz += k*(*nodelist)[inode].q001;
	a1->qzz += k*(*nodelist)[inode].sqp;
	k = c2*xt*yt*a1->q;
	a1->VP += k*(*nodelist)[inode].q110;
	a1->dpx += k*(*nodelist)[inode].q010;
	a1->dpy += k*(*nodelist)[inode].q100;
	a1->qxy += k*(*nodelist)[inode].sqp;
	k = c2*xt*zt*a1->q;
	a1->VP += k*(*nodelist)[inode].q101;
	a1->dpx += k*(*nodelist)[inode].q001;
	a1->dpz += k*(*nodelist)[inode].q100;
	a1->qxz += k*(*nodelist)[inode].sqp;
	k = c2*yt*zt*a1->q;
	a1->VP += k*(*nodelist)[inode].q011;
	a1->dpy += k*(*nodelist)[inode].q001;
	a1->dpz += k*(*nodelist)[inode].q010;
	a1->qyz += k*(*nodelist)[inode].sqp;
/* n=3 */
	k = (c3*xt3 +3*c2*xt)*a1->q;
	a1->VP += k*(*nodelist)[inode].q300;
	a1->dpx += k*(*nodelist)[inode].q200;
	a1->qxx += k*(*nodelist)[inode].q100;
	k = (c3*yt3 +3*c2*yt)*a1->q;
	a1->VP += k*(*nodelist)[inode].q030;
	a1->dpy += k*(*nodelist)[inode].q020;
	a1->qyy += k*(*nodelist)[inode].q010;
	k = (c3*zt3 +3*c2*zt)*a1->q;
	a1->VP += k*(*nodelist)[inode].q003;
	a1->dpz += k*(*nodelist)[inode].q002;
	a1->qzz += k*(*nodelist)[inode].q001;
	k = (c3*xt2*yt+c2*yt)*a1->q;
	a1->VP += k*(*nodelist)[inode].q210;
	a1->dpx += k*(*nodelist)[inode].q110;
	a1->dpy += k*(*nodelist)[inode].q200;
	a1->qxx += k*(*nodelist)[inode].q010;
	a1->qxy += k*(*nodelist)[inode].q100;
	k = (c3*yt2*xt+c2*xt)*a1->q;
	a1->VP += k*(*nodelist)[inode].q120;
	a1->dpx += k*(*nodelist)[inode].q020;
	a1->dpy += k*(*nodelist)[inode].q110;
	a1->qyy += k*(*nodelist)[inode].q100;
	a1->qxy += k*(*nodelist)[inode].q010;
	k = (c3*xt2*zt+c2*zt)*a1->q;
	a1->VP += k*(*nodelist)[inode].q201;
	a1->dpx += k*(*nodelist)[inode].q101;
	a1->dpz += k*(*nodelist)[inode].q200;
	a1->qxx += k*(*nodelist)[inode].q001;
	a1->qxz += k*(*nodelist)[inode].q100;
	k = (c3*zt2*xt+c2*xt)*a1->q;
	a1->VP += k*(*nodelist)[inode].q102;
	a1->dpx += k*(*nodelist)[inode].q002;
	a1->dpz += k*(*nodelist)[inode].q101;
	a1->qzz += k*(*nodelist)[inode].q100;
	a1->qxz += k*(*nodelist)[inode].q001;
	k = (c3*yt2*zt+c2*zt)*a1->q;
	a1->VP += k*(*nodelist)[inode].q021;
	a1->dpy += k*(*nodelist)[inode].q011;
	a1->dpz += k*(*nodelist)[inode].q020;
	a1->qyy += k*(*nodelist)[inode].q001;
	a1->qyz += k*(*nodelist)[inode].q010;
	k = (c3*zt2*yt+c2*yt)*a1->q;
	a1->VP += k*(*nodelist)[inode].q012;
	a1->dpy += k*(*nodelist)[inode].q002;
	a1->dpz += k*(*nodelist)[inode].q011;
	a1->qzz += k*(*nodelist)[inode].q010;
	a1->qyz += k*(*nodelist)[inode].q001;
	k = (c3*zt*yt*xt)*a1->q;
	a1->VP += k*(*nodelist)[inode].q111;
	a1->dpx += k*(*nodelist)[inode].q011;
	a1->dpy += k*(*nodelist)[inode].q101;
	a1->dpz += k*(*nodelist)[inode].q110;
/* n=4 */
#ifdef FOURTH
	k = (c4*xt4 +six*c3*(xt2) +three*c2)*a1->q;
	a1->VP += k*(*nodelist)[inode].q400;
	a1->dpx += k*(*nodelist)[inode].q300;
	a1->qxx += k*(*nodelist)[inode].q200;
	k = (c4*yt4 +six*c3*(yt2) +three*c2)*a1->q;
	a1->VP += k*(*nodelist)[inode].q040;
	a1->dpy += k*(*nodelist)[inode].q030;
	a1->qyy += k*(*nodelist)[inode].q020;
	k = (c4*zt4 +six*c3*(zt2) +three*c2)*a1->q;
	a1->VP += k*(*nodelist)[inode].q004;
	a1->dpz += k*(*nodelist)[inode].q003;
	a1->qzz += k*(*nodelist)[inode].q002;
	k = (c4*xt3*yt + three*c3*xt*yt)*a1->q;
	a1->VP += k*(*nodelist)[inode].q310;
	a1->dpx += k*(*nodelist)[inode].q210;
	a1->dpy += k*(*nodelist)[inode].q300;
	a1->qxx += k*(*nodelist)[inode].q110;
	a1->qxy += k*(*nodelist)[inode].q200;
	k = (c4*yt3*xt + three*c3*xt*yt)*a1->q;
	a1->VP += k*(*nodelist)[inode].q130;
	a1->dpx += k*(*nodelist)[inode].q030;
	a1->dpy += k*(*nodelist)[inode].q120;
	a1->qyy += k*(*nodelist)[inode].q110;
	a1->qxy += k*(*nodelist)[inode].q020;
	k = (c4*xt3*zt + three*c3*xt*zt)*a1->q;
	a1->VP += k*(*nodelist)[inode].q301;
	a1->dpx += k*(*nodelist)[inode].q201;
	a1->dpz += k*(*nodelist)[inode].q300;
	a1->qxx += k*(*nodelist)[inode].q101;
	a1->qxz += k*(*nodelist)[inode].q200;
	k = (c4*zt3*yt + three*c3*xt*yt)*a1->q;
	a1->VP += k*(*nodelist)[inode].q103;
	a1->dpx += k*(*nodelist)[inode].q003;
	a1->dpz += k*(*nodelist)[inode].q102;
	a1->qzz += k*(*nodelist)[inode].q101;
	a1->qxz += k*(*nodelist)[inode].q002;
	k = (c4*yt3*zt + three*c3*zt*yt)*a1->q;
	a1->VP += k*(*nodelist)[inode].q031;
	a1->dpz += k*(*nodelist)[inode].q030;
	a1->dpy += k*(*nodelist)[inode].q021;
	a1->qyy += k*(*nodelist)[inode].q011;
	a1->qyz += k*(*nodelist)[inode].q020;
	k = (c4*zt3*yt + three*c3*zt*yt)*a1->q;
	a1->VP += k*(*nodelist)[inode].q013;
	a1->dpz += k*(*nodelist)[inode].q012;
	a1->dpy += k*(*nodelist)[inode].q003;
	a1->qzz += k*(*nodelist)[inode].q011;
	a1->qyz += k*(*nodelist)[inode].q002;
	k = (c4*xt2*yt2 + c3*(xt2+yt2) +c2)*a1->q;
	a1->VP += k*(*nodelist)[inode].q220;
	a1->dpx += k*(*nodelist)[inode].q120;
	a1->dpy += k*(*nodelist)[inode].q210;
	a1->qxx += k*(*nodelist)[inode].q020;
	a1->qyy += k*(*nodelist)[inode].q200;
	a1->qxy += k*(*nodelist)[inode].q110;
	k = (c4*xt2*zt2 + c3*(xt2+zt2) +c2)*a1->q;
	a1->VP += k*(*nodelist)[inode].q202;
	a1->dpx += k*(*nodelist)[inode].q102;
	a1->dpz += k*(*nodelist)[inode].q201;
	a1->qxx += k*(*nodelist)[inode].q002;
	a1->qzz += k*(*nodelist)[inode].q200;
	a1->qxz += k*(*nodelist)[inode].q101;
	k = (c4*zt2*yt2 + c3*(zt2+yt2) +c2)*a1->q;
	a1->VP += k*(*nodelist)[inode].q022;
	a1->dpz += k*(*nodelist)[inode].q021;
	a1->dpy += k*(*nodelist)[inode].q012;
	a1->qzz += k*(*nodelist)[inode].q020;
	a1->qyy += k*(*nodelist)[inode].q002;
	a1->qyz += k*(*nodelist)[inode].q011;
	k = (c4*xt2*yt*zt +c3*yt*zt)*a1->q;
	a1->VP += k*(*nodelist)[inode].q211;
	a1->dpz += k*(*nodelist)[inode].q210;
	a1->dpy += k*(*nodelist)[inode].q201;
	a1->dpx += k*(*nodelist)[inode].q111;
	a1->qxx += k*(*nodelist)[inode].q011;
	a1->qxy += k*(*nodelist)[inode].q101;
	a1->qyz += k*(*nodelist)[inode].q200;
	a1->qxz += k*(*nodelist)[inode].q110;
	k = (c4*xt*yt2*zt +c3*xt*zt)*a1->q;
	a1->VP += k*(*nodelist)[inode].q121;
	a1->dpz += k*(*nodelist)[inode].q120;
	a1->dpy += k*(*nodelist)[inode].q111;
	a1->dpx += k*(*nodelist)[inode].q021;
	a1->qyy += k*(*nodelist)[inode].q101;
	a1->qxy += k*(*nodelist)[inode].q011;
	a1->qyz += k*(*nodelist)[inode].q110;
	a1->qxz += k*(*nodelist)[inode].q020;
	k = (c4*xt*yt*zt2 +c3*yt*xt)*a1->q;
	a1->VP += k*(*nodelist)[inode].q112;
	a1->dpz += k*(*nodelist)[inode].q111;
	a1->dpy += k*(*nodelist)[inode].q102;
	a1->dpx += k*(*nodelist)[inode].q012;
	a1->qzz += k*(*nodelist)[inode].q110;
	a1->qxy += k*(*nodelist)[inode].q002;
	a1->qxz += k*(*nodelist)[inode].q011;
	a1->qyz += k*(*nodelist)[inode].q101;
#endif
/* n=5 */
#ifdef FIFTH
	k = ((c5*xt+9*c4)*xt4  +15*c3*xt)*a1->q;
	a1->VP += k*(*nodelist)[inode].q500;
	a1->dpx += k*(*nodelist)[inode].q400;
	a1->qxx += k*(*nodelist)[inode].q300;
	k = ((c5*yt+9*c4)*yt4  +15*c3*yt)*a1->q;
	a1->VP += k*(*nodelist)[inode].q050;
	a1->dpy += k*(*nodelist)[inode].q040;
	a1->qyy += k*(*nodelist)[inode].q030;
	k = ((c5*zt+9*c4)*zt4  +15*c3*zt)*a1->q;
	a1->VP += k*(*nodelist)[inode].q005;
	a1->dpz += k*(*nodelist)[inode].q004;
	a1->qzz += k*(*nodelist)[inode].q003;
	k = (c5*xt4+six*c4*xt2 +three*c3)*yt*a1->q;
	a1->VP += k*(*nodelist)[inode].q410;
	a1->dpx += k*(*nodelist)[inode].q310;
	a1->dpy += k*(*nodelist)[inode].q400;
	a1->qxx += k*(*nodelist)[inode].q210;
	a1->qxy += k*(*nodelist)[inode].q300;
	k = (c5*yt4+six*c4*yt2 +three*c3)*xt*a1->q;
	a1->VP += k*(*nodelist)[inode].q140;
	a1->dpx += k*(*nodelist)[inode].q040;
	a1->dpy += k*(*nodelist)[inode].q130;
	a1->qyy += k*(*nodelist)[inode].q120;
	a1->qxy += k*(*nodelist)[inode].q030;
	k = (c5*xt4+six*c4*xt2 +three*c3)*zt*a1->q;
	a1->VP += k*(*nodelist)[inode].q401;
	a1->dpx += k*(*nodelist)[inode].q301;
	a1->dpz += k*(*nodelist)[inode].q400;
	a1->qxx += k*(*nodelist)[inode].q201;
	a1->qxz += k*(*nodelist)[inode].q300;
	k = (c5*zt4+six*c4*zt2 +three*c3)*xt*a1->q;
	a1->VP += k*(*nodelist)[inode].q104;
	a1->dpx += k*(*nodelist)[inode].q004;
	a1->dpz += k*(*nodelist)[inode].q103;
	a1->qzz += k*(*nodelist)[inode].q102;
	a1->qxz += k*(*nodelist)[inode].q003;
	k = (c5*yt4+six*c4*yt2 +three*c3)*zt*a1->q;
	a1->VP += k*(*nodelist)[inode].q041;
	a1->dpy += k*(*nodelist)[inode].q031;
	a1->dpz += k*(*nodelist)[inode].q040;
	a1->qyy += k*(*nodelist)[inode].q021;
	a1->qyz += k*(*nodelist)[inode].q030;
	k = (c5*zt4+six*c4*zt2 +three*c3)*yt*a1->q;
	a1->VP += k*(*nodelist)[inode].q014;
	a1->dpy += k*(*nodelist)[inode].q004;
	a1->dpz += k*(*nodelist)[inode].q013;
	a1->qzz += k*(*nodelist)[inode].q012;
	a1->qyz += k*(*nodelist)[inode].q003;
	k = (c5*xt3*yt2 +c4*(three*xt*yt2-xt3) +three*c3*xt)*a1->q;
	a1->VP += k*(*nodelist)[inode].q320;
	a1->dpx += k*(*nodelist)[inode].q220;
	a1->dpy += k*(*nodelist)[inode].q310;
	a1->qxx += k*(*nodelist)[inode].q120;
	a1->qxy += k*(*nodelist)[inode].q210;
	a1->qyy += k*(*nodelist)[inode].q300;
	k = (c5*yt3*xt2 +c4*(three*yt*xt2-yt3) +three*c3*yt)*a1->q;
	a1->VP += k*(*nodelist)[inode].q230;
	a1->dpx += k*(*nodelist)[inode].q130;
	a1->dpy += k*(*nodelist)[inode].q220;
	a1->qxx += k*(*nodelist)[inode].q030;
	a1->qxy += k*(*nodelist)[inode].q120;
	a1->qyy += k*(*nodelist)[inode].q210;
	k = (c5*xt3*zt2 +c4*(three*xt*zt2-xt3) +three*c3*xt)*a1->q;
	a1->VP += k*(*nodelist)[inode].q302;
	a1->dpx += k*(*nodelist)[inode].q202;
	a1->dpz += k*(*nodelist)[inode].q301;
	a1->qxx += k*(*nodelist)[inode].q102;
	a1->qxz += k*(*nodelist)[inode].q201;
	a1->qzz += k*(*nodelist)[inode].q300;
	k = (c5*zt3*xt2 +c4*(three*zt*xt2-zt3) +three*c3*zt)*a1->q;
	a1->VP += k*(*nodelist)[inode].q203;
	a1->dpx += k*(*nodelist)[inode].q103;
	a1->dpz += k*(*nodelist)[inode].q202;
	a1->qxx += k*(*nodelist)[inode].q003;
	a1->qxz += k*(*nodelist)[inode].q102;
	a1->qzz += k*(*nodelist)[inode].q201;
	k = (c5*yt3*zt2 +c4*(three*yt*zt2-yt3) +three*c3*yt)*a1->q;
	a1->VP += k*(*nodelist)[inode].q032;
	a1->dpy += k*(*nodelist)[inode].q022;
	a1->dpz += k*(*nodelist)[inode].q031;
	a1->qyy += k*(*nodelist)[inode].q012;
	a1->qyz += k*(*nodelist)[inode].q021;
	a1->qzz += k*(*nodelist)[inode].q030;
	k = (c5*zt3*yt2 +c4*(three*zt*yt2-zt3) +three*c3*zt)*a1->q;
	a1->VP += k*(*nodelist)[inode].q023;
	a1->dpy += k*(*nodelist)[inode].q013;
	a1->dpz += k*(*nodelist)[inode].q022;
	a1->qyy += k*(*nodelist)[inode].q003;
	a1->qyz += k*(*nodelist)[inode].q012;
	a1->qzz += k*(*nodelist)[inode].q021;
	k = (c5*xt2*yt2 +c4*(xt2+yt2) +c3)*zt*a1->q;
	a1->VP += k*(*nodelist)[inode].q221;
	a1->dpx += k*(*nodelist)[inode].q121;
	a1->dpy += k*(*nodelist)[inode].q211;
	a1->dpz += k*(*nodelist)[inode].q220;
	a1->qxx += k*(*nodelist)[inode].q021;
	a1->qyy += k*(*nodelist)[inode].q201;
	a1->qxz += k*(*nodelist)[inode].q120;
	a1->qyz += k*(*nodelist)[inode].q210;
	k = (c5*xt2*zt2 +c4*(xt2+zt2) +c3)*yt*a1->q;
	a1->VP += k*(*nodelist)[inode].q212;
	a1->dpx += k*(*nodelist)[inode].q112;
	a1->dpy += k*(*nodelist)[inode].q202;
	a1->dpz += k*(*nodelist)[inode].q211;
	a1->qxx += k*(*nodelist)[inode].q012;
	a1->qzz += k*(*nodelist)[inode].q210;
	a1->qxz += k*(*nodelist)[inode].q111;
	a1->qyz += k*(*nodelist)[inode].q201;
	k = (c5*zt2*yt2 +c4*(zt2+yt2) +c3)*xt*a1->q;
	a1->VP += k*(*nodelist)[inode].q122;
	a1->dpx += k*(*nodelist)[inode].q022;
	a1->dpy += k*(*nodelist)[inode].q112;
	a1->dpz += k*(*nodelist)[inode].q121;
	a1->qzz += k*(*nodelist)[inode].q120;
	a1->qyy += k*(*nodelist)[inode].q102;
	a1->qxy += k*(*nodelist)[inode].q022;
	a1->qxz += k*(*nodelist)[inode].q022;
	k = (c5*xt3+three*c4*xt)*yt*zt*a1->q;
	a1->VP += k*(*nodelist)[inode].q311;
	a1->dpx += k*(*nodelist)[inode].q211;
	a1->dpy += k*(*nodelist)[inode].q301;
	a1->dpz += k*(*nodelist)[inode].q310;
	a1->qxx += k*(*nodelist)[inode].q211;
	a1->qxy += k*(*nodelist)[inode].q201;
	a1->qxz += k*(*nodelist)[inode].q210;
	k = (c5*yt3+three*c4*yt)*xt*zt*a1->q;
	a1->VP += k*(*nodelist)[inode].q131;
	a1->dpx += k*(*nodelist)[inode].q031;
	a1->dpy += k*(*nodelist)[inode].q121;
	a1->dpz += k*(*nodelist)[inode].q130;
	a1->qyy += k*(*nodelist)[inode].q111;
	a1->qxy += k*(*nodelist)[inode].q021;
	a1->qyz += k*(*nodelist)[inode].q120;
	k = (c5*zt3+three*c4*zt)*yt*xt*a1->q;
	a1->VP += k*(*nodelist)[inode].q113;
	a1->dpx += k*(*nodelist)[inode].q013;
	a1->dpy += k*(*nodelist)[inode].q103;
	a1->dpz += k*(*nodelist)[inode].q112;
	a1->qzz += k*(*nodelist)[inode].q111;
	a1->qyz += k*(*nodelist)[inode].q102;
	a1->qxz += k*(*nodelist)[inode].q012;
#endif

	} /* if innode > 0  end if */
	} else if( (*nodelist)[inode].innode > 0) 
		{ /* if not mm use explicit */
/* first load the atoms onto atomall */
	imax = 0;
	i = (*nodelist)[inode].first;
	 if( (*nodelist)[inode].innode > 0  && ((*atomlist)[i].who)->serial > a1->serial)
	 {  (*atomall)[imax++] = (*atomlist)[i].who;}
	for( j=1; j< (*nodelist)[inode].innode -1 ; j++)
	{
	 i = (*atomlist)[i].next;
	 if( ((*atomlist)[i].who)->serial > a1->serial)
	 {  (*atomall)[imax++] = (*atomlist)[i].who;}
	}
	/*
	for( j=0; j< jj; j++)
	{
		if( (*atomlist)[j].which == inode && 
		    (*atomlist)[j].who->serial > a1->serial)
		    {(*atomall)[imax] = (*atomlist)[j].who;
		    imax+= 1;}
	}
	*/
	for( i=0; i< imax; i++)
	{
	a2 = (*atomall)[i];
	j = i*4;
	(*vector)[j  ] = a2->px - a1->px ;
	(*vector)[j+1] = a2->py - a1->py ;
	(*vector)[j+2] = a2->pz - a1->pz ;
	}
	for( i=0; i< imax; i++)
	{
		j = i*4;
		(*vector)[j+3] = sqrt((*vector)[j]*(*vector)[j] +
				 (*vector)[j+1]*(*vector)[j+1] +
				 (*vector)[j+2]*(*vector)[j+2]);
	}
/* add the new components */
	for( i=0; i< imax; i++)
	{
	a2 = (*atomall)[i];
	for( j=0; j< a1->dontuse; j++)
	{ if( a2 == a1->excluded[j]) goto SKIPNEW;}
	j = i*4;
	if( (*vector)[j+3] > mxcut || inclose > NCLOSE )
	{
	r0 = one/(*vector)[j+3];  
	r = r0*r0;
	r = r*r*r; /* r0^-6 */
	xt = a1->q*a2->q*dielectric*r0;
	yt = a1->a*a2->a*r;
	zt = a1->b*a2->b*r*r;
	k = xt - yt + zt;
	xt = xt*r0; yt = yt*r0; zt = zt*r0;
	k1 = xt - yt*six + zt*twelve;
	xt = xt*r0; yt = yt*r0; zt = zt*r0;
	k2 = xt*three; ka2 = - yt*six*eight; kb2 =  zt*twelve*14;
#ifdef CUBIC
	xt = xt*r0; yt = yt*r0; zt = zt*r0;
	k3 = -xt*5*3; ka3 =   yt*6*8*10 ; kb3 =  -zt*12*14*16;
#endif
	k1 = -k1;
	xt = (*vector)[j]*r0 ;
	yt = (*vector)[j+1]*r0 ;
	zt = (*vector)[j+2] *r0;
	/*
	xt = (*vector)[j] ;
	yt = (*vector)[j+1] ;
	zt = (*vector)[j+2] ;
	*/
	a1->VP += k;
	a2->dpx -= k1*xt;
	a1->dpx += k1*xt;
	a2->dpy -= k1*yt;
	a1->dpy += k1*yt;
	a2->dpz -= k1*zt;
	a1->dpz += k1*zt;
	xt2 = xt*xt; yt2 = yt*yt; zt2 = zt*zt;
	a2->qxx -= k2*(xt2 - third) + ka2*(xt2 - eightth)+kb2*(xt2-fourteenth) ;
	a1->qxx -= k2*(xt2 - third) + ka2*(xt2 - eightth)+kb2*(xt2-fourteenth) ;
	a2->qxy -= (k2+ka2+kb2)*yt*xt;
	a1->qxy -= (k2+ka2+kb2)*yt*xt;
	a2->qxz -= (k2+ka2+kb2)*zt*xt;
	a1->qxz -= (k2+ka2+kb2)*zt*xt;
	a2->qyy -= k2*(yt2 - third) + ka2*(yt2 - eightth)+kb2*(yt2-fourteenth) ;
	a1->qyy -= k2*(yt2 - third) + ka2*(yt2 - eightth)+kb2*(yt2-fourteenth) ;
	a2->qyz -= (k2+ka2+kb2)*yt*zt;
	a1->qyz -= (k2+ka2+kb2)*yt*zt;
	a2->qzz -= k2*(zt2 - third) + ka2*(zt2 - eightth)+kb2*(zt2-fourteenth) ;
	a1->qzz -= k2*(zt2 - third) + ka2*(zt2 - eightth)+kb2*(zt2-fourteenth) ;
#ifdef CUBIC
	a2->qxxx -= k3*(xt*xt*xt - xt*( 9./15 )) ;
	a2->qxxx -= ka3*(xt*xt*xt - xt*( 24./80 )) ;
	a2->qxxx -= kb3*(xt*xt*xt - xt*( 42./(14*18))); 
	a1->qxxx += k3*(xt*xt*xt - xt*( 9./15 )) ;
	a1->qxxx += ka3*(xt*xt*xt - xt*( 24./80 )) ;
	a1->qxxx += kb3*(xt*xt*xt - xt*( 42./(14*18))); 
	a2->qxxy -= k3*(yt*xt*xt - yt*( 6./ 15));
	a2->qxxy -= ka3*(yt*xt*xt - yt*( 11./ 80));
	a2->qxxy -= kb3*(yt*xt*xt - yt*( 17./ (14*18)));
	a1->qxxy += k3*(yt*xt*xt - yt*( 6./ 15));
	a1->qxxy += ka3*(yt*xt*xt - yt*( 11./ 80));
	a1->qxxy += kb3*(yt*xt*xt - yt*( 17./ (14*18)));
	a2->qxxz -= k3*(zt*xt*xt - zt*( 6./ 15));
	a2->qxxz -= ka3*(zt*xt*xt - zt*( 11./ 80));
	a2->qxxz -= kb3*(zt*xt*xt - zt*( 17./ (14*18)));
	a1->qxxz += k3*(zt*xt*xt - zt*( 6./ 15));
	a1->qxxz += ka3*(zt*xt*xt - zt*( 11./ 80));
	a1->qxxz += kb3*(zt*xt*xt - zt*( 17./ (14*18)));
	a2->qxyy -= k3*(yt*yt*xt - xt*( 6./ 15));
	a2->qxyy -= ka3*(yt*yt*xt - xt*( 11./ 80));
	a2->qxyy -= kb3*(yt*yt*xt - xt*( 17./ (14*18)));
	a1->qxyy += k3*(yt*yt*xt - xt*( 6./ 15));
	a1->qxyy += ka3*(yt*yt*xt - xt*( 11./ 80));
	a1->qxyy += kb3*(yt*yt*xt - xt*( 17./ (14*18)));
	a2->qxyz -= (k3+ka3+kb3)*yt*zt*xt;
	a1->qxyz += (k3+ka3+kb3)*yt*zt*xt;
	a2->qxzz -= k3*(zt*zt*xt - xt*( 6./ 15));
	a2->qxzz -= ka3*(zt*zt*xt - xt*( 11./ 80));
	a2->qxzz -= kb3*(zt*zt*xt - xt*( 17./ (14*18)));
	a1->qxzz += k3*(zt*zt*xt - xt*( 6./ 15));
	a1->qxzz += ka3*(zt*zt*xt - xt*( 11./ 80));
	a1->qxzz += kb3*(zt*zt*xt - xt*( 17./ (14*18)));
	a2->qyyy -= k3*(yt*yt*yt - yt*( 9./15 )) ;
	a2->qyyy -= ka3*(yt*yt*yt - yt*( 24./80 )) ;
	a2->qyyy -= kb3*(yt*yt*yt - yt*( 42./(14*18))); 
	a1->qyyy += k3*(yt*yt*yt - yt*( 9./15 )) ;
	a1->qyyy += ka3*(yt*yt*yt - yt*( 24./80 )) ;
	a1->qyyy += kb3*(yt*yt*yt - yt*( 42./(14*18))); 
	a2->qyyz -= k3*(yt*yt*zt - zt*( 6./ 15));
	a2->qyyz -= ka3*(yt*yt*zt - zt*( 11./ 80));
	a2->qyyz -= kb3*(yt*yt*zt - zt*( 17./ (14*18)));
	a1->qyyz += k3*(yt*yt*zt - zt*( 6./ 15));
	a1->qyyz += ka3*(yt*yt*zt - zt*(11./ 80));
	a1->qyyz += kb3*(yt*yt*zt - zt*( 17./ (14*18)));
	a2->qyzz -= k3*(zt*zt*yt - yt*( 6./ 15));
	a2->qyzz -= ka3*(zt*zt*yt - yt*( 11./ 80));
	a2->qyzz -= kb3*(zt*zt*yt - yt*( 17./ (14*18)));
	a1->qyzz += k3*(zt*zt*yt - yt*( 6./ 15));
	a1->qyzz += ka3*(zt*zt*yt - yt*( 11./ 80));
	a1->qyzz += kb3*(zt*zt*yt - yt*( 17./ (14*18)));
	a2->qzzz -= k3*(zt*zt*zt - zt*( 9./15 )) ;
	a2->qzzz -= ka3*(zt*zt*zt - zt*( 24./80 )) ;
	a2->qzzz -= kb3*(zt*zt*zt - zt*( 42./(14*18))); 
	a1->qzzz += k3*(zt*zt*zt - zt*( 9./15 )) ;
	a1->qzzz += ka3*(zt*zt*zt - zt*( 24./80 )) ;
	a1->qzzz += kb3*(zt*zt*zt - zt*( 42./(14*18))); 
#endif
	}else {
	a1->close[inclose++] = (*atomall)[i];
/* debugging
	j = i *4;
	fprintf(stderr," mxcut %f %f inclose %d who %d \n",mxcut,(*vector)[j+3],inclose,(*atomall)[i]->serial);
	fprintf(stderr," vector %f %f %f \n", (*vector)[j],(*vector)[j+1],(*vector)[j+2]);
*/
	if( inclose == NCLOSE)
	{
	aaerror(
	" fv_update_nonbon> too many atoms increase NCLOSE or decrease mxcut");
/*		exit(0); 
*/
	}
	}
SKIPNEW:  j =  j;
	}/* end of loop i */
	}/* end of if in close MM node */
	}/* end of loop inode */
/* merge the non-bond mxcut lists */
        /* SPEC remove following line which causes compiler warnings.
         * The line was a no-op anyway, since double equals is a 
         * compare.  The author of AMMP says that we should
         * NOT change it to a single equals, as that would "cause a
         * failure under certain odd conditions.  The actual array is
         * preinitialized above to NULL and is filled during these
         * routines" - jh/9/24/99 */
	/* a1->close[inclose] == NULL; */
/* set the position */
	a1->px = a1->dx*lambda + a1->x;
	a1->py = a1->dy*lambda + a1->y;
	a1->pz = a1->dz*lambda + a1->z;

	}  /* end of ii loop */
	
	a_inactive_f_zero();

	free( atomlist);
	free( nodelist);
	free( vector);
	free (atomall);
	return 1;

}

	float loop_lambda;
	float r,r0,xt,yt,zt;
	float xt2,xt3,xt4,yt2,yt3,yt4,zt2,zt3,zt4;
	float k,k1,k2,k3,k4,k5;
	float ka2,ka3;
	float kb2,kb3;
	float c1,c2,c3,c4,c5; /* constants for the mm expansion */
	float get_f_variable(char*);
	int inbond,inangle,i_loop;
	ATOM *ap,*a1,*a2,*bonded[10],*angled[10];
	ATOM *a_next(int); /* returns first ATOM when called with -1 */
	int a_number();
	int ii,j,jj,imax;
	int vloop_size;
	float (*vector_loop)[];
/* */
	ATOM *(*atomall)[];
	float mxdq,dielectric,mxcut; 
	float mmbox;
	float xmax,xmin,ymax,ymin,zmax,zmin;
	int nx,ny,nz;
	int ix,iy,iz,inode;
	int naybor[27];
	char line[80];
	MMNODE (*nodelist)[];
	MMATOM (*atomlist)[];
//	pthread_mutex_t mutexes [9582];

void loop_0(){
//	cout<<"imax: "<<imax<<endl;
for(i_loop=0; i_loop< imax; i_loop++)
	{
//		pthread_mutex_init(&mutexes[i_loop], NULL);
		(*atomall)[i_loop] = a_next(i_loop);
		(*atomlist)[i_loop].who = (*atomall)[i_loop];
	}
}
void loop_1(){
	for( ii=0; ii< imax; ii++)
	{
	        a1 = (*atomall)[ii];
		j = ii*4;
	        (*vector_loop)[j] = a1->dx*loop_lambda +a1->x ;
	        (*vector_loop)[j+1] = a1->dy*loop_lambda +a1->y;
	        (*vector_loop)[j+2] = a1->dz*loop_lambda +a1->z;
	}
}
void loop_2(){
	/*SECOND II FOR*/	

	for( ii= 0; ii< imax; ii++)
	{
		j = ii*4;
	  if( xmax < (*vector_loop)[j] ) xmax = (*vector_loop)[j];
	  if( ymax < (*vector_loop)[j+1] ) ymax = (*vector_loop)[j+1];
	  if( zmax < (*vector_loop)[j+2] ) zmax = (*vector_loop)[j+2];
	  if( xmin > (*vector_loop)[j] ) xmin = (*vector_loop)[j];
	  if( ymin > (*vector_loop)[j+1] ) ymin = (*vector_loop)[j+1];
	  if( zmin > (*vector_loop)[j+2] ) zmin = (*vector_loop)[j+2];
	}

}
void loop_3(){
	/*IX, IY, IZ FOR*/
	for( ix=0; ix< nx; ix++)
		for( iy=0; iy< ny; iy++)
			for( iz=0; iz< nz; iz++)
			{
				inode = ((iz*ny)+iy)*nx + ix;
				(*nodelist)[inode].xc = ix*mmbox + .5*mmbox + xmin;
				(*nodelist)[inode].yc = iy*mmbox + .5*mmbox + ymin;
				(*nodelist)[inode].zc = iz*mmbox + .5*mmbox + zmin;
			}
}
void loop_4(){
	/*THIRD II FOR*/
	for( ii=0; ii < nx*ny*nz; ii++)
	{
		(*nodelist)[ii].sqp = 0.;
		(*nodelist)[ii].sa = 0.;
		(*nodelist)[ii].xa = 0.;
		(*nodelist)[ii].ya = 0.;
		(*nodelist)[ii].za = 0.;
		(*nodelist)[ii].q100 = 0.;
		(*nodelist)[ii].q010 = 0.;
		(*nodelist)[ii].q001 = 0.;
		(*nodelist)[ii].q200 = 0.;
		(*nodelist)[ii].q020 = 0.;
		(*nodelist)[ii].q002 = 0.;
		(*nodelist)[ii].q101 = 0.;
		(*nodelist)[ii].q110 = 0.;
		(*nodelist)[ii].q011 = 0.;
		(*nodelist)[ii].q300 = 0.;
		(*nodelist)[ii].q030 = 0.;
		(*nodelist)[ii].q003 = 0.;
		(*nodelist)[ii].q210 = 0.;
		(*nodelist)[ii].q120 = 0.;
		(*nodelist)[ii].q201 = 0.;
		(*nodelist)[ii].q102 = 0.;
		(*nodelist)[ii].q021 = 0.;
		(*nodelist)[ii].q012 = 0.;
		(*nodelist)[ii].q111 = 0.;
#ifdef FOURTH
		(*nodelist)[ii].q400 = 0.;
		(*nodelist)[ii].q040 = 0.;
		(*nodelist)[ii].q004 = 0.;
		(*nodelist)[ii].q310 = 0.;
		(*nodelist)[ii].q130 = 0.;
		(*nodelist)[ii].q301 = 0.;
		(*nodelist)[ii].q103 = 0.;
		(*nodelist)[ii].q031 = 0.;
		(*nodelist)[ii].q013 = 0.;
		(*nodelist)[ii].q220 = 0.;
		(*nodelist)[ii].q202 = 0.;
		(*nodelist)[ii].q022 = 0.;
		(*nodelist)[ii].q211 = 0.;
		(*nodelist)[ii].q121 = 0.;
		(*nodelist)[ii].q112 = 0.;
#endif
#ifdef FIFTH
		(*nodelist)[ii].q500 = 0.;
		(*nodelist)[ii].q050 = 0.;
		(*nodelist)[ii].q005 = 0.;
		(*nodelist)[ii].q410 = 0.;
		(*nodelist)[ii].q140 = 0.;
		(*nodelist)[ii].q401 = 0.;
		(*nodelist)[ii].q104 = 0.;
		(*nodelist)[ii].q041 = 0.;
		(*nodelist)[ii].q014 = 0.;
		(*nodelist)[ii].q320 = 0.;
		(*nodelist)[ii].q230 = 0.;
		(*nodelist)[ii].q302 = 0.;
		(*nodelist)[ii].q203 = 0.;
		(*nodelist)[ii].q032 = 0.;
		(*nodelist)[ii].q023 = 0.;
		(*nodelist)[ii].q221 = 0.;
		(*nodelist)[ii].q212 = 0.;
		(*nodelist)[ii].q122 = 0.;
		(*nodelist)[ii].q311 = 0.;
		(*nodelist)[ii].q131 = 0.;
		(*nodelist)[ii].q113 = 0.;
#endif
		(*nodelist)[ii].first = -1;
		(*nodelist)[ii].last = -1;
		(*nodelist)[ii].innode = 0;
	}
}
void loop_5(){
	for( ii=0; ii< imax; ii++)
	{
		j = ii*4;
		ix = ((*vector_loop)[j] - xmin )/mmbox;
		iy = ((*vector_loop)[j+1] - ymin )/mmbox;
		iz = ((*vector_loop)[j+2] - zmin )/mmbox;
		inode = ((iz*ny)+iy)*nx + ix;
		(*atomlist)[ii].which = inode;
/* DEBUG
	printf(" error %f %f %f %d %d %d %d\n",
		(*vector_loop)[j],(*vector_loop)[j+1],(*vector_loop)[j+2],ix,iy,iz,inode);

ENDDEBUG */
	}

}
void loop_6(){
	for( inode = 0; inode < nx*ny*nz; inode++)
	{
	/* first find the first atom which belongs to me */
		for( ii = 0; ii< imax; ii++)
		{
			if( (*atomlist)[ii].which == inode)
			{
				(*nodelist)[inode].first = ii;
				(*nodelist)[inode].last = ii;
				(*nodelist)[inode].innode += 1;
				ap = (*atomlist)[ii].who;
				break;
			}
		}
	/* only if i_loop'm not null */
		if( ii != imax )
		{	
			for( ii= (*nodelist)[inode].first; ii < imax; ii++)
			{
				if( (*atomlist)[ii].which == inode)
				{
					(*atomlist)[(*nodelist)[inode].last].next  = ii;
					(*nodelist)[inode].last = ii;
					(*nodelist)[inode].innode += 1;
					ap = (*atomlist)[ii].who;
					xt = ap->x + loop_lambda*ap->dx - (*nodelist)[inode].xc;
					yt = ap->y + loop_lambda*ap->dy - (*nodelist)[inode].yc;
					zt = ap->z + loop_lambda*ap->dz - (*nodelist)[inode].zc;
					(*nodelist)[inode].sqp +=  ap->q;
					(*nodelist)[inode].sa +=  ap->a;
					(*nodelist)[inode].xa +=  ap->a*xt;
					(*nodelist)[inode].ya +=  ap->a*yt;
					(*nodelist)[inode].za +=  ap->a*zt;
					xt2 = xt*xt;
					xt3 = xt2*xt;
					xt4 = xt3*xt;
					yt2 = yt*yt;
					yt3 = yt2*yt;
					yt4 = yt3*yt;
					zt2 = zt*zt;
					zt3 = zt2*zt;
					zt4 = zt3*zt;
					(*nodelist)[inode].q100 += ap->q*xt;
					(*nodelist)[inode].q010 += ap->q*yt;
					(*nodelist)[inode].q001 += ap->q*zt;
					(*nodelist)[inode].q200 += ap->q*xt2;
					(*nodelist)[inode].q020 += ap->q*yt2;
					(*nodelist)[inode].q002 += ap->q*zt2;
					(*nodelist)[inode].q101 += ap->q*xt*zt;
					(*nodelist)[inode].q110 += ap->q*xt*yt;
					(*nodelist)[inode].q011 += ap->q*yt*zt;
					(*nodelist)[inode].q300 += ap->q*xt3;
					(*nodelist)[inode].q030 += ap->q*yt3;
					(*nodelist)[inode].q003 += ap->q*zt3;
					(*nodelist)[inode].q210 += ap->q*xt2*yt;
					(*nodelist)[inode].q120 += ap->q*xt*yt2;
					(*nodelist)[inode].q201 += ap->q*xt2*zt;
					(*nodelist)[inode].q102 += ap->q*xt*zt2;
					(*nodelist)[inode].q021 += ap->q*yt2*zt;
					(*nodelist)[inode].q012 += ap->q*yt*zt2;
					(*nodelist)[inode].q111 += ap->q*xt*yt*zt;
#ifdef FOURTH
					(*nodelist)[inode].q400 += ap->q*xt4;
					(*nodelist)[inode].q040 += ap->q*yt4;
					(*nodelist)[inode].q004 += ap->q*zt4;
					(*nodelist)[inode].q310 += ap->q*xt3*yt;
					(*nodelist)[inode].q130 += ap->q*xt*yt3;
					(*nodelist)[inode].q301 += ap->q*xt3*zt;
					(*nodelist)[inode].q103 += ap->q*xt*zt3;
					(*nodelist)[inode].q031 += ap->q*yt3*zt;
					(*nodelist)[inode].q013 += ap->q*yt*zt3;
					(*nodelist)[inode].q220 += ap->q*xt2*yt2;
					(*nodelist)[inode].q202 += ap->q*xt2*zt2;
					(*nodelist)[inode].q022 += ap->q*yt2*zt2;
					(*nodelist)[inode].q211 += ap->q*xt2*yt*zt;
					(*nodelist)[inode].q121 += ap->q*xt*yt2*zt;
					(*nodelist)[inode].q112 += ap->q*xt*yt*zt2;
#endif
#ifdef FIFTH
					(*nodelist)[inode].q500 += ap->q*xt4*xt;
					(*nodelist)[inode].q050 += ap->q*yt4*yt;
					(*nodelist)[inode].q005 += ap->q*zt4*zt;
					(*nodelist)[inode].q410 += ap->q*xt4*yt;
					(*nodelist)[inode].q140 += ap->q*yt4*xt;
					(*nodelist)[inode].q401 += ap->q*xt4*zt;
					(*nodelist)[inode].q104 += ap->q*zt4*xt;
					(*nodelist)[inode].q041 += ap->q*yt4*zt;
					(*nodelist)[inode].q014 += ap->q*zt4*yt;
					(*nodelist)[inode].q320 += ap->q*xt3*yt2;
					(*nodelist)[inode].q230 += ap->q*yt3*xt2;
					(*nodelist)[inode].q302 += ap->q*xt3*zt2;
					(*nodelist)[inode].q203 += ap->q*zt3*xt2;
					(*nodelist)[inode].q032 += ap->q*yt3*zt2;
					(*nodelist)[inode].q023 += ap->q*zt3*yt2;
					(*nodelist)[inode].q221 += ap->q*xt2*yt2*zt;
					(*nodelist)[inode].q212 += ap->q*xt2*yt*zt2;
					(*nodelist)[inode].q122 += ap->q*xt*yt2*zt2;
					(*nodelist)[inode].q311 += ap->q*xt3*yt*zt;
					(*nodelist)[inode].q131 += ap->q*xt*yt3*zt;
					(*nodelist)[inode].q113 += ap->q*xt*yt*zt3;
#endif
				}
			}/* ii */
		}/* checking if ii != imax */
	}/* inode */

	/*END OF THE INODE FOR*/
}
void loop_7(){
	for( ii = 0; ii < nx*ny*nz; ii ++)
	{
		(*nodelist)[ii].sqp *= k;
		(*nodelist)[ii].q100 *= k;
		(*nodelist)[ii].q010 *= k;
		(*nodelist)[ii].q001 *= k;
		(*nodelist)[ii].q200 *= .5*k;
		(*nodelist)[ii].q020 *= .5*k;
		(*nodelist)[ii].q002 *= .5*k;
		(*nodelist)[ii].q101 *= k;
		(*nodelist)[ii].q110 *= k;
		(*nodelist)[ii].q011 *= k;
		(*nodelist)[ii].q300 *= xt*k;
		(*nodelist)[ii].q030 *= xt*k;
		(*nodelist)[ii].q003 *= xt*k;
		(*nodelist)[ii].q210 *= 0.5*k;
		(*nodelist)[ii].q120 *= 0.5*k;
		(*nodelist)[ii].q201 *= 0.5*k;
		(*nodelist)[ii].q102 *= 0.5*k;
		(*nodelist)[ii].q021 *= 0.5*k;
		(*nodelist)[ii].q012 *= 0.5*k;
		(*nodelist)[ii].q111 *= k;
#ifdef FOURTH
		(*nodelist)[ii].q400 *= yt*k;
		(*nodelist)[ii].q040 *= yt*k;
		(*nodelist)[ii].q004 *= yt*k;
		(*nodelist)[ii].q310 *= xt*k;
		(*nodelist)[ii].q130 *= xt*k;
		(*nodelist)[ii].q301 *= xt*k;
		(*nodelist)[ii].q103 *= xt*k;
		(*nodelist)[ii].q031 *= xt*k;
		(*nodelist)[ii].q013 *= xt*k;
		(*nodelist)[ii].q220 *= .25*k;
		(*nodelist)[ii].q202 *= .25*k;
		(*nodelist)[ii].q022 *= .25*k;
		(*nodelist)[ii].q211 *= .5*k;
		(*nodelist)[ii].q121 *= .5*k;
		(*nodelist)[ii].q112 *= .5*k;
#endif
#ifdef FIFTH
		(*nodelist)[ii].q500 *= zt*k;
		(*nodelist)[ii].q050 *= zt*k;
		(*nodelist)[ii].q005 *= zt*k;
		(*nodelist)[ii].q410 *= yt*k;
		(*nodelist)[ii].q140 *= yt*k;
		(*nodelist)[ii].q401 *= yt*k;
		(*nodelist)[ii].q104 *= yt*k;
		(*nodelist)[ii].q041 *= yt*k;
		(*nodelist)[ii].q014 *= yt*k;
		(*nodelist)[ii].q320 *= .5*xt*k;
		(*nodelist)[ii].q230 *= .5*xt*k;
		(*nodelist)[ii].q302 *= .5*xt*k;
		(*nodelist)[ii].q203 *= .5*xt*k;
		(*nodelist)[ii].q032 *= .5*xt*k;
		(*nodelist)[ii].q023 *= .5*xt*k;
		(*nodelist)[ii].q221 *= .25*k;
		(*nodelist)[ii].q212 *= .25*k;
		(*nodelist)[ii].q122 *= .25*k;
		(*nodelist)[ii].q311 *= xt*k;
		(*nodelist)[ii].q131 *= xt*k;
		(*nodelist)[ii].q113 *= xt*k;
#endif
	/*debug
	printf("%d %f %f\n",ii,(*nodelist)[ii].sqp,(*nodelist)[ii].q100);
	*/
		if( (*nodelist)[ii].sa != 0.)
		{
			(*nodelist)[ii].xa = (*nodelist)[ii].xa/(*nodelist)[ii].sa;
			(*nodelist)[ii].ya = (*nodelist)[ii].ya/(*nodelist)[ii].sa;
			(*nodelist)[ii].za = (*nodelist)[ii].za/(*nodelist)[ii].sa;
		}
		(*nodelist)[ii].xa += (*nodelist)[ii].xc;
		(*nodelist)[ii].ya += (*nodelist)[ii].yc;
		(*nodelist)[ii].za += (*nodelist)[ii].zc;
/* correct for double counting */
		(*nodelist)[ii].sa  *= .5;
	}

}
void loop_8(){
	for( ii=0; ii< jj; ii++)
	{
		a1 = (*atomall)[ii];
		a1-> px = a1->x + loop_lambda*a1->dx;
		a1-> py = a1->y + loop_lambda*a1->dy;
		a1-> pz = a1->z + loop_lambda*a1->dz;
		a1 -> VP = 0.;
		a1 -> dpx = 0.;
		a1 -> dpy = 0.;
		a1 -> dpz = 0.;
		a1 -> qxx = 0.;
		a1 -> qxy = 0.;
		a1 -> qxz = 0.;
		a1 -> qyy = 0.;
		a1 -> qyz = 0.;
		a1 -> qzz = 0.;
#ifdef CUBIC
		a1 -> qxxx = 0.;
		a1 -> qxxy = 0.;
		a1 -> qxxz = 0.;
		a1 -> qxyy = 0.;
		a1 -> qxyz = 0.;
		a1 -> qxzz = 0.;
		a1 -> qyyy = 0.;
		a1 -> qyyz = 0.;
		a1 -> qyzz = 0.;
		a1 -> qzzz = 0.;
#endif
		for( j=0; j< NCLOSE; j++) 
			a1->close[j] = NULL;

	}/* end of initializations */

}
void* loop_9(void* arg){

/*	

	
*/
	float (*vector_loop2)[];
	vector_loop2 = (float (*)[])  malloc( 4*vloop_size*sizeof(float) );
	float r,r0,xt,yt,zt;
	float xt2,xt3,xt4,yt2,yt3,yt4,zt2,zt3,zt4;
	float k,k1,k2,k3,k4,k5;
	float ka2,ka3;
	float kb2,kb3;
	float c1,c2,c3,c4,c5; 
	int ix,iy,iz, ii, inclose, j, inode, i_loop;
	int naybor[27];
	ATOM *a1, *a2;
	int top;
	int base= (int) arg;
	int imax=jj;
	if (base==0){
		top=floor(imax/4);
	}
	else if (base==1){
		base=floor(imax/4);
		top=2*base;
	}
	else if (base==2){
		base=2*floor(imax/4);
		top=base+floor(imax/4);
	}
	else {
		base= 3*floor(imax/4);
		top=imax;
	}
	for( ii=base; ii<  top; ii++)
	{ /* if this is met we update the expansion for this atom */
/*	a1 = (*atomall)[ii];
	atomall will be reused in this loop so we refer to atomlist
	*/
		a1 = (*atomlist)[ii].who;
		inclose = 0;
/* loop over the nodes 
   if the node is mine or a neighbor then use an
   explicit summation
   otherwise use the MM node */
		ix = (a1->px  - xmin )/mmbox ;
		iy = (a1->py  - ymin )/mmbox ;
		iz = (a1->pz  - zmin )/mmbox ;
		naybor[0] = ((iz*ny)+iy)*nx + ix;
		naybor[1] = ((iz*ny)+iy)*nx + ix+1;
		naybor[2] = ((iz*ny)+iy)*nx + ix-1;
		naybor[3] = ((iz*ny)+iy)*nx+nx + ix;
		naybor[4] = ((iz*ny)+iy)*nx-nx + ix;
		naybor[5] = ((iz*ny)+iy)*nx+nx + ix+1;
		naybor[6] = ((iz*ny)+iy)*nx+nx + ix-1;
		naybor[7] = ((iz*ny)+iy)*nx-nx + ix+1;
		naybor[8] = ((iz*ny)+iy)*nx-nx + ix-1;
		naybor[9] = ((iz*ny)+ny+iy)*nx + ix;
		naybor[10] = ((iz*ny)+ny+iy)*nx + ix+1;
		naybor[11] = ((iz*ny)+ny+iy)*nx + ix-1;
		naybor[12] = ((iz*ny)+ny+iy)*nx+nx + ix;
		naybor[13] = ((iz*ny)+ny+iy)*nx-nx + ix;
		naybor[14] = ((iz*ny)+ny+iy)*nx+nx + ix+1;
		naybor[15] = ((iz*ny)+ny+iy)*nx+nx + ix-1;
		naybor[16] = ((iz*ny)+ny+iy)*nx-nx + ix+1;
		naybor[17] = ((iz*ny)+ny+iy)*nx-nx + ix-1;
		naybor[18] = ((iz*ny)-ny+iy)*nx + ix;
		naybor[19] = ((iz*ny)-ny+iy)*nx + ix+1;
		naybor[20] = ((iz*ny)-ny+iy)*nx + ix-1;
		naybor[21] = ((iz*ny)-ny+iy)*nx+nx + ix;
		naybor[22] = ((iz*ny)-ny+iy)*nx-nx + ix;
		naybor[23] = ((iz*ny)-ny+iy)*nx+nx + ix+1;
		naybor[24] = ((iz*ny)-ny+iy)*nx+nx + ix-1;
		naybor[25] = ((iz*ny)-ny+iy)*nx-nx + ix+1;
		naybor[26] = ((iz*ny)-ny+iy)*nx-nx + ix-1;
		
	/*INNER INODE FOR*/	
		for( inode = 0; inode < nx*ny*nz; inode ++) 
		{/* loop over all mm nodes */
	/* check the origin */
			for(j=0; j< 27; j++)
			{  
				if( inode == naybor[j]) break; 
			}
			if( j == 27  )
			{ /* then use mm */
				if( (*nodelist)[inode].innode > 0 )
				{
/* now do the multipole expansion for the electrostatic terms */
/* note that dielectric is included in the multipole expansion */
					xt = (*nodelist)[inode].xc - a1->px;
					yt = (*nodelist)[inode].yc - a1->py;
					zt = (*nodelist)[inode].zc - a1->pz;
					r = one/(xt*xt + yt*yt + zt*zt);
					r0 = sqrt(r);
					c1 =  -r*r0;
					c2 = -three*c1*r;
					c3 = -five*c2*r;
					c4 = -seven*c3*r;
					c5 = -nine*c4*r;
					xt2 = xt*xt;
					xt3 = xt2*xt;
					xt4 = xt3*xt;
					yt2 = yt*yt;
					yt3 = yt2*yt;
					yt4 = yt3*yt;
					zt2 = zt*zt;
					zt3 = zt2*zt;
					zt4 = zt3*zt;
//					pthread_mutex_lock(&mutexes[(*atomlist)[ii].which]);

					a1->VP += (*nodelist)[inode].sqp*a1->q*r0;
					k = c1*a1->q*xt;
					a1->VP += k*(*nodelist)[inode].q100;
					a1->dpx += k*(*nodelist)[inode].sqp;
					k = c1*a1->q*yt;
					a1->VP += k*(*nodelist)[inode].q010;
					a1->dpy += k*(*nodelist)[inode].sqp;
					k = c1*a1->q*zt;
					a1->VP += k*(*nodelist)[inode].q001;
					a1->dpz += k*(*nodelist)[inode].sqp;
/* n=2 */	
					k = (c2*xt2 +c1)*a1->q;
					a1->VP += k*(*nodelist)[inode].q200;
					a1->dpx += k*(*nodelist)[inode].q100;
					a1->qxx += k*(*nodelist)[inode].sqp;
					k = (c2*yt2 +c1)*a1->q;
					a1->VP += k*(*nodelist)[inode].q020;
					a1->dpy += k*(*nodelist)[inode].q010;
					a1->qyy += k*(*nodelist)[inode].sqp;
					k = (c2*zt2 +c1)*a1->q;
					a1->VP += k*(*nodelist)[inode].q002;
					a1->dpz += k*(*nodelist)[inode].q001;
					a1->qzz += k*(*nodelist)[inode].sqp;
					k = c2*xt*yt*a1->q;
					a1->VP += k*(*nodelist)[inode].q110;
					a1->dpx += k*(*nodelist)[inode].q010;
					a1->dpy += k*(*nodelist)[inode].q100;
					a1->qxy += k*(*nodelist)[inode].sqp;
					k = c2*xt*zt*a1->q;
					a1->VP += k*(*nodelist)[inode].q101;
					a1->dpx += k*(*nodelist)[inode].q001;
					a1->dpz += k*(*nodelist)[inode].q100;
					a1->qxz += k*(*nodelist)[inode].sqp;
					k = c2*yt*zt*a1->q;
					a1->VP += k*(*nodelist)[inode].q011;
					a1->dpy += k*(*nodelist)[inode].q001;
					a1->dpz += k*(*nodelist)[inode].q010;
					a1->qyz += k*(*nodelist)[inode].sqp;
/* n=3 */	
					k = (c3*xt3 +3*c2*xt)*a1->q;
					a1->VP += k*(*nodelist)[inode].q300;
					a1->dpx += k*(*nodelist)[inode].q200;
					a1->qxx += k*(*nodelist)[inode].q100;
					k = (c3*yt3 +3*c2*yt)*a1->q;
					a1->VP += k*(*nodelist)[inode].q030;
					a1->dpy += k*(*nodelist)[inode].q020;
					a1->qyy += k*(*nodelist)[inode].q010;
					k = (c3*zt3 +3*c2*zt)*a1->q;
					a1->VP += k*(*nodelist)[inode].q003;
					a1->dpz += k*(*nodelist)[inode].q002;
					a1->qzz += k*(*nodelist)[inode].q001;
					k = (c3*xt2*yt+c2*yt)*a1->q;
					a1->VP += k*(*nodelist)[inode].q210;
					a1->dpx += k*(*nodelist)[inode].q110;
					a1->dpy += k*(*nodelist)[inode].q200;
					a1->qxx += k*(*nodelist)[inode].q010;
					a1->qxy += k*(*nodelist)[inode].q100;
					k = (c3*yt2*xt+c2*xt)*a1->q;
					a1->VP += k*(*nodelist)[inode].q120;
					a1->dpx += k*(*nodelist)[inode].q020;
					a1->dpy += k*(*nodelist)[inode].q110;
					a1->qyy += k*(*nodelist)[inode].q100;
					a1->qxy += k*(*nodelist)[inode].q010;
					k = (c3*xt2*zt+c2*zt)*a1->q;
					a1->VP += k*(*nodelist)[inode].q201;
					a1->dpx += k*(*nodelist)[inode].q101;
					a1->dpz += k*(*nodelist)[inode].q200;
					a1->qxx += k*(*nodelist)[inode].q001;
					a1->qxz += k*(*nodelist)[inode].q100;
					k = (c3*zt2*xt+c2*xt)*a1->q;
					a1->VP += k*(*nodelist)[inode].q102;
					a1->dpx += k*(*nodelist)[inode].q002;
					a1->dpz += k*(*nodelist)[inode].q101;
					a1->qzz += k*(*nodelist)[inode].q100;
					a1->qxz += k*(*nodelist)[inode].q001;
					k = (c3*yt2*zt+c2*zt)*a1->q;
					a1->VP += k*(*nodelist)[inode].q021;
					a1->dpy += k*(*nodelist)[inode].q011;
					a1->dpz += k*(*nodelist)[inode].q020;
					a1->qyy += k*(*nodelist)[inode].q001;
					a1->qyz += k*(*nodelist)[inode].q010;
					k = (c3*zt2*yt+c2*yt)*a1->q;
					a1->VP += k*(*nodelist)[inode].q012;
					a1->dpy += k*(*nodelist)[inode].q002;
					a1->dpz += k*(*nodelist)[inode].q011;
					a1->qzz += k*(*nodelist)[inode].q010;
					a1->qyz += k*(*nodelist)[inode].q001;
					k = (c3*zt*yt*xt)*a1->q;
					a1->VP += k*(*nodelist)[inode].q111;
					a1->dpx += k*(*nodelist)[inode].q011;
					a1->dpy += k*(*nodelist)[inode].q101;
					a1->dpz += k*(*nodelist)[inode].q110;
/* n=4 */	
#ifdef FOURTH
					k = (c4*xt4 +six*c3*(xt2) +three*c2)*a1->q;
					a1->VP += k*(*nodelist)[inode].q400;
					a1->dpx += k*(*nodelist)[inode].q300;
					a1->qxx += k*(*nodelist)[inode].q200;
					k = (c4*yt4 +six*c3*(yt2) +three*c2)*a1->q;
					a1->VP += k*(*nodelist)[inode].q040;
					a1->dpy += k*(*nodelist)[inode].q030;
					a1->qyy += k*(*nodelist)[inode].q020;
					k = (c4*zt4 +six*c3*(zt2) +three*c2)*a1->q;
					a1->VP += k*(*nodelist)[inode].q004;
					a1->dpz += k*(*nodelist)[inode].q003;
					a1->qzz += k*(*nodelist)[inode].q002;
					k = (c4*xt3*yt + three*c3*xt*yt)*a1->q;
					a1->VP += k*(*nodelist)[inode].q310;
					a1->dpx += k*(*nodelist)[inode].q210;
					a1->dpy += k*(*nodelist)[inode].q300;
					a1->qxx += k*(*nodelist)[inode].q110;
					a1->qxy += k*(*nodelist)[inode].q200;
					k = (c4*yt3*xt + three*c3*xt*yt)*a1->q;
					a1->VP += k*(*nodelist)[inode].q130;
					a1->dpx += k*(*nodelist)[inode].q030;
					a1->dpy += k*(*nodelist)[inode].q120;
					a1->qyy += k*(*nodelist)[inode].q110;
					a1->qxy += k*(*nodelist)[inode].q020;
					k = (c4*xt3*zt + three*c3*xt*zt)*a1->q;
					a1->VP += k*(*nodelist)[inode].q301;
					a1->dpx += k*(*nodelist)[inode].q201;
					a1->dpz += k*(*nodelist)[inode].q300;
					a1->qxx += k*(*nodelist)[inode].q101;
					a1->qxz += k*(*nodelist)[inode].q200;
					k = (c4*zt3*yt + three*c3*xt*yt)*a1->q;
					a1->VP += k*(*nodelist)[inode].q103;
					a1->dpx += k*(*nodelist)[inode].q003;
					a1->dpz += k*(*nodelist)[inode].q102;
					a1->qzz += k*(*nodelist)[inode].q101;
					a1->qxz += k*(*nodelist)[inode].q002;
					k = (c4*yt3*zt + three*c3*zt*yt)*a1->q;
					a1->VP += k*(*nodelist)[inode].q031;
					a1->dpz += k*(*nodelist)[inode].q030;
					a1->dpy += k*(*nodelist)[inode].q021;
					a1->qyy += k*(*nodelist)[inode].q011;
					a1->qyz += k*(*nodelist)[inode].q020;
					k = (c4*zt3*yt + three*c3*zt*yt)*a1->q;
					a1->VP += k*(*nodelist)[inode].q013;
					a1->dpz += k*(*nodelist)[inode].q012;
					a1->dpy += k*(*nodelist)[inode].q003;
					a1->qzz += k*(*nodelist)[inode].q011;
					a1->qyz += k*(*nodelist)[inode].q002;
					k = (c4*xt2*yt2 + c3*(xt2+yt2) +c2)*a1->q;
					a1->VP += k*(*nodelist)[inode].q220;
					a1->dpx += k*(*nodelist)[inode].q120;
					a1->dpy += k*(*nodelist)[inode].q210;
					a1->qxx += k*(*nodelist)[inode].q020;
					a1->qyy += k*(*nodelist)[inode].q200;
					a1->qxy += k*(*nodelist)[inode].q110;
					k = (c4*xt2*zt2 + c3*(xt2+zt2) +c2)*a1->q;
					a1->VP += k*(*nodelist)[inode].q202;
					a1->dpx += k*(*nodelist)[inode].q102;
					a1->dpz += k*(*nodelist)[inode].q201;
					a1->qxx += k*(*nodelist)[inode].q002;
					a1->qzz += k*(*nodelist)[inode].q200;
					a1->qxz += k*(*nodelist)[inode].q101;
					k = (c4*zt2*yt2 + c3*(zt2+yt2) +c2)*a1->q;
					a1->VP += k*(*nodelist)[inode].q022;
					a1->dpz += k*(*nodelist)[inode].q021;
					a1->dpy += k*(*nodelist)[inode].q012;
					a1->qzz += k*(*nodelist)[inode].q020;
					a1->qyy += k*(*nodelist)[inode].q002;
					a1->qyz += k*(*nodelist)[inode].q011;
					k = (c4*xt2*yt*zt +c3*yt*zt)*a1->q;
					a1->VP += k*(*nodelist)[inode].q211;
					a1->dpz += k*(*nodelist)[inode].q210;
					a1->dpy += k*(*nodelist)[inode].q201;
					a1->dpx += k*(*nodelist)[inode].q111;
					a1->qxx += k*(*nodelist)[inode].q011;
					a1->qxy += k*(*nodelist)[inode].q101;
					a1->qyz += k*(*nodelist)[inode].q200;
					a1->qxz += k*(*nodelist)[inode].q110;
					k = (c4*xt*yt2*zt +c3*xt*zt)*a1->q;
					a1->VP += k*(*nodelist)[inode].q121;
					a1->dpz += k*(*nodelist)[inode].q120;
					a1->dpy += k*(*nodelist)[inode].q111;
					a1->dpx += k*(*nodelist)[inode].q021;
					a1->qyy += k*(*nodelist)[inode].q101;
					a1->qxy += k*(*nodelist)[inode].q011;
					a1->qyz += k*(*nodelist)[inode].q110;
					a1->qxz += k*(*nodelist)[inode].q020;
					k = (c4*xt*yt*zt2 +c3*yt*xt)*a1->q;
					a1->VP += k*(*nodelist)[inode].q112;
					a1->dpz += k*(*nodelist)[inode].q111;
					a1->dpy += k*(*nodelist)[inode].q102;
					a1->dpx += k*(*nodelist)[inode].q012;
					a1->qzz += k*(*nodelist)[inode].q110;
					a1->qxy += k*(*nodelist)[inode].q002;
					a1->qxz += k*(*nodelist)[inode].q011;
					a1->qyz += k*(*nodelist)[inode].q101;
#endif	
/* n=5 */	
#ifdef FIFTH	
					k = ((c5*xt+9*c4)*xt4  +15*c3*xt)*a1->q;
					a1->VP += k*(*nodelist)[inode].q500;
					a1->dpx += k*(*nodelist)[inode].q400;
					a1->qxx += k*(*nodelist)[inode].q300;
					k = ((c5*yt+9*c4)*yt4  +15*c3*yt)*a1->q;
					a1->VP += k*(*nodelist)[inode].q050;
					a1->dpy += k*(*nodelist)[inode].q040;
					a1->qyy += k*(*nodelist)[inode].q030;
					k = ((c5*zt+9*c4)*zt4  +15*c3*zt)*a1->q;
					a1->VP += k*(*nodelist)[inode].q005;
					a1->dpz += k*(*nodelist)[inode].q004;
					a1->qzz += k*(*nodelist)[inode].q003;
					k = (c5*xt4+six*c4*xt2 +three*c3)*yt*a1->q;
					a1->VP += k*(*nodelist)[inode].q410;
					a1->dpx += k*(*nodelist)[inode].q310;
					a1->dpy += k*(*nodelist)[inode].q400;
					a1->qxx += k*(*nodelist)[inode].q210;
					a1->qxy += k*(*nodelist)[inode].q300;
					k = (c5*yt4+six*c4*yt2 +three*c3)*xt*a1->q;
					a1->VP += k*(*nodelist)[inode].q140;
					a1->dpx += k*(*nodelist)[inode].q040;
					a1->dpy += k*(*nodelist)[inode].q130;
					a1->qyy += k*(*nodelist)[inode].q120;
					a1->qxy += k*(*nodelist)[inode].q030;
					k = (c5*xt4+six*c4*xt2 +three*c3)*zt*a1->q;
					a1->VP += k*(*nodelist)[inode].q401;
					a1->dpx += k*(*nodelist)[inode].q301;
					a1->dpz += k*(*nodelist)[inode].q400;
					a1->qxx += k*(*nodelist)[inode].q201;
					a1->qxz += k*(*nodelist)[inode].q300;
					k = (c5*zt4+six*c4*zt2 +three*c3)*xt*a1->q;
					a1->VP += k*(*nodelist)[inode].q104;
					a1->dpx += k*(*nodelist)[inode].q004;
					a1->dpz += k*(*nodelist)[inode].q103;
					a1->qzz += k*(*nodelist)[inode].q102;
					a1->qxz += k*(*nodelist)[inode].q003;
					k = (c5*yt4+six*c4*yt2 +three*c3)*zt*a1->q;
					a1->VP += k*(*nodelist)[inode].q041;
					a1->dpy += k*(*nodelist)[inode].q031;
					a1->dpz += k*(*nodelist)[inode].q040;
					a1->qyy += k*(*nodelist)[inode].q021;
					a1->qyz += k*(*nodelist)[inode].q030;
					k = (c5*zt4+six*c4*zt2 +three*c3)*yt*a1->q;
					a1->VP += k*(*nodelist)[inode].q014;
					a1->dpy += k*(*nodelist)[inode].q004;
					a1->dpz += k*(*nodelist)[inode].q013;
					a1->qzz += k*(*nodelist)[inode].q012;
					a1->qyz += k*(*nodelist)[inode].q003;
					k = (c5*xt3*yt2 +c4*(three*xt*yt2-xt3) +three*c3*xt)*a1->q;
					a1->VP += k*(*nodelist)[inode].q320;
					a1->dpx += k*(*nodelist)[inode].q220;
					a1->dpy += k*(*nodelist)[inode].q310;
					a1->qxx += k*(*nodelist)[inode].q120;
					a1->qxy += k*(*nodelist)[inode].q210;
					a1->qyy += k*(*nodelist)[inode].q300;
					k = (c5*yt3*xt2 +c4*(three*yt*xt2-yt3) +three*c3*yt)*a1->q;
					a1->VP += k*(*nodelist)[inode].q230;
					a1->dpx += k*(*nodelist)[inode].q130;
					a1->dpy += k*(*nodelist)[inode].q220;
					a1->qxx += k*(*nodelist)[inode].q030;
					a1->qxy += k*(*nodelist)[inode].q120;
					a1->qyy += k*(*nodelist)[inode].q210;
					k = (c5*xt3*zt2 +c4*(three*xt*zt2-xt3) +three*c3*xt)*a1->q;
					a1->VP += k*(*nodelist)[inode].q302;
					a1->dpx += k*(*nodelist)[inode].q202;
					a1->dpz += k*(*nodelist)[inode].q301;
					a1->qxx += k*(*nodelist)[inode].q102;
					a1->qxz += k*(*nodelist)[inode].q201;
					a1->qzz += k*(*nodelist)[inode].q300;
					k = (c5*zt3*xt2 +c4*(three*zt*xt2-zt3) +three*c3*zt)*a1->q;
					a1->VP += k*(*nodelist)[inode].q203;
					a1->dpx += k*(*nodelist)[inode].q103;
					a1->dpz += k*(*nodelist)[inode].q202;
					a1->qxx += k*(*nodelist)[inode].q003;
					a1->qxz += k*(*nodelist)[inode].q102;
					a1->qzz += k*(*nodelist)[inode].q201;
					k = (c5*yt3*zt2 +c4*(three*yt*zt2-yt3) +three*c3*yt)*a1->q;
					a1->VP += k*(*nodelist)[inode].q032;
					a1->dpy += k*(*nodelist)[inode].q022;
					a1->dpz += k*(*nodelist)[inode].q031;
					a1->qyy += k*(*nodelist)[inode].q012;
					a1->qyz += k*(*nodelist)[inode].q021;
					a1->qzz += k*(*nodelist)[inode].q030;
					k = (c5*zt3*yt2 +c4*(three*zt*yt2-zt3) +three*c3*zt)*a1->q;
					a1->VP += k*(*nodelist)[inode].q023;
					a1->dpy += k*(*nodelist)[inode].q013;
					a1->dpz += k*(*nodelist)[inode].q022;
					a1->qyy += k*(*nodelist)[inode].q003;
					a1->qyz += k*(*nodelist)[inode].q012;
					a1->qzz += k*(*nodelist)[inode].q021;
					k = (c5*xt2*yt2 +c4*(xt2+yt2) +c3)*zt*a1->q;
					a1->VP += k*(*nodelist)[inode].q221;
					a1->dpx += k*(*nodelist)[inode].q121;
					a1->dpy += k*(*nodelist)[inode].q211;
					a1->dpz += k*(*nodelist)[inode].q220;
					a1->qxx += k*(*nodelist)[inode].q021;
					a1->qyy += k*(*nodelist)[inode].q201;
					a1->qxz += k*(*nodelist)[inode].q120;
					a1->qyz += k*(*nodelist)[inode].q210;
					k = (c5*xt2*zt2 +c4*(xt2+zt2) +c3)*yt*a1->q;
					a1->VP += k*(*nodelist)[inode].q212;
					a1->dpx += k*(*nodelist)[inode].q112;
					a1->dpy += k*(*nodelist)[inode].q202;
					a1->dpz += k*(*nodelist)[inode].q211;
					a1->qxx += k*(*nodelist)[inode].q012;
					a1->qzz += k*(*nodelist)[inode].q210;
					a1->qxz += k*(*nodelist)[inode].q111;
					a1->qyz += k*(*nodelist)[inode].q201;
					k = (c5*zt2*yt2 +c4*(zt2+yt2) +c3)*xt*a1->q;
					a1->VP += k*(*nodelist)[inode].q122;
					a1->dpx += k*(*nodelist)[inode].q022;
					a1->dpy += k*(*nodelist)[inode].q112;
					a1->dpz += k*(*nodelist)[inode].q121;
					a1->qzz += k*(*nodelist)[inode].q120;
					a1->qyy += k*(*nodelist)[inode].q102;
					a1->qxy += k*(*nodelist)[inode].q022;
					a1->qxz += k*(*nodelist)[inode].q022;
					k = (c5*xt3+three*c4*xt)*yt*zt*a1->q;
					a1->VP += k*(*nodelist)[inode].q311;						
					a1->dpx += k*(*nodelist)[inode].q211;
					a1->dpy += k*(*nodelist)[inode].q301;
					a1->dpz += k*(*nodelist)[inode].q310;
					a1->qxx += k*(*nodelist)[inode].q211;
					a1->qxy += k*(*nodelist)[inode].q201;
					a1->qxz += k*(*nodelist)[inode].q210;
					k = (c5*yt3+three*c4*yt)*xt*zt*a1->q;
					a1->VP += k*(*nodelist)[inode].q131;
					a1->dpx += k*(*nodelist)[inode].q031;
					a1->dpy += k*(*nodelist)[inode].q121;
					a1->dpz += k*(*nodelist)[inode].q130;
					a1->qyy += k*(*nodelist)[inode].q111;
					a1->qxy += k*(*nodelist)[inode].q021;
					a1->qyz += k*(*nodelist)[inode].q120;
					k = (c5*zt3+three*c4*zt)*yt*xt*a1->q;
					a1->VP += k*(*nodelist)[inode].q113;
					a1->dpx += k*(*nodelist)[inode].q013;
					a1->dpy += k*(*nodelist)[inode].q103;
					a1->dpz += k*(*nodelist)[inode].q112;
					a1->qzz += k*(*nodelist)[inode].q111;
					a1->qyz += k*(*nodelist)[inode].q102;
					a1->qxz += k*(*nodelist)[inode].q012;
#endif
					//pthread_mutex_unlock(&mutexes[(*atomlist)[ii].which]);
				} /* if innode > 0  end if */
			} else if( (*nodelist)[inode].innode > 0) 
			{ /* if not mm use explicit */
/* first load the atoms onto atomall */

				imax = 0;
				i_loop = (*nodelist)[inode].first;
				 if( (*nodelist)[inode].innode > 0  && ((*atomlist)[i_loop].who)->serial > a1->serial)
				 {  
					//pthread_mutex_lock(&mutexes[imax+1]);
					(*atomall)[imax++] = (*atomlist)[i_loop].who; //POSIBLE VIOL DEP
					//pthread_mutex_unlock(&mutexes[imax]);
				 }

				for( j=1; j< (*nodelist)[inode].innode -1 ; j++)
				{
					 i_loop = (*atomlist)[i_loop].next;
					 if( ((*atomlist)[i_loop].who)->serial > a1->serial)
					 {  
					//	pthread_mutex_lock(&mutexes[imax+1]);
						(*atomall)[imax++] = (*atomlist)[i_loop].who; //POSIBLE VIOL DEP
					//	pthread_mutex_unlock(&mutexes[imax]);
					 }
				}
	/*
	for( j=0; j< jj; j++)
	{
		if( (*atomlist)[j].which == inode && 
		    (*atomlist)[j].who->serial > a1->serial)
		    {(*atomall)[imax] = (*atomlist)[j].who;
		    imax+= 1;}
	}
	*/
				for( i_loop=0; i_loop< imax; i_loop++)
				{	

					a2 = (*atomall)[i_loop];
					j = i_loop*4;
					(*vector_loop2)[j  ] = a2->px - a1->px ; 
					(*vector_loop2)[j+1] = a2->py - a1->py ;
					(*vector_loop2)[j+2] = a2->pz - a1->pz ;
				}
						
				for( i_loop=0; i_loop< imax; i_loop++)

				{
					j = i_loop*4;
					(*vector_loop2)[j+3] = sqrt((*vector_loop2)[j]*(*vector_loop2)[j] + (*vector_loop2)[j+1]*(*vector_loop2)[j+1] +
					 (*vector_loop2)[j+2]*(*vector_loop2)[j+2]); //VIOL DEP
				}
/* add the new components */





				for( i_loop=0; i_loop< imax; i_loop++)
				{
					a2 = (*atomall)[i_loop];
					for( j=0; j< a1->dontuse; j++)
					{ if( a2 == a1->excluded[j]) goto SKIPNEW;}
					j = i_loop*4;
					if( (*vector_loop2)[j+3] > mxcut || inclose > NCLOSE ) 
					{

					/*	int node_to_lock=(*atomlist)[ii].which;
						int missing=-1;
						if (node_to_lock>i_loop){
							pthread_mutex_lock(&mutexes[node_to_lock]);
							missing=i_loop;
						}
						else{
							pthread_mutex_lock(&mutexes[i_loop]);
							missing=node_to_lock;
						}
						if (i_loop!=node_to_lock){
							pthread_mutex_lock(&mutexes[missing]);
						}*/
						
							

						r0 = one/(*vector_loop2)[j+3];  
						r = r0*r0;
						r = r*r*r; /* r0^-6 */
						xt = a1->q*a2->q*dielectric*r0;
						yt = a1->a*a2->a*r;
						zt = a1->b*a2->b*r*r;
						k = xt - yt + zt;
						xt = xt*r0; yt = yt*r0; zt = zt*r0;
						k1 = xt - yt*six + zt*twelve;
						xt = xt*r0; yt = yt*r0; zt = zt*r0;
						k2 = xt*three; ka2 = - yt*six*eight; kb2 =  zt*twelve*14;
#ifdef CUBIC
						xt = xt*r0; yt = yt*r0; zt = zt*r0;
						k3 = -xt*5*3; ka3 =   yt*6*8*10 ; kb3 =  -zt*12*14*16;
#endif
						k1 = -k1;
						xt = (*vector_loop2)[j]*r0 ;
						yt = (*vector_loop2)[j+1]*r0 ;
						zt = (*vector_loop2)[j+2] *r0;
/*
	xt = (*vector_loop)[j] ;
	yt = (*vector_loop)[j+1] ;
	zt = (*vector_loop)[j+2] ;
	*/
						a1->VP += k;
						a2->dpx -= k1*xt;
						a1->dpx += k1*xt;
						a2->dpy -= k1*yt;
						a1->dpy += k1*yt;
						a2->dpz -= k1*zt;
						a1->dpz += k1*zt;
						xt2 = xt*xt; yt2 = yt*yt; zt2 = zt*zt;
						a2->qxx -= k2*(xt2 - third) + ka2*(xt2 - eightth)+kb2*(xt2-fourteenth) ;
						a1->qxx -= k2*(xt2 - third) + ka2*(xt2 - eightth)+kb2*(xt2-fourteenth) ;
						a2->qxy -= (k2+ka2+kb2)*yt*xt;
						a1->qxy -= (k2+ka2+kb2)*yt*xt;
						a2->qxz -= (k2+ka2+kb2)*zt*xt;
						a1->qxz -= (k2+ka2+kb2)*zt*xt;
						a2->qyy -= k2*(yt2 - third) + ka2*(yt2 - eightth)+kb2*(yt2-fourteenth) ;
						a1->qyy -= k2*(yt2 - third) + ka2*(yt2 - eightth)+kb2*(yt2-fourteenth) ;
						a2->qyz -= (k2+ka2+kb2)*yt*zt;
						a1->qyz -= (k2+ka2+kb2)*yt*zt;
						a2->qzz -= k2*(zt2 - third) + ka2*(zt2 - eightth)+kb2*(zt2-fourteenth) ;
						a1->qzz -= k2*(zt2 - third) + ka2*(zt2 - eightth)+kb2*(zt2-fourteenth) ;
#ifdef CUBIC
						a2->qxxx -= k3*(xt*xt*xt - xt*( 9./15 )) ;
						a2->qxxx -= ka3*(xt*xt*xt - xt*( 24./80 )) ;
						a2->qxxx -= kb3*(xt*xt*xt - xt*( 42./(14*18))); 
						a1->qxxx += k3*(xt*xt*xt - xt*( 9./15 )) ;
						a1->qxxx += ka3*(xt*xt*xt - xt*( 24./80 )) ;
						a1->qxxx += kb3*(xt*xt*xt - xt*( 42./(14*18))); 
						a2->qxxy -= k3*(yt*xt*xt - yt*( 6./ 15));
						a2->qxxy -= ka3*(yt*xt*xt - yt*( 11./ 80));
						a2->qxxy -= kb3*(yt*xt*xt - yt*( 17./ (14*18)));
						a1->qxxy += k3*(yt*xt*xt - yt*( 6./ 15));
						a1->qxxy += ka3*(yt*xt*xt - yt*( 11./ 80));
						a1->qxxy += kb3*(yt*xt*xt - yt*( 17./ (14*18)));
						a2->qxxz -= k3*(zt*xt*xt - zt*( 6./ 15));
						a2->qxxz -= ka3*(zt*xt*xt - zt*( 11./ 80));
						a2->qxxz -= kb3*(zt*xt*xt - zt*( 17./ (14*18)));
						a1->qxxz += k3*(zt*xt*xt - zt*( 6./ 15));
						a1->qxxz += ka3*(zt*xt*xt - zt*( 11./ 80));
						a1->qxxz += kb3*(zt*xt*xt - zt*( 17./ (14*18)));
						a2->qxyy -= k3*(yt*yt*xt - xt*( 6./ 15));
						a2->qxyy -= ka3*(yt*yt*xt - xt*( 11./ 80));
						a2->qxyy -= kb3*(yt*yt*xt - xt*( 17./ (14*18)));
						a1->qxyy += k3*(yt*yt*xt - xt*( 6./ 15));
						a1->qxyy += ka3*(yt*yt*xt - xt*( 11./ 80));
						a1->qxyy += kb3*(yt*yt*xt - xt*( 17./ (14*18)));
						a2->qxyz -= (k3+ka3+kb3)*yt*zt*xt;
						a1->qxyz += (k3+ka3+kb3)*yt*zt*xt;
						a2->qxzz -= k3*(zt*zt*xt - xt*( 6./ 15));
						a2->qxzz -= ka3*(zt*zt*xt - xt*( 11./ 80));
						a2->qxzz -= kb3*(zt*zt*xt - xt*( 17./ (14*18)));
						a1->qxzz += k3*(zt*zt*xt - xt*( 6./ 15));
						a1->qxzz += ka3*(zt*zt*xt - xt*( 11./ 80));
						a1->qxzz += kb3*(zt*zt*xt - xt*( 17./ (14*18)));
						a2->qyyy -= k3*(yt*yt*yt - yt*( 9./15 )) ;
						a2->qyyy -= ka3*(yt*yt*yt - yt*( 24./80 )) ;
						a2->qyyy -= kb3*(yt*yt*yt - yt*( 42./(14*18))); 
						a1->qyyy += k3*(yt*yt*yt - yt*( 9./15 )) ;
						a1->qyyy += ka3*(yt*yt*yt - yt*( 24./80 )) ;
						a1->qyyy += kb3*(yt*yt*yt - yt*( 42./(14*18))); 
						a2->qyyz -= k3*(yt*yt*zt - zt*( 6./ 15));
						a2->qyyz -= ka3*(yt*yt*zt - zt*( 11./ 80));
						a2->qyyz -= kb3*(yt*yt*zt - zt*( 17./ (14*18)));
						a1->qyyz += k3*(yt*yt*zt - zt*( 6./ 15));
						a1->qyyz += ka3*(yt*yt*zt - zt*(11./ 80));
						a1->qyyz += kb3*(yt*yt*zt - zt*( 17./ (14*18)));
						a2->qyzz -= k3*(zt*zt*yt - yt*( 6./ 15));
						a2->qyzz -= ka3*(zt*zt*yt - yt*( 11./ 80));
						a2->qyzz -= kb3*(zt*zt*yt - yt*( 17./ (14*18)));
						a1->qyzz += k3*(zt*zt*yt - yt*( 6./ 15));
						a1->qyzz += ka3*(zt*zt*yt - yt*( 11./ 80));
						a1->qyzz += kb3*(zt*zt*yt - yt*( 17./ (14*18)));
						a2->qzzz -= k3*(zt*zt*zt - zt*( 9./15 )) ;
						a2->qzzz -= ka3*(zt*zt*zt - zt*( 24./80 )) ;
						a2->qzzz -= kb3*(zt*zt*zt - zt*( 42./(14*18))); 
						a1->qzzz += k3*(zt*zt*zt - zt*( 9./15 )) ;
						a1->qzzz += ka3*(zt*zt*zt - zt*( 24./80 )) ;
						a1->qzzz += kb3*(zt*zt*zt - zt*( 42./(14*18))); 
#endif
/*						if (node_to_lock<i_loop){
							pthread_mutex_unlock(&mutexes[node_to_lock]);
							missing=i_loop;
						}
						else{
							pthread_mutex_unlock(&mutexes[i_loop]);
							missing=node_to_lock;
						}
						if (i_loop!=node_to_lock){
							pthread_mutex_unlock(&mutexes[missing]);
						}*/
					}
					else {
/*
						int node_to_lock=(*atomlist)[ii].which;
						int missing=-1;
						if (node_to_lock>i_loop){
							pthread_mutex_lock(&mutexes[node_to_lock]);
							missing=i_loop;
						}
						else{
							pthread_mutex_lock(&mutexes[i_loop]);
							missing=node_to_lock;
						}
						if (i_loop!=node_to_lock){
							pthread_mutex_lock(&mutexes[missing]);
						}*/

						if ((inclose+1)>=NCLOSE){
							cout<<"ERROR!! NCLOSE MAYOR..."<<endl;
						}
						if (i_loop>=jj){
							cout<<"ERROR!! ILOOP MAYOR..."<<endl;
						}
//						cout<<"Inclose: "<<inclose<<" "<<" ILoop: "<<i_loop<<endl;
//						cout<<"Pasa..."<<endl;
						
						//a1->close[inclose++] = (*atomall)[i_loop]; //POSIBLE VIOL DE DEP
						inclose++;

/*						if (node_to_lock<i_loop){
							pthread_mutex_unlock(&mutexes[node_to_lock]);
							missing=i_loop;
						}
						else{
							pthread_mutex_unlock(&mutexes[i_loop]);
							missing=node_to_lock;
						}
						if (i_loop!=node_to_lock){
							pthread_mutex_unlock(&mutexes[missing]);
						}
*/
						//inclose++;						
/* debugging
	j = i_loop *4;
	fprintf(stderr," mxcut %f %f inclose %d who %d \n",mxcut,(*vector_loop)[j+3],inclose,(*atomall)[i_loop]->serial);
	fprintf(stderr," vector_loop %f %f %f \n", (*vector_loop)[j],(*vector_loop)[j+1],(*vector_loop)[j+2]);
*/
						if( inclose == NCLOSE)
						{
							aaerror(
							" fv_update_nonbon> too many atoms increase NCLOSE or decrease mxcut");
/*		exit(0); 
*/
						}
					}
					SKIPNEW:  j =  j;
				}/* end of loop i_loop */
			}/* end of if in close MM node */
		}/* end of loop inode */

		/*END OF INNER INODE FOR*/
/* merge the non-bond mxcut lists */
        /* SPEC remove following line which causes compiler warnings.
         * The line was a no-op anyway, since double equals is a 
         * compare.  The author of AMMP says that we should
         * NOT change it to a single equals, as that would "cause a
         * failure under certain odd conditions.  The actual array is
         * preinitialized above to NULL and is filled during these
         * routines" - jh/9/24/99 */
	/* a1->close[inclose] == NULL; */
/* set the position */

		a1->px = a1->dx*loop_lambda + a1->x;
		a1->py = a1->dy*loop_lambda + a1->y;
		a1->pz = a1->dz*loop_lambda + a1->z;

	}  /* end of ii loop */

	free( vector_loop2);

	parallel_loop9.commit();
}

int parallel_mm_fv_update (float lambda)
{

	loop_lambda=lambda;
	mmbox = get_f_variable("mmbox");
	mxcut = get_f_variable("mxcut");
	if( mxcut < 0.) mxcut= 5.;

	dielectric = get_f_variable("dielec");
	if( dielectric <= 0.) dielectric = 1.;
	dielectric = 332.17752/dielectric;
	
/*  get the number of atoms and allocate the memory for the array space */
	i_loop = a_number();
	vloop_size=i_loop;
	vector_loop = (float (*)[])  malloc( 4*i_loop*sizeof(float) );
	if( vector_loop == NULL) 
	{ aaerror("cannot allocate memory in mm_fv_update\n");
	return 0;}
	atomall = (ATOM*(*)[]) malloc( i_loop*sizeof(ATOM *) );
	if( atomall == NULL)
	{aaerror("cannot allocate memory in mm_fv_update\n"); 
	return 0;}
	atomlist = (MMATOM (*)[]) malloc( i_loop * sizeof( MMATOM ));
	if( atomlist == NULL)
	{ aaerror("cannot allocate memory in mm_fv_update\n"); 
	return 0;}

	imax = a_number();
	jj = imax;

	loop_0();
/* first check if anyone's moved and update the lists */
/* note that this must be a look-ahead rather than
*  look back search because
* we cannot update ->px until we've used that atom !!! */


	/*FIRST II FOR*/

	loop_1();
/* determine the bounds of box which surrounds all of the atoms */
	xmax = -10e10;
	ymax = -10e10;
	zmax = -10e10;
	xmin =  10e10;
	ymin =  10e10;
	zmin =  10e10;

	loop_2();

	nx = (xmax - xmin)/mmbox + 1 ;
	ny = (ymax - ymin)/mmbox + 1 ;
	nz = (zmax - zmin)/mmbox + 1 ;

/* DEBUG	
	sprintf(line,"before allocation mmbox %f nx %d ny %d nz %d \n",mmbox,nx,ny,nz);
	aaerror( line);
	sprintf(line," xmin xmax %f %f ymin ymax %f %f zmin zmax %f %f\n",
	xmin,xmax,ymin,ymax,zmin,zmax);
	aaerror( line);
end of DEBUG */

/* now try to malloc the mmnodes */
	nodelist = (MMNODE (*)[]) malloc( nx*ny*nz * sizeof( MMNODE ));
	if( nodelist == NULL)
	{ aaerror("cannot allocate node memory in mm_fv_update (doubling grid )\n"); 
	sprintf(line,"mmbox %f nx %d ny %d nz %d ",mmbox,nx,ny,nz);
	aaerror( line);
	sprintf(line," xmin xmax %f %f ymin ymax %f %f zmin zmax %f %f",
	xmin,xmax,ymin,ymax,zmin,zmax);
	aaerror( line);
	mmbox = mmbox *2;
	set_f_variable( "mmbox",mmbox);
	nx = (xmax - xmin)/mmbox + 1;
	ny = (ymax - ymin)/mmbox + 1;
	nz = (zmax - zmin)/mmbox + 1;
	nodelist = (MMNODE(*)[])malloc( nx*ny*nz * sizeof( MMNODE ));
	if( nodelist == NULL)
	{ aaerror("cannot allocate node memory in mm_fv_update (cannot do it)\n");
	return 0; }
	}

	loop_3();

	loop_4();

/* now decide for each atom who he belongs to */	

	loop_5();

	/*FOURTH II FOR*/

/* and generate the links */ 
	
	/*FIRST INODE FOR*/

	loop_6();

/* and now (almost done with the MM setup)
* normalize the accumulated nodal data */
	/* multiplied by .5 to correct for double counting */
	k = dielectric *.5;
	xt = .5/3.;
	yt = xt/4.;
	zt = yt/5.;


	/*FIFTH II FOR*/
	
	loop_7();


/* initiallization of the mmnodes is done !!! */

/*  initialize the data for every atom */

	loop_8();

	/*SIXTH II FOR*/

	/*SEVENTH II FOR*/


//	loop_9((void*)0);
//	loop_9((void*)1);

	script_vector input_functions;
	vector <void*> input_vars;
  	input_vars.push_back((void*)0); //Input for first iteration
	input_functions.push_back(loop_9);

	parallel_loop9.run(input_functions, input_vars);
	parallel_loop9.append(loop_9, (void*)1);
	parallel_loop9.append(loop_9, (void*)2);
	parallel_loop9.append(loop_9, (void*)3);
	parallel_loop9.commit();
	parallel_loop9.get_results();

//	input_vars[0]=(void*)1;

//	parallel_loop9.run(input_functions, input_vars);
//	parallel_loop9.commit();
//	parallel_loop9.get_results();

	
	/*END OF SEVENTH II FOR*/
	a_inactive_f_zero();

	free( atomlist);
	free( nodelist);
	free( vector_loop);
	free (atomall);
	return 1;

}



/* vnonbon.c
*
* collection of routines to service nonbonded potentials
*
* POOP (Poor-mans Object Oriented Programming) using scope rules
*
* the routines for potential value, force and (eventually) second
* derivatives are here also
*
* force and 2nd derivative routines assume zero'd arrays for output
* this allows for parralellization if needed (on a PC?)
*
* forces are symmetric - so we don't have to mess around with
* s matrices and the like.
*
* note that the non-bonded information is in the ATOM structures 
*
*
* attempts at vectorization
*/
/*
*  copyright 1992, 1993 Robert W. Harrison
*  
*  This notice may not be removed
*  This program may be copied for scientific use
*  It may not be sold for profit without explicit
*  permission of the author(s) who retain any
*  commercial rights including the right to modify 
*  this notice
*/

/* SPEC add function proto to reduce compiler warnings jh/9/21/99 */
void a_inactive_f_zero ();

/* ATOM structure contains a serial number for indexing into
* arrays and the like (a Hessian)
* but otherwise is self-contained. Note the hooks for Non-nonboned potentials
*/

/* v_nonbon()
* this function sums up the potentials
* for the atoms defined in the nonbon data structure.
*/
/* standard returns 0 if error (any) 1 if ok
* V is the potential */

int fv_update_nonbon(float lambda)
{
	float r,r0,xt,yt,zt;
	float k,k1,k2,k3,k4,k5;
	float ka3,ka2;
	float kb3,kb2;
	float get_f_variable(char*);
	int inbond,inangle,i;
	ATOM *a1,*a2,*bonded[10],*angled[10];
	ATOM *a_next(int); /* returns first ATOM when called with -1 */
	int (*indexes)[],inindex,in;
	int a_number();
	int ii,j,jj,imax,inclose;
	float (*vector)[];
/*	float (*vecold)[];
*/
	ATOM *close[NCLOSE],*(*atomall)[];
	float mxdq,dielectric,mxcut; 
	static float dielecold = -1.;


	mxdq = get_f_variable("mxdq");
/*	if( mxdq < 0.) mxdq = 0.;
*/
	mxcut = get_f_variable("mxcut");
	if( mxcut < 0.) mxcut= 5.;
	if( mxdq > 0.) mxdq = mxdq*mxdq;

	dielectric = get_f_variable("dielec");
	if( dielectric < 1.) dielectric = 1.;
	if( dielecold != dielectric)
	{
	 dielecold = dielectric;
	mxdq = -1.;
	}
	dielectric = 332.17752/dielectric;
	
/*  get the number of atoms and allocate the memory for the array space */
	i = a_number();
	vector = (float (*)[]) malloc( 4*i*sizeof(float) );
	if( vector == NULL) 
	{ aaerror("cannot allocate memory in v_nonbon\n"); return 0;}
/*
	vecold = malloc( 4*i*sizeof(float) );
	if( vecold == NULL) 
	{ aaerror("cannot allocate memory in v_nonbon\n"); return 0;}
*/
	atomall = (ATOM* (*)[]) malloc( i*sizeof(ATOM *) );
	if( atomall == NULL)
	{aaerror("cannot allocate memory in v_nonbon\n"); return 0;}

	imax = a_number();
	for( i=0; i< imax; i++)
	{
		(*atomall)[i] = a_next(i);
	}
/* first check if anyone's moved and update the lists */
/* note that this must be a look-ahead rather than
*  look back search because
* we cannot update ->px until we've used that atom !!! */
	for( ii=0; ii< imax; ii++)
	{
        a1 = (*atomall)[ii];
        xt = a1->dx*lambda +a1->x - a1->px;
        yt = a1->dy*lambda +a1->y - a1->py;
        zt = a1->dz*lambda +a1->z - a1->pz;
        r = xt*xt + yt*yt + zt*zt;
	if( r > mxdq ) goto DOIT;
	}
	
	free( vector);
/*	free( vecold);
*/
	free (atomall);
	return 1;
DOIT:
	xt = get_f_variable("mmbox");
	if( xt > 0.)
	{ 
	free(vector); 
	free(atomall);
	//parallel_mm_fv_update.append(parallel_mm_fv_update_f, (void*)&extra_float);
	parallel_mm_fv_update (lambda);
	//mm_fv_update_nonbon(lambda); //Parte mas costosa...
                  return 1;}
	indexes = (int(*)[])malloc( imax* sizeof(int) );
	if( indexes == NULL ){
	aaerror(" cannot allocate memory in fv_update\n");
	return 0;}
	for( ii=0; ii< imax; ii++)
	{
	a1 = (*atomall)[ii];
	a1 -> VP = 0.;
	a1 -> dpx = 0.;
	a1 -> dpy = 0.;
	a1 -> dpz = 0.;
	a1 -> qxx = 0.;
	a1 -> qxy = 0.;
	a1 -> qxz = 0.;
	a1 -> qyy = 0.;
	a1 -> qyz = 0.;
	a1 -> qzz = 0.;
#ifdef CUBIC
	a1 -> qxxx = 0.;
	a1 -> qxxy = 0.;
	a1 -> qxxz = 0.;
	a1 -> qxyy = 0.;
	a1 -> qxyz = 0.;
	a1 -> qxzz = 0.;
	a1 -> qyyy = 0.;
	a1 -> qyyz = 0.;
	a1 -> qyzz = 0.;
	a1 -> qzzz = 0.;
#endif
	for( j=0; j< NCLOSE; j++) 
		a1->close[j] = NULL;

	}
	for( ii=0; ii<  imax; ii++)
	{ /* if this is met we update the expansion for this atom */
	a1 = (*atomall)[ii];
	inclose = 0;
	if( lambda != 0.)
	{
	for( i=ii+1; i< imax; i++)
	{
	a2 = (*atomall)[i];
	j = i*4;
	(*vector)[j  ] = a2->x - a1->x + lambda*(a2->dx -a1->dx);
	(*vector)[j+1] = a2->y - a1->y + lambda*(a2->dy -a1->dy);
	(*vector)[j+2] = a2->z - a1->z + lambda*(a2->dz -a1->dz);
	}
	}else {
	for( i=ii+1; i< imax; i++)
	{
	a2 = (*atomall)[i];
	j = i*4;
	(*vector)[j  ] = a2->x - a1->x ;
	(*vector)[j+1] = a2->y - a1->y ;
	(*vector)[j+2] = a2->z - a1->z ;
	}
	} /* end of difference position into vector loops */
	for( i=ii+1; i< imax; i++)
	{
		j = i*4;
		(*vector)[j+3] = sqrt((*vector)[j]*(*vector)[j] +
				 (*vector)[j+1]*(*vector)[j+1] +
				 (*vector)[j+2]*(*vector)[j+2]);
	}
/* add the new components */
/* first extract indexes */
	inindex = 0;
	for( i=ii+1; i< imax; i++)
	{
	a2 = (*atomall)[i];
	for( j=0; j< a1->dontuse; j++)
	{ if( a2 == a1->excluded[j]) goto SKIPNEW;}
	j = i*4;
	if( (*vector)[j+3] > mxcut || inclose > NCLOSE)
	{
	(*indexes)[inindex++] = i;
	}else {
	a1->close[inclose++] = (*atomall)[i];
	}
	if( inclose == NCLOSE)
	{
	aaerror(
	" fv_update_nonbon> too many atoms increase NCLOSE or decrease mxcut");
	}
SKIPNEW:   i = i;
	}
	/* and then use them */
	for( in=0; in< inindex; in++)
	{
	i = (*indexes)[in];
	a2 = (*atomall)[i];
	j = i*4;
	r0 = one/(*vector)[j+3];  
	r = r0*r0;
	r = r*r*r; /* r0^-6 */
	xt = a1->q*a2->q*dielectric*r0;
	yt = a1->a*a2->a*r;
	zt = a1->b*a2->b*r*r;
	k = xt - yt + zt;
	xt = xt*r0; yt = yt*r0; zt = zt*r0;
	k1 = xt - yt*six + zt*twelve;
	xt = xt*r0; yt = yt*r0; zt = zt*r0;
	/*
	k2 = xt*three; ka2 = - yt*6*8; kb2 =  zt*12*14;
	*/
	k2 = xt*three; ka2 = - yt*48.; kb2 =  zt*168.;
#ifdef CUBIC
	xt = xt*r0; yt = yt*r0; zt = zt*r0;
	k3 = -xt*5*3; ka3 =   yt*6*8*10 ; kb3 =  -zt*12*14*16;
#endif
	k1 = -k1;
	xt = (*vector)[j]*r0 ;
	yt = (*vector)[j+1]*r0 ;
	zt = (*vector)[j+2] *r0;
	/*
	xt = (*vector)[j] ;
	yt = (*vector)[j+1] ;
	zt = (*vector)[j+2] ;
	*/
	a1->VP += k;
	a2->dpx -= k1*xt;
	a1->dpx += k1*xt;
	a2->dpy -= k1*yt;
	a1->dpy += k1*yt;
	a2->dpz -= k1*zt;
	a1->dpz += k1*zt;
/*  note that xt has the 1/r in it so k2*xt*xt is 1/r^5 */
	a2->qxx -= k2*(xt*xt - third) + ka2*(xt*xt - eightth) + kb2*(xt*xt-fourteenth) ;
	a1->qxx -= k2*(xt*xt - third) + ka2*(xt*xt - eightth) + kb2*(xt*xt-fourteenth) ;
	a2->qxy -= (k2+ka2+kb2)*yt*xt;
	a1->qxy -= (k2+ka2+kb2)*yt*xt;
	a2->qxz -= (k2+ka2+kb2)*zt*xt;
	a1->qxz -= (k2+ka2+kb2)*zt*xt;
	a2->qyy -= k2*(yt*yt - third) + ka2*(yt*yt - eightth) + kb2*(yt*yt-fourteenth) ;
	a1->qyy -= k2*(yt*yt - third) + ka2*(yt*yt - eightth) + kb2*(yt*yt-fourteenth) ;
	a2->qyz -= (k2+ka2+kb2)*yt*zt;
	a1->qyz -= (k2+ka2+kb2)*yt*zt;
	a2->qzz -= k2*(zt*zt - third) + ka2*(zt*zt - eightth) + kb2*(zt*zt-fourteenth) ;
	a1->qzz -= k2*(zt*zt - third) + ka2*(zt*zt - eightth) + kb2*(zt*zt-fourteenth) ;
#ifdef CUBIC
	a2->qxxx -= k3*(xt*xt*xt - xt*( 9./15 )) ;
	a2->qxxx -= ka3*(xt*xt*xt - xt*( 24./80 )) ;
	a2->qxxx -= kb3*(xt*xt*xt - xt*( 42./(14*16))); 
	a1->qxxx += k3*(xt*xt*xt - xt*( 9./15 )) ;
	a1->qxxx += ka3*(xt*xt*xt - xt*( 24./80 )) ;
	a1->qxxx += kb3*(xt*xt*xt - xt*( 42./(14*16))); 
	a2->qxxy -= k3*(yt*xt*xt - yt*( 6./ 15));
	a2->qxxy -= ka3*(yt*xt*xt - yt*( 11./ 80));
	a2->qxxy -= kb3*(yt*xt*xt - yt*( 17./ (14*16)));
	a1->qxxy += k3*(yt*xt*xt - yt*( 6./ 15));
	a1->qxxy += ka3*(yt*xt*xt - yt*( 11./ 80));
	a1->qxxy += kb3*(yt*xt*xt - yt*( 17./ (14*16)));
	a2->qxxz -= k3*(zt*xt*xt - zt*( 6./ 15));
	a2->qxxz -= ka3*(zt*xt*xt - zt*( 11./ 80));
	a2->qxxz -= kb3*(zt*xt*xt - zt*( 17./ (14*16)));
	a1->qxxz += k3*(zt*xt*xt - zt*( 6./ 15));
	a1->qxxz += ka3*(zt*xt*xt - zt*( 11./ 80));
	a1->qxxz += kb3*(zt*xt*xt - zt*( 17./ (14*16)));
	a2->qxyy -= k3*(yt*yt*xt - xt*( 6./ 15));
	a2->qxyy -= ka3*(yt*yt*xt - xt*( 11./ 80));
	a2->qxyy -= kb3*(yt*yt*xt - xt*( 17./ (14*16)));
	a1->qxyy += k3*(yt*yt*xt - xt*( 6./ 15));
	a1->qxyy += ka3*(yt*yt*xt - xt*( 11./ 80));
	a1->qxyy += kb3*(yt*yt*xt - xt*( 17./ (14*16)));
	a2->qxyz -= (k3+ka3+kb3)*yt*zt*xt;
	a1->qxyz += (k3+ka3+kb3)*yt*zt*xt;
	a2->qxzz -= k3*(zt*zt*xt - xt*( 6./ 15));
	a2->qxzz -= ka3*(zt*zt*xt - xt*( 11./ 80));
	a2->qxzz -= kb3*(zt*zt*xt - xt*( 17./ (14*16)));
	a1->qxzz += k3*(zt*zt*xt - xt*( 6./ 15));
	a1->qxzz += ka3*(zt*zt*xt - xt*( 11./ 80));
	a1->qxzz += kb3*(zt*zt*xt - xt*( 17./ (14*16)));
	a2->qyyy -= k3*(yt*yt*yt - yt*( 9./15 )) ;
	a2->qyyy -= ka3*(yt*yt*yt - yt*( 24./80 )) ;
	a2->qyyy -= kb3*(yt*yt*yt - yt*( 42./(14*16))); 
	a1->qyyy += k3*(yt*yt*yt - yt*( 9./15 )) ;
	a1->qyyy += ka3*(yt*yt*yt - yt*( 24./80 )) ;
	a1->qyyy += kb3*(yt*yt*yt - yt*( 42./(14*16))); 
	a2->qyyz -= k3*(yt*yt*zt - zt*( 6./ 15));
	a2->qyyz -= ka3*(yt*yt*zt - zt*( 11./ 80));
	a2->qyyz -= kb3*(yt*yt*zt - zt*( 17./ (14*16)));
	a1->qyyz += k3*(yt*yt*zt - zt*( 6./ 15));
	a1->qyyz += ka3*(yt*yt*zt - zt*( 11./ 80));
	a1->qyyz += kb3*(yt*yt*zt - zt*( 17./ (14*16)));
	a2->qyzz -= k3*(zt*zt*yt - yt*( 6./ 15));
	a2->qyzz -= ka3*(zt*zt*yt - yt*( 11./ 80));
	a2->qyzz -= kb3*(zt*zt*yt - yt*( 17./ (14*16)));
	a1->qyzz += k3*(zt*zt*yt - yt*( 6./ 15));
	a1->qyzz += ka3*(zt*zt*yt - yt*( 11./ 80));
	a1->qyzz += kb3*(zt*zt*yt - yt*( 17./ (14*16)));
	a2->qzzz -= k3*(zt*zt*zt - zt*( 9./15 )) ;
	a2->qzzz -= ka3*(zt*zt*zt - zt*( 24./80 )) ;
	a2->qzzz -= kb3*(zt*zt*zt - zt*( 42./(14*16))); 
	a1->qzzz += k3*(zt*zt*zt - zt*( 9./15 )) ;
	a1->qzzz += ka3*(zt*zt*zt - zt*( 24./80 )) ;
	a1->qzzz += kb3*(zt*zt*zt - zt*( 42./(14*16))); 
#endif

/* debugging
	j = i *4;
	fprintf(stderr," mxcut %f %f inclose %d who %d \n",mxcut,(*vector)[j+3],inclose,(*atomall)[i]->serial);
	fprintf(stderr," vector %f %f %f \n", (*vector)[j],(*vector)[j+1],(*vector)[j+2]);
*/
	}/* end of loop i */
/* merge the non-bond mxcut lists */
        /* SPEC remove following line which causes compiler warnings.
         * The line was a no-op anyway, since double equals is a 
         * compare.  The author of AMMP says that we should
         * NOT change it to a single equals, as that would "cause a
         * failure under certain odd conditions.  The actual array is
         * preinitialized above to NULL and is filled during these
         * routines" - jh/9/24/99 */
	/* a1->close[inclose] == NULL; */
/* set the position */
	a1->px = a1->dx*lambda + a1->x;
	a1->py = a1->dy*lambda + a1->y;
	a1->pz = a1->dz*lambda + a1->z;

	}  /* end of ii loop */
	
	a_inactive_f_zero();

	free( indexes);
	free( vector);
/*	free( vecold);
*/
	free (atomall);
	return 1;

}


/* f_nonbon()
*
* f_nonbon increments the forces in the atom structures by the force
* due to the nonbon components.  NOTE THE WORD increment.
* the forces should first be zero'd.
* if not then this code will be invalid.  THIS IS DELIBERATE.
* on bigger (and better?) machines the different potential terms
* may be updated at random or in parrellel, if we assume that this routine
* will initialize the forces then we can't do this.
*/
int f_nonbon(float lambda)
/*  returns 0 if error, 1 if OK */
{
	float ux,uy,uz;
	float k,r,r0,xt,yt,zt;
	float lcutoff,cutoff,get_f_variable(char*);
	int inbond,inangle,i,test;
	ATOM *a1,*a2,*bonded[10],*angled[10];
	ATOM *a_next(int); /* returns first ATOM when called with -1 */
	int a_number(),inbuffer,imax;
	float (*buffer)[];
	int invector,atomsused,ii,jj;
	float (*vector)[];
	ATOM *(*atms)[],*(*atomall)[];
        float dielectric;
	float fx,fy,fz;
	float xt2,xt3,xt4;
	float yt2,yt3,yt4;
	float zt2,zt3,zt4;

/* first update the lists 
*  this routine checks if any atom has
*   broken the mxdq barrier and updates the
* forces, potentials and expansions thereof */
	fv_update_nonbon( lambda); //La parte mas costosa...

        dielectric = get_f_variable("dielec");
        if( dielectric < 1.) dielectric = 1.;
        dielectric = 332.17752/dielectric;

/*  get the number of atoms and allocate the memory for the array space */
        i = a_number();
        atomall = (ATOM*(*)[])malloc( i*sizeof(ATOM *) );
        if( atomall == NULL)
        {aaerror("cannot allocate memory in f_nonbon"); return 0;}

        imax = a_number();
        for( i=0; i< imax; i++)
        {
                (*atomall)[i] = a_next(i);
        }
        for( i= 0; i< imax; i++)
        {
	fx = 0.; fy = 0.; fz = 0.;
                a1 = (*atomall)[i];
        xt = a1->dx*lambda +a1->x - a1->px;
        yt = a1->dy*lambda +a1->y - a1->py;
        zt = a1->dz*lambda +a1->z - a1->pz;


        fx = (a1->qxx*xt + a1->qxy*yt
                        + a1->qxz*zt) ;
        fy = (a1->qxy*xt + a1->qyy*yt
                        + a1->qyz*zt) ;
        fz = (a1->qxz*xt + a1->qyz*yt
                        + a1->qzz*zt) ;
#ifdef CUBIC
	xt2 = xt*xt; yt2 = yt*yt; zt2 = zt*zt;
	fx += a1->qxxx*xt2/2. + a1->qxxy*xt*yt + a1->qxxz*xt*zt
		+ a1->qxyy*yt/2. + a1->qxyz*yt*zt + a1->qxzz*zt2/2.;
	fy += a1->qxxy*xt2/2. + a1->qxyy*xt*yt + a1->qxyz*xt*zt
		+ a1->qyyy*yt/2. + a1->qyyz*yt*zt + a1->qyzz*zt2/2.;
	fz += a1->qxxz*xt2/2. + a1->qxyz*xt*yt + a1->qxzz*xt*zt
		+ a1->qyyz*yt/2. + a1->qyzz*yt*zt + a1->qzzz*zt2/2.;
#endif
#ifdef QUARTIC
	xt3 = xt*xt2; yt3 = yt*yt2; zt3 = zt*zt2;
	fx +=  a1->qxxxx*xt3/6. + a1->qxxxy*xt2*yt/2. + a1->qxxxz*xt2*zt/2.
		+ a1->qxxyy*xt*yt/2. + a1->qxxyz*xt*yt*zt + a1->qxxzz*xt*zt2/2.
		+ a1->qxyyy*yt3/6. + a1->qxyyz*yt2*zt/2. + a1->qxyzz*yt*zt2/2.
		+ a1->qxzzz*zt3/6.;
	fy +=  a1->qxxxy*xt3/6. + a1->qxxyy*xt2*yt/2. + a1->qxxyz*xt2*zt/2.
		+ a1->qxyyy*xt*yt/2. + a1->qxyyz*xt*yt*zt + a1->qxyzz*xt*zt2/2.
		+ a1->qyyyy*yt3/6. + a1->qyyyz*yt2*zt/2. + a1->qyyzz*yt*zt2/2.
		+ a1->qyzzz*zt3/6.;
	fz +=  a1->qxxxz*xt3/6. + a1->qxxyz*xt2*yt/2. + a1->qxxzz*xt2*zt/2.
		+ a1->qxyyz*xt*yt/2. + a1->qxyzz*xt*yt*zt + a1->qxzzz*xt*zt2/2.
		+ a1->qyyyz*yt3/6. + a1->qyyzz*yt2*zt/2. + a1->qyzzz*yt*zt2/2.
		+ a1->qzzzz*zt3/6.;
#endif
#ifdef QUINTIC
	xt4 = xt*xt3; yt4 = yt*yt3; zt4 = zt*zt3;
	fx += a1->qxxxxx*xt4/24. + a1->qxxxxy*xt3*yt/6. + a1->qxxxxz*xt3*zt/6.
		+ a1->qxxxyy*xt2*yt2/4. + a1->qxxxyz*xt2*yt*zt/2. + a1->qxxxzz*xt2*zt2/4.
		+ a1->qxxyyy*xt*yt3/6. + a1->qxxyyz*xt*yt2*zt/2. + a1->qxxyzz*xt*yt*zt2/2.
		+ a1->qxxzzz*xt*zt3/6. + a1->qxyyyy*yt4/24. + a1->qxyyyz*yt3*zt/6. 
		+ a1->qxyyzz*yt2*zt2/4. + a1->qxyzzz*yt*zt3/6. + a1->qxzzzz*zt4/24.;
	fy += a1->qxxxxy*xt4/24. + a1->qxxxyy*xt3*yt/6. + a1->qxxxyz*xt3*zt/6.
		+ a1->qxxyyy*xt2*yt2/4. + a1->qxxyyz*xt2*yt*zt/2. + a1->qxxyzz*xt2*zt2/4.
		+ a1->qxyyyy*xt*yt3/6. + a1->qxyyyz*xt*yt2*zt/2. + a1->qxyyzz*xt*yt*zt2/2.
		+ a1->qxyzzz*xt*zt3/6. + a1->qyyyyy*yt4/24. + a1->qyyyyz*yt3*zt/6. 
		+ a1->qyyyzz*yt2*zt2/4. + a1->qyyzzz*yt*zt3/6. + a1->qyzzzz*zt4/24.;
	fz += a1->qxxxxz*xt4/24. + a1->qxxxyz*xt3*yt/6. + a1->qxxxzz*xt3*zt/6.
		+ a1->qxxyyz*xt2*yt2/4. + a1->qxxyzz*xt2*yt*zt/2. + a1->qxxzzz*xt2*zt2/4.
		+ a1->qxyyyz*xt*yt3/6. + a1->qxyyzz*xt*yt2*zt/2. + a1->qxyzzz*xt*yt*zt2/2.
		+ a1->qxzzzz*xt*zt3/6. + a1->qyyyyz*yt4/24. + a1->qyyyzz*yt3*zt/6. 
		+ a1->qyyzzz*yt2*zt2/4. + a1->qyzzzz*yt*zt3/6. + a1->qzzzzz*zt4/24.;
#endif
	a1->fx += fx  + a1->dpx;
	a1->fy += fy  + a1->dpy;
	a1->fz += fz  + a1->dpz;
/* do the close atoms */
	for( jj=0; jj< NCLOSE; jj++)
	{ if( a1->close[jj] == NULL) break; }
        for( ii=0; ii< jj;ii++)
        {
        a2 = (ATOM*) a1->close[ii]; 
/* note ux is backwards from below */
        ux = (a2->dx -a1->dx)*lambda +(a2->x -a1->x);
        uy = (a2->dy -a1->dy)*lambda +(a2->y -a1->y);
        uz = (a2->dz -a1->dz)*lambda +(a2->z -a1->z);
        r =one/( ux*ux + uy*uy + uz*uz); r0 = sqrt(r);
        ux = ux*r0; uy = uy*r0; uz = uz*r0;
        k = -dielectric*a1->q*a2->q*r;
        r = r*r*r;
        k += a1->a*a2->a*r*r0*six;
        k -= a1->b*a2->b*r*r*r0*twelve;
        a1->fx += ux*k;
        a1->fy += uy*k;
        a1->fz += uz*k;
        a2->fx -= ux*k;
        a2->fy -= uy*k;
        a2->fz -= uz*k;
        }
        } 

	a_inactive_f_zero();
        free( atomall); return 1;

}
/* v_nonbon()
* this function sums up the potentials
* for the atoms defined in the nonbon data structure.
*/
/* standard returns 0 if error (any) 1 if ok
* V is the potential */
int v_nonbon( float *V, float lambda)
{
        float r,r0,xt,yt,zt;
        float get_f_variable(char*);
        float k;
        int inbond,inangle,i;
        ATOM *a1,*a2,*bonded[10],*angled[10];
        ATOM *a_next(int); /* returns first ATOM when called with -1 */
        int a_number(),inbuffer;
        int invector,atomsused,ii,jj,imax;
        float (*vector)[];
        float vx;
        float k2;
        ATOM *(*atomall)[];
        float dielectric;
	float xt2,xt3,xt4,xt5;
	float yt2,yt3,yt4,yt5;
	float zt2,zt3,zt4,zt5;

	fv_update_nonbon( lambda);

        dielectric = get_f_variable("dielec");
        if( dielectric < 1.) dielectric = 1.;
        dielectric = 332.17752/dielectric;

/*  get the number of atoms and allocate the memory for the array space */
        i = a_number();
        atomall = (ATOM* (*)[])malloc( i*sizeof(ATOM *) );
        if( atomall == NULL)
        {aaerror("cannot allocate memory in v_nonbon"); return 0;}

        imax = a_number();
        for( i=0; i< imax; i++)
        {
                (*atomall)[i] = a_next(i);
        }
        for( i= 0; i< imax; i++)
        {
                a1 = (*atomall)[i];
		vx = a1->VP;
                xt = a1->dx*lambda +a1->x - a1->px;
                yt = a1->dy*lambda +a1->y - a1->py;
                zt = a1->dz*lambda +a1->z - a1->pz;
                vx -= (a1->dpx*xt + a1->dpy*yt
                        + a1->dpz*zt) ;
                vx -= ( (xt*(.5*a1->qxx*xt + a1->qxy*yt + a1->qxz*zt)
                + yt*(.5*a1->qyy*yt + a1->qyz*zt) + .5*zt*a1->qzz*zt));
#ifdef CUBIC
		xt2 = xt*xt; yt2 = yt*yt;  zt2 = zt*zt;
		xt3 = xt2*xt; yt3 = yt2*yt; zt3 = zt2*zt;

	vx -= a1->qxxx*xt3/6. + a1->qxxy*xt2*yt/2 + a1->qxxz*xt2*zt/2
		+ a1->qxyy*xt*yt2/2 + a1->qxyz*xt*yt*zt + a1->qxzz*xt*zt2/2
		+ a1->qyyy*yt3/6 + a1->qyyz*yt2*zt/2 + a1->qyzz*yt*zt2/2 
		+ a1->qzzz*zt3/6.;
#endif
#ifdef QUARTIC
		xt4 = xt3*xt; yt4 = yt3*yt; zt4 = zt3*zt;
	vx -= a1->qxxxx*xt4/24. + a1->qxxxy*xt3*yt/6. + a1->qxxxz*xt3*yt/6. + a1->qxxyy*xt2*yt2/4.
		+ a1->qxxyz*xt2*yt*zt/2. + a1->qxxzz*xt2*zt2/4. + a1->qxyyy*xt*yt3/6.
		+ a1->qxyyz*xt*yt2*zt/2. + a1->qxyzz*xt*yt*zt2/2. + a1->qxzzz*xt*zt3/6.
		+ a1->qyyyy*yt4/24. + a1->qyyyz*yt3*zt/6. + a1->qyyzz*yt2*zt2/4. + a1->qyzzz*yt*zt3/6.
		+ a1->qzzzz*zt4/24.;
#endif
#ifdef QUINTIC
		xt5 = xt4*xt; yt5 = yt4*yt; zt5 = zt4*zt;
	vx -= a1->qxxxxx*xt5/120. + a1->qxxxxy*xt4*yt/24. + a1->qxxxxz*xt4*zt/24.
		+ a1->qxxxyy*xt3*yt2/12. + a1->qxxxyz*xt3*yt*zt/6. + a1->qxxxzz*xt3*zt2/12.
		+ a1->qxxyyy*xt2*yt3/12. + a1->qxxyyz*xt2*yt2*zt/4. + a1->qxxyzz*xt2*yt*zt2/4.
		+ a1->qxxzzz*xt2*zt3/12. + a1->qxyyyy*xt*yt4/24.  + a1->qxyyyz*xt*yt3*zt/6.
		+ a1->qxyyzz*xt*yt2*zt2/4. + a1->qxyzzz*xt*yt*zt3/6. + a1->qxzzzz*xt*zt4/24.
		+ a1->qyyyyy*yt5/120. + a1->qyyyyz*yt4*zt/24 + a1->qyyyzz*yt3*zt2/12.
		+ a1->qyyzzz*yt2*zt3/12. + a1->qyzzzz*yt*zt4/24. + a1->qzzzzz*zt5/120.;

#endif
/* do the close atoms */
	for( jj=0; jj< NCLOSE; jj++)
	{ if( a1->close[jj] == NULL) break; }
        for( ii=0; ii< jj;ii++)
        {
        a2 = (ATOM*)a1->close[ii]; 
        xt = (a2->dx -a1->dx)*lambda +(a2->x -a1->x);
        yt = (a2->dy -a1->dy)*lambda +(a2->y -a1->y);
        zt = (a2->dz -a1->dz)*lambda +(a2->z -a1->z);
        r = one/(xt*xt + yt*yt + zt*zt); r0 = sqrt(r);
/*      xt = xt/r0; yt = yt/r0; zt = zt/r0;
*/
        k = dielectric*a1->q*a2->q*r0;
        r = r*r*r;
        k -= a1->a*a2->a*r;
        k += a1->b*a2->b*r*r;
	vx += k;
        }
        *V += vx;
	}
	a_inactive_f_zero();
        free( atomall); return 1;
}

/*zone_nonbon()
* this function sums up the potentials
* for the atoms defined in the nonbon data structure.
*/
/* standard returns 0 if error (any) 1 if ok
* V is the potential */
int zone_nonbon(float *V,float lambda, ATOM *( *alist)[10], int inalist)
{
	float r,r0,xt,yt,zt;
	float lcutoff,cutoff,get_f_variable(char*);
	int inbond,inangle,i,ii;
	ATOM *a1,*a2;
	ATOM *a_next(int); /* returns first ATOM when called with -1 */
	float dielectric,ve,va,vh;
	ATOM *a_m_serial(int);

/* nonbonded potentials 
* do a double loop starting from the first atom to the 
* last 
* then from the second to the last 
* etc
*
* also check to avoid bonded and 1-3 bonded atoms
*/
	if( inalist <= 0 ) return 1;
	dielectric = get_f_variable("dielec");
	if( dielectric < 1.) dielectric = 1.;
	dielectric = 332.17752/dielectric;
	cutoff = get_f_variable("cutoff");
	if( cutoff < 1.) cutoff = 1.e10;
	lcutoff = -cutoff;
	for( ii=0; ii< inalist; ii++)
	{
	a1 = (*alist)[ii];
	if( a1 == NULL ) goto NOTANATOM;
	ve = 0.; va = 0.; vh = 0.;
	a2 = a_next(-1);
/*	
*	for(i = 0; i< a1->dontuse; i++)
*	printf("%d ",a1->excluded[i]->serial);
*	printf("\n");
*/
/*
	while(  (a2->next != a2) && (a2->next != NULL))
	*/
	while(  (a2 != NULL) && (a2->next != NULL) && a2->next != a2)
	{
/* goto SKIP is used because this is one case where it makes sense */
/*	if( a2 == a1) break;  */
	if( a2 == a1) goto SKIP;
	for(i = 0; i< a1->dontuse; i++)
		if( a2 == a1->excluded[i]) goto SKIP;
/* non - bonded are only used when the atoms arent bonded */

	xt = (a1->x -a2->x) + lambda*(a1->dx - a2->dx);
	if( (xt > cutoff) || (xt < lcutoff) ) goto SKIP;
	yt = (a1->y -a2->y) + lambda*(a1->dy - a2->dy);
	if( (yt > cutoff) || (yt < lcutoff) ) goto SKIP;
	zt = (a1->z -a2->z) + lambda*(a1->dz - a2->dz);
	if( (zt > cutoff) || (zt < lcutoff) ) goto SKIP;

	r = xt*xt+yt*yt+zt*zt;
	if( r < 1.) r = 1.;  

	r0 = sqrt(r); r = r*r*r ;
/* debugging
	printf(" %d %d %f %f %f \n", a1->serial,a2->serial,a1->q,a2->q,
	332.17752*a1->q*a2->q/r0);
*/
	ve += dielectric*a1->q*a2->q/r0; 
	va -= a1->a*a2->a/r;
	vh += a1->b*a2->b/r/r; 

SKIP:
/*	if( a2->next == a1) break; */
	if( (ATOM*) a2->next == a2) break;  
	a2 = (ATOM*)a2->next;
	}
	*V += ve + va + vh;
NOTANATOM:
	i = i;
	}
	return 1;

}


/* unonbon.c
*
* collection of routines to service nonbonded potentials
*
* POOP (Poor-mans Object Oriented Programming) using scope rules
*
* the routines for potential value, force and (eventually) second
* derivatives are here also
*
* force and 2nd derivative routines assume zero'd arrays for output
* this allows for parralellization if needed (on a PC?)
*
* forces are symmetric - so we don't have to mess around with
* s matrices and the like.
*
* note that the non-bonded information is in the ATOM structures 
*
* unonbon uses a 'use list' for the interactions
*
*  this is physically incorrect, but often done.
*  the use list is coded as
*  ap ... bp's for interaction ... ap
* a single array is malloc'd, it is 20 a_number() long
*  the global variable (in variable storage) nbdeep will allowriding
*  every nsteps ( 10 default ) will recalculate the list
*  again this may be over ridden with  nbstep
*  and if cutoff is not set (== 0) these routines silently call the
*  regular routines which don't care about cutoff
*  will always redo the list if a_number() changes
*
*/
/*
*  copyright 1992 Robert W. Harrison
*  
*  This notice may not be removed
*  This program may be copied for scientific use
*  It may not be sold for profit without explicit
*  permission of the author(s) who retain any
*  commercial rights including the right to modify 
*  this notice
*/

/* ATOM structure contains a serial number for indexing into
* arrays and the like (a Hessian)
* but otherwise is self-contained. Note the hooks for Non-nonboned potentials
*/

/* uselist()
*  returns a pointer to an array of ATOM structure pointers
*  these are encoded as
*  a,bcdsfg,a where a is the outer most atom in the N^2 -N tree.
*
* not over brilliant
*
*  checks for change in atom number
*  other wise redoes the list every (nbstep (default= 10)) steps
*  makes a list with total storage (nbdeep (default = 20)) time a_number
*/
int uselist(){
//Used to be: int uselist(ATOM *(*thelist)[],int *thesize, float cutoff)
/* static stuff used to keep information about status */
/*static int  oldatomnumber = 0;
static int  since = 0,lsize;
static ATOM *(*local)[];
static float oldcutoff = -1;
int a_number();
ATOM *a_next(int),*a1,*a2,*ap,*bp;
int i,j,k,max;
int get_i_variable(char*),set_i_variable(char*, int);
float lcutoff;
float x,y,z,r,rcut;

/* check on wether to redo it or not  */
/*i = a_number();
j = get_i_variable("nbstep");
if( j <= 0) j= 10;
if( (i == oldatomnumber) && (since < j) && (cutoff == oldcutoff) )
{
	*thelist = local;
	*thesize = lsize;
	since += 1;
	return 1;
}
/* a free and malloc are used because nbdeep may change */
/*RESET:
/* don't free if it hasn't been malloc'd */
/*if( oldatomnumber > 0) free(local);
oldcutoff = cutoff;
lcutoff = -cutoff;
since = 0;
oldatomnumber = i;
j = get_i_variable("nbdeep");
if( j<= 0) j = 20;
max = i*j;
local = (ATOM* (*)[])  malloc( max*sizeof(ATOM * ) );
if( local == NULL )
{ aaerror("cannot allocate uselist memory"); exit(0);}
/* now have the uselist allocated */
/**thelist = local;

*thesize = 0;
rcut = cutoff*cutoff;
a1 = a_next(-1);
a1 = (ATOM*) a1->next;
for( i=1; i< oldatomnumber; i++,a1=ap)
{
	ap = (ATOM*) a1->next;
	(*local)[*thesize] = a1;
	*thesize += 1;
	a2 = a_next(-1);
	for( j=0;j<i; j++,a2=bp)
	{
	for(k=0; k< a1->dontuse; k++)
	{
		if( a2 == a1->excluded[k]) goto SKIP;
	}	
		if( (a2->x-a1->x) > cutoff) goto SKIP;
		if( (a2->x-a1->x) < lcutoff) goto SKIP;
		if( (a2->y-a1->y) > cutoff) goto SKIP;
		if( (a2->y-a1->y) < lcutoff) goto SKIP;
		if( (a2->z-a1->z) > cutoff) goto SKIP;
		if( (a2->z-a1->z) < lcutoff) goto SKIP;
/* now calculate the radius */
/*	x = a2->x -a1->x;
	y = a2->y -a1->y;
	z = a2->z -a1->z;
	r = x*x + y*y + z*z;
	if( r > rcut) goto SKIP;
	
	(*local)[*thesize] = a2;
	*thesize += 1;
	if( *thesize >= max)
	{aaerror("please increase nbdeep (seti nbdeep (>20);)");
	 i = a_number();
	 j = get_i_variable("nbdeep");
	 if( j== 0) j = 20;
	 if( j == i+2) 
	 {aaerror("Terrible error in uselist, too many interactions");
	  exit( 0) ; }
	 j = 2*j; if( j > i+2) j = i+2;
	 set_i_variable("nbdeep",j);
	 goto RESET;
	 }
SKIP:
	bp = a_next(1);
	}
	(*local)[*thesize] = a1;
	*thesize += 1;
	lsize = *thesize;

}
/*printf(" uselist finished %d %d\n",*thesize,max); */
return 1;
}

/* v_nonbon()
* this function sums up the potentials
* for the atoms defined in the nonbon data structure.
*/
/* standard returns 0 if error (any) 1 if ok
* V is the potential */
int u_v_nonbon(float *V, float lambda)
{
	float r,r0,xt,yt,zt;
	float lcutoff,cutoff,get_f_variable(char*);
	float rdebye;
	int inbond,inangle,i;
	ATOM *a1,*a2,*bonded[10],*angled[10];
	ATOM *a_next(int); /* returns first ATOM when called with -1 */
	ATOM *(*use)[];
	int uselist();
//	int uselist (ATOM*(**)[],int*,float);
	int nuse,used;
	ATOM *cp,*bp;
	int a_number(),inbuffer;
	float (*buffer)[],xx,yy,zz;
	int invector,atomsused,ii,jj,imax;
	float (*vector)[];
	ATOM *(*atms)[];
	float dielectric;

/* nonbonded potentials 
* do a double loop starting from the first atom to the 
* last 
* then from the second to the last 
* etc
*
* also check to avoid bonded and 1-3 bonded atoms
*/
	cutoff = get_f_variable("cutoff");
	if( cutoff < 1.) 
	{
		v_nonbon( V,lambda);
		return 1;
	}
/*	rdebye = cutoff/2.;
	dielectric = get_f_variable("dielec");
	if( dielectric < 1.) dielectric = 1.;
	dielectric = 332.17752/dielectric;
	if( !uselist( &use,&nuse,cutoff) )return 0;
/*  get the number of atoms and allocate the memory for the array space */
/*	i = a_number();
	buffer = (float (*)[]) malloc( 3*i*sizeof(float) );
	if( buffer == NULL) 
	{ aaerror("cannot allocate memory in u_v_nonbon\n"); return 0;}
	vector = (float (*)[]) malloc( i*sizeof(float) );
	if( vector == NULL) 
	{ aaerror("cannot allocate memory in u_v_nonbon\n"); return 0;}
	atms = (ATOM* (*)[]) malloc( i*sizeof(ATOM *) );
	if( atms == NULL) 
	{ aaerror("cannot allocate memory in u_v_nonbon\n"); return 0;}


	a1 = a_next(-1);
	a1 = (ATOM*) a1->next;
	imax = a_number();
	used = 0;
	for( jj=1; jj<imax; jj++,a1=bp)
	{
	bp = (ATOM*) a1->next;
	inbuffer = 0;
	if( (*use)[used] == a1) 
	{ used += 1;}
	else { aaerror("error in uselist - must abort"); return 0;}
	while( (*use)[used] != a1)
	{
	  (*atms)[inbuffer++] = (*use)[used];
	  used += 1;
	}
	used += 1;
/* (*atms) now contains the list of atoms to be  done 
*  there are inbuffer of them
*  of course inbuffer can be zero so we must check for that
*/
/*	if( inbuffer > 0)
	{
	for( i=0; i< inbuffer; i++)
	{
	(*buffer)[3*i  ] = (*atms)[i]->x;
	(*buffer)[3*i+1] = (*atms)[i]->y;
	(*buffer)[3*i+2] = (*atms)[i]->z;
	}
	if( lambda != 0.)
	{
	for( i=0; i< inbuffer; i++)
	{
		(*buffer)[3*i  ] = (*atms)[i]->x +(*atms)[i]->dx*lambda;	
		(*buffer)[3*i+1] = (*atms)[i]->y +(*atms)[i]->dy*lambda;	
		(*buffer)[3*i+2] = (*atms)[i]->z +(*atms)[i]->dz*lambda;	
	}
	}
	xx = a1->x + lambda*a1->dx;
	yy = a1->y + lambda*a1->dy;
	zz = a1->z + lambda*a1->dz;
/* now for the work */
/*	for( i=0;i< inbuffer; i++)
	{
	xt = xx - (*buffer)[3*i];
	yt = yy - (*buffer)[3*i+1];
	zt = zz - (*buffer)[3*i+2];
	r = xt*xt+yt*yt+zt*zt;
	if( r < 2.) r = 2.; 
	r0 = sqrt(r); r = r*r*r ;
/* the standard which follows is recursive */
/*	 *V += 332.17752*a1->q*a2->q/r0; 
	*V -= a1->a*a2->a/r;
	*V += a1->b*a2->b/r/r; 
*/
/* use debye screen e(-r0/rdebye) */
/*	(*vector)[i] = a1->q*(*atms)[i]->q/r0*dielectric*exp(-r0/rdebye) 
		     - a1->a*(*atms)[i]->a/r
		     + a1->b*(*atms)[i]->b/r/r;
	}
	for(i=0; i< inbuffer; i++)
		*V += (*vector)[i];

	} /* end of the inbuffer if check many lines ago */
//	}
	free( atms); free( buffer); 
	free( vector);
	return 1;

}
/* u_f_nonbon()
*
* u_f_nonbon increments the forces in the atom structures by the force
* due to the nonbon components.  NOTE THE WORD increment.
* the forces should first be zero'd.
* if not then this code will be invalid.  THIS IS DELIBERATE.
* on bigger (and better?) machines the different potential terms
* may be updated at random or in parrellel, if we assume that this routine
* will initialize the forces then we can't do this.
*/
int u_f_nonbon(float lambda)
/*  returns 0 if error, 1 if OK */
{
	float r,r0,xt,yt,zt;
	float lcutoff,cutoff,get_f_variable(char*);
	float rdebye;
	int inbond,inangle,i;
	ATOM *a1,*a2,*bonded[10],*angled[10];
	ATOM *a_next(int); /* returns first ATOM when called with -1 */
	ATOM *(*use)[];
	int uselist(); //ATOM *(**)[],int *, float),nuse,used;
	ATOM *cp,*bp;
	int a_number(),inbuffer;
	float (*buffer)[],xx,yy,zz,k;
	int invector,atomsused,ii,jj,imax;
	float (*vector)[];
	ATOM *(*atms)[];
	float dielectric;

/* nonbonded potentials 
* do a double loop starting from the first atom to the 
* last 
* then from the second to the last 
* etc
*
* also check to avoid bonded and 1-3 bonded atoms
*/
	cutoff = get_f_variable("cutoff");
	if( cutoff < 1.) 
	{
		f_nonbon( lambda);
		return 1;
	}
/*	rdebye = cutoff/2.;
	dielectric = get_f_variable("dielec");
	if( dielectric < 1.) dielectric = 1.;
	dielectric = 332.17752/dielectric;
	if( !uselist( &use,&nuse,cutoff) )return 0;
/*  get the number of atoms and allocate the memory for the array space */
/*	i = a_number();
	buffer = (float (*)[]) malloc( 3*i*sizeof(float) );
	if( buffer == NULL) 
	{ aaerror("cannot allocate memory in u_v_nonbon\n"); return 0;}
	vector = (float (*)[]) malloc( 3*i*sizeof(float) );
	if( vector == NULL) 
	{ aaerror("cannot allocate memory in u_v_nonbon\n"); return 0;}
	atms = (ATOM (*)[]) malloc( i*sizeof(ATOM *) );
	if( atms == NULL) 
	{ aaerror("cannot allocate memory in u_v_nonbon\n"); return 0;}


	a1 = a_next(-1);
	a1 = (ATOM*) a1->next;
	imax = a_number();
	used = 0;
	for( jj=1; jj<imax; jj++,a1=bp)
	{
	bp = (ATOM*) a1->next;
	inbuffer = 0;
	if( (*use)[used] == a1) 
	{ used += 1;}
	else { aaerror("error in uselist - must abort"); return 0;}
	while( (*use)[used] != a1)
	{
	  (*atms)[inbuffer++] = (*use)[used];
	  used += 1;
	}
	used += 1;
/* (*atms) now contains the list of atoms to be  done 
*  there are inbuffer of them
*  of course inbuffer can be zero so we must check for that
*/
/*	if( inbuffer > 0)
	{
	for( i=0; i< inbuffer; i++)
	{
	(*buffer)[3*i  ] = (*atms)[i]->x;
	(*buffer)[3*i+1] = (*atms)[i]->y;
	(*buffer)[3*i+2] = (*atms)[i]->z;
	}
	if( lambda != 0.)
	{
	for( i=0; i< inbuffer; i++)
	{
		(*buffer)[3*i  ] = (*atms)[i]->x +(*atms)[i]->dx*lambda;	
		(*buffer)[3*i+1] = (*atms)[i]->y +(*atms)[i]->dy*lambda;	
		(*buffer)[3*i+2] = (*atms)[i]->z +(*atms)[i]->dz*lambda;	
	}
	}
	xx = a1->x + lambda*a1->dx;
	yy = a1->y + lambda*a1->dy;
	zz = a1->z + lambda*a1->dz;
/* now for the work */
/*	for( i=0;i< inbuffer; i++)
	{
	xt = xx - (*buffer)[3*i];
	yt = yy - (*buffer)[3*i+1];
	zt = zz - (*buffer)[3*i+2];
	r = xt*xt+yt*yt+zt*zt;
	 /* watch for FP errors*/
/*	 if( r <= 1.)
	 { r = 1.; }
	r0 = sqrt(r); xt = xt/r0; yt = yt/r0; zt = zt/r0;
/* use debye screen e(-r0/rdebye) */
/* d/dx(e(-r0/rdebye)/r0  = e(-r0/rdebye)*(-1/rdebye)/r0 + e(-r0/rdebye)/r) */
/*	 k = -a1->q*(*atms)[i]->q*dielectric*exp(-r0/rdebye)*
		(1./(rdebye*r0) +1./r) ; 
	r = r*r*r;
	k += a1->a*(*atms)[i]->a/r/r0*6;
	k -= a1->b*(*atms)[i]->b/r/r/r0*12;
	(*vector)[3*i  ] = xt*k;
	(*vector)[3*i+1] = yt*k;
	(*vector)[3*i+2] = zt*k;
/*
*	a1->fx += ux*k; 
*	a1->fy += uy*k; 
*	a1->fz += uz*k; 
*	a2->fx -= ux*k; 
*	a2->fy -= uy*k; 
*	a2->fz -= uz*k;
*/
/*	}
	for(i=0; i< inbuffer; i++)
	{
		a1->fx -= (*vector)[3*i  ];
		a1->fy -= (*vector)[3*i+1];
		a1->fz -= (*vector)[3*i+2];
		(*atms)[i] ->fx += (*vector)[3*i  ];
		(*atms)[i] ->fy += (*vector)[3*i+1];
		(*atms)[i] ->fz += (*vector)[3*i+2];
	}

	} /* end of the inbuffer if check many lines ago */
//	}
	free( atms); free( buffer); 
	free( vector);
	return 1;
}

