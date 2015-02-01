/* monitor.c
*
*  routine to monitor energy and force for AMMP.
*
*  monitors the potential due to each kind of potential used
*
*  reports kinetic energy and total potential and action (T-V)
*
*  reports maximum l_infinity force
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
//#include "ammp.h"
/* ATOM structure contains a serial number for indexing into
* arrays and the like (a Hessian)
* but otherwise is self-contained. Note the hooks for Non-bonded potentials
*/
void AMMPmonitor(int  (*vfs[])(float*, float),int(*ffs[])(float), int nfs, FILE *op)
{
/* block of function used in eval()
*   only the v_stuff are needed
*/
int v_bond(float*, float),f_bond(float),v_angle(float*, float),f_angle(float);
int v_mmbond(float*, float),f_mmbond(float),v_mmangle(float*, float),f_mmangle(float);
int v_ho_bond(float*, float),f_ho_bond(float),v_ho_angle(float*, float),f_ho_angle(float);
int f_c_angle(float),v_c_angle(float*, float);
int v_nonbon(float*, float),f_nonbon(float),v_torsion(float*, float),f_torsion(float);
int atom(float, float, float, int, float, float, float, float, char*),bond(int, int, float, float),angle(int, int, int, float, float),torsion(int, int,int,int,float,int,float);
int v_hybrid(float*, float),f_hybrid(float);
int restrain(int, int,float, float),v_restrain(float*, float),f_restrain(float);
int tether(int,float,float,float,float),v_tether(float*, float),f_tether(float);
int u_v_nonbon(float*, float),u_f_nonbon(float);
int v_noel(float*, float),f_noel(float);
int v_ho_noel(float*, float),f_ho_noel(float);

int a_number();
float mxdq,get_f_variable(char*);

float V,T,vt;
float a_max_f(),a_l2_f();
ATOM *ap,*a_next(int);
int ifs,a_f_zero();

if( a_number() < 1) 
{ aaerror(" no atoms defined - nothing to calculate \n"); return;}

 V = 0.; T = 0.;
a_f_zero();
for( ifs = 0; ifs < nfs; ifs++ )
{
	vt = 0.; 
	(*vfs[ifs])(&vt,0.);
	mxdq = get_f_variable("mxdq");
	set_f_variable("mxdq",100.);
	(*ffs[ifs])(0.);
	set_f_variable("mxdq",mxdq);

	V += vt;
	if( vfs[ifs] == v_bond)
	{ fprintf( op," %f bond energy\n",vt); goto DONE;}
	if( vfs[ifs] == v_ho_bond)
	{ fprintf( op," %f homotopy bond energy\n",vt);
		vt = 0; v_bond(&vt,0.);
	 fprintf( op," %f bond energy\n",vt); 
		 goto DONE;}
	if( vfs[ifs] == v_mmbond)
	{ fprintf( op," %f mm bond energy\n",vt); goto DONE;}
	if( vfs[ifs] == v_c_angle)
	{ fprintf( op," %f cangle energy\n",vt); goto DONE;}
	if( vfs[ifs] == v_mmangle)
	{ fprintf( op," %f mm angle energy\n",vt); goto DONE;}
	if( vfs[ifs] == v_angle)
	{ fprintf( op," %f angle energy\n",vt); goto DONE;}
	if( vfs[ifs] == v_ho_angle)
	{ fprintf( op," %f homotopy angle energy\n",vt); 
		vt = 0.; v_angle( &vt,0.);
	 	fprintf( op," %f angle energy\n",vt); 
		goto DONE;}
	if( vfs[ifs] == v_noel)
	{ fprintf( op," %f noel energy\n",vt); goto DONE;}
	if( vfs[ifs] == v_ho_noel)
	{ fprintf( op," %f homotopy noel energy\n",vt); 
		vt = 0.; v_noel( &vt,0.);
	 	fprintf( op," %f noel energy\n",vt); 
	goto DONE;}
	if( vfs[ifs] == u_v_nonbon)
	{ fprintf( op," %f non-bonded energy\n",vt); goto DONE;}
	if( vfs[ifs] == v_nonbon)
	{ fprintf( op," %f non-bonded energy\n",vt); goto DONE;}
	if( vfs[ifs] == v_torsion)
	{ fprintf( op," %f torsion energy\n",vt); goto DONE;}
	if( vfs[ifs] == v_hybrid)
	{ fprintf( op," %f hybrid energy\n",vt); goto DONE;}
	if( vfs[ifs] == v_tether)
	{ fprintf( op," %f tether restraint energy\n",vt); goto DONE;}
	if( vfs[ifs] == v_restrain)
	{ fprintf( op," %f restraint bond energy\n",vt); goto DONE;}
	fprintf( op," %f unknown potential type\n",vt);
DONE:  
/* next statement is needed because cannot have a label at an end loop */
	vt = 0.;
}
	fprintf( op," %f total potential energy\n",V);
/* update variables */
	set_f_variable( "l2f",a_l2_f());
	set_f_variable( "lmaxf",a_max_f());
	set_f_variable( "totalp",V);

	ifs = -1;
	while( (ap = a_next(ifs)) != NULL)
	{
		ifs = 1; 
		T += ap->vx*ap->vx*ap->mass;
		T += ap->vy*ap->vy*ap->mass;
		T += ap->vz*ap->vz*ap->mass;
	}
	T = T*.5/4.184/1000/1000;
	set_f_variable("totalk",T);
	fprintf( op," %f total kinetic energy\n",T);
	fprintf( op," %f total energy\n",T+V);
	fprintf( op," %f total action\n",T-V);
/* end of routine */
}

