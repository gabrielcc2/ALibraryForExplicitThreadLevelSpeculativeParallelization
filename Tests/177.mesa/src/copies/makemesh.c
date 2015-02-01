/* makemesh.c */


/*
 * Generate the quadmesh for rendering in the MEsa benchmark.
 *
 * Brian Paul  brianp@elastic.avid.com
 *
 *
 * This program generates surfaces representing sfunction of the
 * form z = f(x, y).
 *
 *
 * Tunable parameters:
 *       ROWS - number of rows (Y values) to evaluate
 *       COLUMNS - number of columns (X values) to evaluate
 *
 *
 * Command line usage/parameters:
 *    makemesh [filename] [-help]
 * where
 *    [filename] is optional name of output mesh data file.  If not
 *            provided, the default is "mesa.mesh".
 *
 *
 * Compilation instructions:
 *    cc makemesh.c -lm -o makemesh
 */


#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/*
 * wwlk
 * Nota bene - Tunable parameters above!!!!!
 *
 * We can turn a HUGE knob here to increase the workload!
 * This knob is an order n^2 knob!
 * So let's raise it by 3x, increasing vertex processing by 9x.
 * Not a bad place to find almost an order of magnitude.
 *
 * Yes, this means the mesh is finer and the triangles smaller.
 * But the triangles are still not too small - about
 * 18 pixels/triangle for a 1024x1024 window.
 *
 * The days of 50 pixel triangles are long gone for non-textured
 * graphics - With a 50x50 mesh, the triangles are well over
 * 100 pixels each!  (And bonus points to the first person
 * who notes that this is a textured benchmark.  But it's a
 * 1D texture, mostly we are using the texture to highlight
 * constant Z.  Still, we are mostly looking at the shading
 * of the benchmark, not the texture.  The finer mesh
 * is justified.  Compare and contrast to say, Quake III, where
 * texture texture texture is what you see - if you are not looking
 * at all the blood and gore that is.)
 *
 *
 * But wait, I should discuss some intended consequences of
 * turning this knob.
 *   - Rasterization of pixels will diminish in
 *     importance compared to vertex processing.  This is fine.
 *   - Span setup will increase in importance during
 *     rasterization.  This is value neutral.
 *
 * BOTTOM LINE - no, I haven't set this knob to 11, and I
 * could in good conscience turn it higher still.
 * I could also in good conscience turn it lower.
 *
 */

/* #define ROWS 50							`						   wwlk */
/* #define COLUMNS 50												   wwlk */
#define ROWS 150													/* wwlk */
#define COLUMNS 150													/* wwlk */

static float SurfaceV[ROWS][COLUMNS][3];
static float SurfaceN[ROWS][COLUMNS][3];
static float Xmin = -10.0, Xmax = 10.0;
static float Ymin = -10.0, Ymax = 10.0;



/*
 * Compute z = f(x, y)
 */
static float Func1( float x, float y )
{
   float s = cos(x*0.5) * (10.0 - fabs(x));
   float t = cos(y*0.5) * (10.0 - fabs(y));
   return s * t * 0.05;
}


/*
 * Generate the surface of z = f(x, y) over closed intervals of x and y.
 */
static void MakeSurface( float (*func)(float, float) )
{
   float x, y;
   float dx, dy;
   int i, j;

   dx = (Xmax - Xmin) / (COLUMNS-1);
   dy = (Ymax - Ymin) / (ROWS-1);

   y = Ymin;
   for (i=0; i < ROWS; i++) {
      x = Xmin;
      for (j=0; j < COLUMNS; j++) {
         float z = (*func)(x, y);
         SurfaceV[i][j][0] = x;
         SurfaceV[i][j][1] = y;
         SurfaceV[i][j][2] = z;
         x += dx;
      }
      y += dy;
   }
}


/*
 * Compute the normal vectors of the surface
 */
static void ComputeNormals( void )
{
   int i, j;

   float dx = (Xmax - Xmin) / (COLUMNS-1);
   float dy = (Ymax - Ymin) / (ROWS-1);

   for (i=0; i < ROWS; i++) {
      for (j=0; j < COLUMNS; j++) {
         float ax = dx;
         float ay = 0.0;
         float az = SurfaceV[i][j+1][2] - SurfaceV[i][j][2];
         float bx = 0.0;
         float by = dy;
         float bz = SurfaceV[i+1][j][2] - SurfaceV[i][j][2];
         float nx = ay*bz - az*by;
         float ny = az*bx - ax*bz;
         float nz = ax*by - ay*bx;
         float len = sqrt(nx*nx + ny*ny + nz*nz);
         SurfaceN[i][j][0] = nx / len;
         SurfaceN[i][j][1] = ny / len;
         SurfaceN[i][j][2] = nz / len;
      }
   }
   /* fudge the edge normals */
   for (i=0; i < ROWS; i++) {
      SurfaceN[i][COLUMNS-1][0] = 0;
      SurfaceN[i][COLUMNS-1][1] = 0;
      SurfaceN[i][COLUMNS-1][2] = 1;
   }
   for (j=0; j < COLUMNS; j++) {
      SurfaceN[ROWS-1][j][0] = 0;
      SurfaceN[ROWS-1][j][1] = 0;
      SurfaceN[ROWS-1][j][2] = 1;
   }
}


static void WriteMesh(const char *filename)
{
   int i, j;
   FILE *f = fopen(filename, "w");
   if (!f) {
      printf("Error: couldn't open output file: %s\n", filename);
      return;
   }

   /* write number of rows and columns */
   fprintf(f, "%d %d\n", ROWS, COLUMNS);

   /* write vertices and normals */
   for (i=0; i<ROWS; i++) {
      for (j=0; j<ROWS; j++) {
	 fprintf(f, "%f %f %f  %f %f %f\n",
		 SurfaceV[i][j][0], SurfaceV[i][j][1], SurfaceV[i][j][2],
		 SurfaceN[i][j][0], SurfaceN[i][j][1], SurfaceN[i][j][2]);
      }
   }

   fclose(f);
}



int main(int argc, char *argv[])
{
   MakeSurface( Func1 );
   ComputeNormals();

   if (argc > 1)
      WriteMesh(argv[1]);
   else
      WriteMesh("mesa.mesh");

   return 0;
}
