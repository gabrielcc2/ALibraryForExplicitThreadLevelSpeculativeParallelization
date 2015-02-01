/* mesa3.c */


/*
 * Mesa SPEC benchmark
 *
 * Brian Paul  brianp@elastic.avid.com
 *
 *
 * This program renders images of a function of the form z = f(x, y).
 *
 * The rendering of the function is drawn with filled, smooth-shaded, lit
 * triangle strips.
 * The surface's geometry is computed only once.
 *
 * Tunable parameters:
 *    WIDTH, HEIGHT = size of image to generate, in pixels.
 *
 * Command line usage/parameters:
 * 	  mesa3 [-frames n] [-meshfile filename] [-ppmfile filename]
 * where
 *    -frames specifies number of frames of animation (1000 is default)
 *    -meshfile specifies name of input mesh ("mesa.mesh" is default)
 *    -ppmfile specifies the name of output ppm image file ("mesa.ppm"
 *           is default)
 *
 *
 * Version history:
 *    mesa1.c   - original version sent to Rahul.
 *    mesa2.c   - updated to use a 1-D texture for constant lines of Z.
 *    mesa3.c   - read mesh from file instead of generate during init.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "GL/osmesa.h"


/*
 * The frame buffer:
 */
#define WIDTH 800
#define HEIGHT 800



/*
 * The object we're rendering:
 */
static int NumRows, NumColumns;
static GLfloat *SurfaceV;
static GLfloat *SurfaceN;
static float Xmin = -10.0, Xmax = 10.0;
static float Ymin = -10.0, Ymax = 10.0;


/*
 * Viewing parameters:
 */
static GLfloat Xrot = -35.0, Yrot = 40.0;



/*
 * Read quadmesh from given file.
 */
static void ReadMesh(const char *filename)
{
   int i, j;
   FILE *f = fopen(filename, "r");
   if (!f) {
      printf("Error: couldn't open input mesh file: %s\n", filename);
      exit(1);
   }

   fscanf(f, "%d %d\n", &NumRows, &NumColumns);
   if (NumRows < 2) {
      printf("Error: number of mesh rows invalid\n");
      exit(1);
   }
   if (NumColumns < 2) {
      printf("Error: number of mesh columns invalid\n");
      exit(1);
   }

   /* Allocate storage for mesh vertices and normal vectors */
   SurfaceV = (GLfloat *) malloc(NumRows * NumColumns * 3 * sizeof(GLfloat));
   SurfaceN = (GLfloat *) malloc(NumRows * NumColumns * 3 * sizeof(GLfloat));
   if (!SurfaceV || !SurfaceN) {
      printf("Error: unable to allocate memory for mesh data\n");
      exit(1);
   }

   for (i=0; i<NumRows; i++) {
      for (j=0; j<NumColumns; j++) {
	 int k = (i * NumColumns + j) * 3;
	 float vx, vy, vz, nx, ny, nz;
	 fscanf(f, "%f %f %f  %f %f %f\n", &vx, &vy, &vz, &nx, &ny, &nz);
	 SurfaceV[k+0] = vx;
	 SurfaceV[k+1] = vy;
	 SurfaceV[k+2] = vz;
	 SurfaceN[k+0] = nx;
	 SurfaceN[k+1] = ny;
	 SurfaceN[k+2] = nz;
      }
   }

   /* Mesh read successfully */
   fclose(f);
}




/*
 * Draw the surface mesh.
 */
static void DrawMesh( void )
{
   int i, j;

   for (i=0; i < NumRows-1; i++) {
      glBegin(GL_TRIANGLE_STRIP);
      for (j=0; j < NumColumns; j++) {
	 int k0 = ( i    * NumColumns + j) * 3;
	 int k1 = ((i+1) * NumColumns + j) * 3;
         glNormal3fv(SurfaceN + k0);
         glVertex3fv(SurfaceV + k0);
         glNormal3fv(SurfaceN + k1);
         glVertex3fv(SurfaceV + k1);
      }
      glEnd();
   }
}



/*
 * This is the main rendering function.  We simply render the surface
 * 'frames' times with different rotations.
 */
static void Render( int frames )
{
   int i;
   for (i=0; i<frames; i++) {
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      glPushMatrix();
      glRotatef(-Xrot, 1, 0, 0);
      glRotatef(Yrot, 0, 1, 0);
      glRotatef(-90, 1, 0, 0);

      DrawMesh();

      glPopMatrix();

      Yrot += 5.0;
   }
}


/*
 * Called to setup the viewport, projection and viewing parameters.
 */
static void Reshape( int width, int height )
{
   GLfloat w = (float) width / (float) height;
   glViewport( 0, 0, width, height );
   glMatrixMode( GL_PROJECTION );
   glLoadIdentity();
   glFrustum( -w, w, -1.0, 1.0, 5.0, 100.0 );
   glMatrixMode( GL_MODELVIEW );
   glLoadIdentity();
   glTranslatef( 0.0, 0.0, -60.0 );
}