void AMMPmonitor_mute( int  (*vfs[])(float*, float), int (*ffs[])(float), int nfs, FILE *op)
{
/* block of function used in eval()
*   only the v_stuff are needed
*/
int v_bond(float*, float),f_bond(float),v_angle(float*, float),f_angle(float);
int v_mmbond(float*, float),f_mmbond(float),v_mmangle(float*, float),f_mmangle(float);
int v_c_angle(float*, float), f_c_angle(float);
int v_ho_bond(float*, float),f_ho_bond(float);
int v_ho_angle(float*, float),f_ho_angle(float);
int v_nonbon(float*, float),f_nonbon(float),v_torsion(float*, float),f_torsion(float);
int atom(float, float, float, int, float, float, float, float, char*),bond(int, int, float, float),angle(int, int, int, float, float),torsion(int, int,int,int,float,int,float);
int v_hybrid(float*, float),f_hybrid(float);
int restrain(int, int,float, float),v_restrain(float*, float),f_restrain(float);
int tether(int,float,float,float,float),v_tether(float*, float),f_tether(float);
int u_v_nonbon(float*, float),u_f_nonbon(float);
int v_noel(float*, float),f_noel(float);
int v_ho_noel(float*, float),f_ho_noel(float);

int a_number();
float mxdq,get_f_variable(char*);

float V,T,vt;
float a_max_f(),a_l2_f();
ATOM *ap,*a_next(int);
int ifs,a_f_zero();

if( a_number() < 1) 
{ aaerror(" no atoms defined - nothing to calculate \n"); return;}

 V = 0.; T = 0.;
a_f_zero();
for( ifs = 0; ifs < nfs; ifs++ )
{
	vt = 0.; 
	(*vfs[ifs])(&vt,0.);
	mxdq = get_f_variable("mxdq");
	set_f_variable("mxdq",100.);
	(*ffs[ifs])(0.);
	set_f_variable("mxdq",mxdq);

	V += vt;
#ifdef RWH_UNMUTED
	if( vfs[ifs] == v_bond)
	{ fprintf( op," %f bond energy\n",vt); goto DONE;}
	if( vfs[ifs] == v_ho_bond)
	{ fprintf( op," %f homotopy bond energy\n",vt);
		vt = 0; v_bond(&vt,0.);
	 fprintf( op," %f bond energy\n",vt); 
		 goto DONE;}
	if( vfs[ifs] == v_mmbond)
	{ fprintf( op," %f mm bond energy\n",vt); goto DONE;}
	if( vfs[ifs] == v_c_angle)
	{ fprintf( op," %f cangle energy\n",vt); goto DONE;}
	if( vfs[ifs] == v_mmangle)
	{ fprintf( op," %f mm angle energy\n",vt); goto DONE;}
	if( vfs[ifs] == v_angle)
	{ fprintf( op," %f angle energy\n",vt); goto DONE;}
	if( vfs[ifs] == v_ho_angle)
	{ fprintf( op," %f homotopy angle energy\n",vt); 
		vt = 0.; v_angle( &vt,0.);
	 	fprintf( op," %f angle energy\n",vt); 
		goto DONE;}
	if( vfs[ifs] == v_noel)
	{ fprintf( op," %f noel energy\n",vt); goto DONE;}
	if( vfs[ifs] == v_ho_noel)
	{ fprintf( op," %f homotopy noel energy\n",vt); 
		vt = 0.; v_noel( &vt,0.);
	 	fprintf( op," %f noel energy\n",vt); 
	goto DONE;}
	if( vfs[ifs] == u_v_nonbon)
	{ fprintf( op," %f non-bonded energy\n",vt); goto DONE;}
	if( vfs[ifs] == v_nonbon)
	{ fprintf( op," %f non-bonded energy\n",vt); goto DONE;}
	if( vfs[ifs] == v_torsion)
	{ fprintf( op," %f torsion energy\n",vt); goto DONE;}
	if( vfs[ifs] == v_hybrid)
	{ fprintf( op," %f hybrid energy\n",vt); goto DONE;}
	if( vfs[ifs] == v_tether)
	{ fprintf( op," %f tether restraint energy\n",vt); goto DONE;}
	if( vfs[ifs] == v_restrain)
	{ fprintf( op," %f restraint bond energy\n",vt); goto DONE;}
	fprintf( op," %f unknown potential type\n",vt);
DONE:  
/* next statement is needed because cannot have a label at an end loop */
	vt = 0.;
#endif
}
/* rwh
	fprintf( op," %f total potential energy\n",V);
*/
/* update variables */
	set_f_variable( "l2f",a_l2_f());
	set_f_variable( "lmaxf",a_max_f());
	set_f_variable( "totalp",V);

	ifs = -1;
	while( (ap = a_next(ifs)) != NULL)
	{
		ifs = 1; 
		T += ap->vx*ap->vx*ap->mass;
		T += ap->vy*ap->vy*ap->mass;
		T += ap->vz*ap->vz*ap->mass;
	}
	T = T*.5/4.184/1000/1000;
	set_f_variable("totalk",T);
/* rwh
	fprintf( op," %f total kinetic energy\n",T);
*/
	fprintf( op," %f total energy\n",T+V);
/* rwh
	fprintf( op," %f total action\n",T-V);
*/
/* end of routine */
}


