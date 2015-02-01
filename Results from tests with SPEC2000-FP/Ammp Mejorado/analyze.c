/* analyze.c
*
*  routine to analyze energy and force for AMMP.
*
*  analyzes the potential due to each kind of potential used
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

void analyze(int (*vfs[])(float*, float), int nfs, int ilow, int ihigh, FILE *op)
{
/* block of function used in eval()
*   only the v_stuff are needed
*/
int v_bond(float*, float),f_bond(float),v_angle(float*, float),f_angle(float);
int v_mmbond(float*, float),f_mmbond(float),v_mmangle(float*, float),f_mmangle(float);
int v_nonbon(float*, float),f_nonbon(float),v_torsion(float*, float),f_torsion(float);
int atom(),bond(),angle(),torsion();
int v_hybrid(float*, float),f_hybrid(float);
int restrain(),v_restrain(float*, float),f_restrain(float);
int tether(),v_tether(float*, float),f_tether(float);
int u_v_nonbon(float*, float), u_f_nonbon(float);
int v_noel(float*, float),a_noel(float*, float, int, int, FILE*);
int v_ho_noel(float*, float);
int a_bond(float*, float, int, int, FILE*),a_mmbond(float*, float, int, int, FILE*),a_angle(float*, float, int, int, FILE*),a_mmangle(float*, float, int, int, FILE*);
int a_nonbon(float*, float, int, int, FILE*),a_torsion(float*, float, int, int, FILE*),a_hybrid(float*, float, int, int, FILE*),a_restrain(float*, float, int, int, FILE*);
int a_tether(float*, float, int, int, FILE*);


float V,T,vt;
int ifs;
int i,j;
i = ilow;
j = ihigh;
if( ihigh < ilow ) j = ilow;
 V = 0.;
for( ifs = 0; ifs < nfs; ifs++ )
{
	vt = 0.; 
/*	(*vfs[ifs])(&vt,0.); */
	if( vfs[ifs] == v_bond)
	{ a_bond(&vt,0.,i,j,op);fprintf( op," %f bond energy\n",vt); goto DONE;}
	if( vfs[ifs] == v_mmbond)
	{a_mmbond(&vt,0.,i,j,op); fprintf( op," %f mm bond energy\n",vt); goto DONE;}
	if( vfs[ifs] == v_mmangle)
	{a_mmangle(&vt,0.,i,j,op); fprintf( op," %f mm angle energy\n",vt); goto DONE;}
	if( vfs[ifs] == v_angle)
	{a_angle(&vt,0.,i,j,op); fprintf( op," %f angle energy\n",vt); goto DONE;}
	if( vfs[ifs] == v_noel)
	{a_noel(&vt,0.,i,j,op); fprintf( op," %f noel energy\n",vt); goto DONE;}
	if( vfs[ifs] == v_ho_noel)
	{a_noel(&vt,0.,i,j,op); fprintf( op," %f noel energy\n",vt); goto DONE;}
	if( vfs[ifs] == u_v_nonbon)
	{a_nonbon(&vt,0.,i,j,op); fprintf( op," %f non-bonded energy\n",vt); goto DONE;}
	if( vfs[ifs] == v_nonbon)
	{a_nonbon(&vt,0.,i,j,op); fprintf( op," %f non-bonded energy\n",vt); goto DONE;}
	if( vfs[ifs] == v_torsion)
	{a_torsion(&vt,0.,i,j,op); fprintf( op," %f torsion energy\n",vt); goto DONE;}
	if( vfs[ifs] == v_hybrid)
	{a_hybrid(&vt,0.,i,j,op); fprintf( op," %f hybrid energy\n",vt); goto DONE;}
	if( vfs[ifs] == v_tether)
	{a_tether(&vt,0.,i,j,op); fprintf( op," %f tether restraint energy\n",vt); goto DONE;}
	if( vfs[ifs] == v_restrain)
	{a_restrain(&vt,0.,i,j,op); fprintf( op," %f restraint bond energy\n",vt); goto DONE;}
DONE:  
/* next statement is needed because cannot have a label at an end loop */
	V += vt;
	vt = 0.;
}
	fprintf( op," %f total potential energy\n",V);
/* end of routine */
}