/*
 * Initialize library parameters.
 */
static void Init( void )
{
   GLint i;
   GLubyte texture[256];
   static GLfloat texPlane[4] = {0.0, 0.0, 0.1, 0.0};
   static GLfloat blue[4] = {0.2, 0.2, 1.0, 1.0};
   static GLfloat white[4] = {1.0, 1.0, 1.0, 1.0};
   static GLfloat pos[4] = {0.0, 2.0, 5.0, 0.0};
   static GLfloat gray[4] = {0.5, 0.5, 0.5, 1.0};
   glEnable(GL_LIGHTING);
   glEnable(GL_LIGHT0);

   glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, blue);
   glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, white);
   glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 15.0);
   glLightfv(GL_LIGHT0, GL_POSITION, pos);
   glLightfv(GL_LIGHT0, GL_DIFFUSE, gray);
   glLightfv(GL_LIGHT0, GL_SPECULAR, gray);
   glEnable(GL_DEPTH_TEST);

   /* Setup 1-D texture map to render lines of constant Z */
   for (i=0; i<256; i++) {
      if ((i % 16) < 1) {
         texture[i] = 0;
      }
      else {
         texture[i] = 255;
      }
   }
   glTexImage1D(GL_TEXTURE_1D, 0, 1, 256, 0,
                GL_LUMINANCE, GL_UNSIGNED_BYTE, texture);
   glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glEnable(GL_TEXTURE_1D);

   glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
   glTexGenfv(GL_S, GL_OBJECT_PLANE, texPlane);
   glEnable(GL_TEXTURE_GEN_S);

   Reshape(WIDTH, HEIGHT);
}


/*
 * Write current image in Buffer to the named file.
 */
static void WriteImage( const char *filename, int width, int height,
                        const void *buffer )
{
   FILE *f = fopen( filename, "w" );
   if (f) {
      int i, x, y;
      GLubyte *ptr;
      ptr = (GLubyte *) buffer;
      fprintf(f, "P6\n");
      fprintf(f, "# ppm-file created by %s\n", "SPEC mesa1");
      fprintf(f, "%i %i\n", width, height);
      fprintf(f, "255\n");
      fclose(f);
      f = fopen( filename, "ab" );  /* reopen in binary append mode */
      for (y=HEIGHT-1; y>=0; y--) {
         for (x=0; x<WIDTH; x++) {
            i = (y*WIDTH + x) * 4;
/*          fputc(ptr[i], f);*/   /* write red */
/*          fputc(ptr[i+1], f);*/ /* write green */
/*          fputc(ptr[i+2], f); *//* write blue */
            fprintf(f, "%d %d %d", ptr[i], ptr[i+1], ptr[i+2]); 
         }
      }
      fclose(f);
   }
}


int main(int argc, char *argv[])
{
   OSMesaContext ctx;
   void *buffer;
   int frames = 1000;
   char *ppmFile = "mesa.ppm";
   char *meshFile = "mesa.mesh";
   int i;

   for (i=1; i<argc; i++) {
      if (strcmp(argv[i],"-frames")==0) {
	 if (i+1 >= argc) {
	    printf("Error:  missing argument after -frames\n");
	    return 1;
	 }
	 frames = atoi(argv[i+1]);
	 i++;
	 if (frames <= 0) {
	    printf("Error:  number of frames must be >= 1\n");
	    return 1;
	 }
      }
      else if (strcmp(argv[i],"-ppmfile")==0) {
	 if (i+1 >= argc) {
	    printf("Error:  missing argument after -ppmfile\n");
	    return 1;
	 }
	 ppmFile = argv[i+1];
	 i++;
      }
      else if (strcmp(argv[i],"-meshfile")==0) {
	 if (i+1 >= argc) {
	    printf("Error:  missing argument after -meshfile\n");
	    return 1;
	 }
	 meshFile = argv[i+1];
	 i++;
      }
      else {
	 printf("Error:  unexpect command line parameter: %s\n", argv[i]);
	 return 1;
      }
   }

   /* Create an RGBA-mode context */
   ctx = OSMesaCreateContext( GL_RGBA, NULL );

   /* Allocate the image buffer */
   buffer = malloc( WIDTH * HEIGHT * 4 );

   /* Bind the buffer to the context and make it current */
   OSMesaMakeCurrent( ctx, buffer, GL_UNSIGNED_BYTE, WIDTH, HEIGHT );

   /* Initialize GL stuff */
   Init();

   /* Load surface mesh */
   ReadMesh(meshFile);

   Render( frames );

   /* If an optional output filename is given write a PPM image file */
   WriteImage( ppmFile, WIDTH, HEIGHT, buffer );

   /* free the image buffer */
   free( buffer );

   /* destroy the context */
   OSMesaDestroyContext( ctx );

   return 0;
}
