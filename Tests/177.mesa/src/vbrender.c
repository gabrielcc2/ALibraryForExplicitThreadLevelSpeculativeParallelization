/* $Id: depth.c,v 1.11 1997/07/24 01:24:45 brianp Exp $ */

/*
 * Mesa 3-D graphics library
 * Version:  2.4
 * Copyright (C) 1995-1997  Brian Paul
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


/*
 * $Log: depth.c,v $
 * Revision 1.11  1997/07/24 01:24:45  brianp
 * changed precompiled header symbol from PCH to PC_HEADER
 *
 * Revision 1.10  1997/05/28 03:24:22  brianp
 * added precompiled header (PCH) support
 *
 * Revision 1.9  1997/04/20 19:54:15  brianp
 * replaced abort() with gl_problem()
 *
 * Revision 1.8  1997/02/27 19:58:52  brianp
 * don't try to clear depth buffer if there isn't one
 *
 * Revision 1.7  1997/01/31 23:33:08  brianp
 * replaced calloc with malloc in gl_alloc_depth_buffer()
 *
 * Revision 1.6  1996/11/04 01:42:07  brianp
 * multiply Viewport.Sz and .Tz by DEPTH_SCALE
 *
 * Revision 1.5  1996/10/09 03:07:25  brianp
 * replaced malloc with calloc in gl_alloc_depth_buffer()
 *
 * Revision 1.4  1996/09/27 01:24:58  brianp
 * added missing default cases to switches
 *
 * Revision 1.3  1996/09/19 00:54:05  brianp
 * added missing returns after some gl_error() calls
 *
 * Revision 1.2  1996/09/15 14:19:16  brianp
 * now use GLframebuffer and GLvisual
 *
 * Revision 1.1  1996/09/13 01:38:16  brianp
 * Initial revision
 *
 */


/*
 * Depth buffer functions
 */


#ifdef PC_HEADER
#include "all.h"
#else
#include <stdlib.h>
#include <string.h>
#include "context.h"
#include "depth.h"
#include "dlist.h"
#include "macros.h"
#include "types.h"
#endif


GLcontext* global_ctx;
GLuint global_it_count, global_max_it;
bool it_finished;

/**********************************************************************/
/*****                          API Functions                     *****/
/**********************************************************************/



void gl_ClearDepth( GLcontext* ctx, GLclampd depth )
{
   if (INSIDE_BEGIN_END(ctx)) {
      gl_error( ctx, GL_INVALID_OPERATION, "glClearDepth" );
      return;
   }
   ctx->Depth.Clear = (GLfloat) CLAMP( depth, 0.0, 1.0 );
}



void gl_DepthFunc( GLcontext* ctx, GLenum func )
{
   if (INSIDE_BEGIN_END(ctx)) {
      gl_error( ctx, GL_INVALID_OPERATION, "glDepthFunc" );
      return;
   }

   switch (func) {
      case GL_NEVER:
      case GL_LESS:    /* (default) pass if incoming z < stored z */
      case GL_GEQUAL:
      case GL_LEQUAL:
      case GL_GREATER:
      case GL_NOTEQUAL:
      case GL_EQUAL:
      case GL_ALWAYS:
         ctx->Depth.Func = func;
         ctx->NewState |= NEW_RASTER_OPS;
         break;
      default:
         gl_error( ctx, GL_INVALID_ENUM, "glDepth.Func" );
   }
}



void gl_DepthMask( GLcontext* ctx, GLboolean flag )
{
   if (INSIDE_BEGIN_END(ctx)) {
      gl_error( ctx, GL_INVALID_OPERATION, "glDepthMask" );
      return;
   }

   /*
    * GL_TRUE indicates depth buffer writing is enabled (default)
    * GL_FALSE indicates depth buffer writing is disabled
    */
   ctx->Depth.Mask = flag;
   ctx->NewState |= NEW_RASTER_OPS;
}



void gl_DepthRange( GLcontext* ctx, GLclampd nearval, GLclampd farval )
{
   /*
    * nearval - specifies mapping of the near clipping plane to window
    *   coordinates, default is 0
    * farval - specifies mapping of the far clipping plane to window
    *   coordinates, default is 1
    *
    * After clipping and div by w, z coords are in -1.0 to 1.0,
    * corresponding to near and far clipping planes.  glDepthRange
    * specifies a linear mapping of the normalized z coords in
    * this range to window z coords.
    */

   GLfloat n, f;

   if (INSIDE_BEGIN_END(ctx)) {
      gl_error( ctx, GL_INVALID_OPERATION, "glDepthRange" );
      return;
   }

   n = (GLfloat) CLAMP( nearval, 0.0, 1.0 );
   f = (GLfloat) CLAMP( farval, 0.0, 1.0 );

   ctx->Viewport.Near = n;
   ctx->Viewport.Far = f;
   ctx->Viewport.Sz = DEPTH_SCALE * ((f - n) / 2.0);
   ctx->Viewport.Tz = DEPTH_SCALE * ((f - n) / 2.0 + n);
}



/**********************************************************************/
/*****                   Depth Testing Functions                  *****/
/**********************************************************************/


/*
 * Depth test horizontal spans of fragments.  These functions are called
 * via ctx->Driver.depth_test_span only.
 *
 * Input:  n - number of pixels in the span
 *         x, y - location of leftmost pixel in span in window coords
 *         z - array [n] of integer depth values
 * In/Out:  mask - array [n] of flags (1=draw pixel, 0=don't draw) 
 * Return:  number of pixels which passed depth test
 */


/*
 * glDepthFunc( any ) and glDepthMask( GL_TRUE or GL_FALSE ).
 */
GLuint gl_depth_test_span_generic( GLcontext* ctx,
                                   GLuint n, GLint x, GLint y,
                                   const GLdepth z[],
                                   GLubyte mask[] )
{
   GLdepth *zptr = Z_ADDRESS( ctx, x, y );
   GLubyte *m = mask;
   GLuint i;
   GLuint passed = 0;

   /* switch cases ordered from most frequent to less frequent */
   switch (ctx->Depth.Func) {
      case GL_LESS:
         if (ctx->Depth.Mask) {
	    /* Update Z buffer */
	    for (i=0; i<n; i++,zptr++,m++) {
	       if (*m) {
		  if (z[i] < *zptr) {
		     /* pass */
		     *zptr = z[i];
		     passed++;
		  }
		  else {
		     /* fail */
		     *m = 0;
		  }
	       }
	    }
	 }
	 else {
	    /* Don't update Z buffer */
	    for (i=0; i<n; i++,zptr++,m++) {
	       if (*m) {
		  if (z[i] < *zptr) {
		     /* pass */
		     passed++;
		  }
		  else {
		     *m = 0;
		  }
	       }
	    }
	 }
	 break;
      case GL_LEQUAL:
	 if (ctx->Depth.Mask) {
	    /* Update Z buffer */
	    for (i=0;i<n;i++,zptr++,m++) {
	       if (*m) {
		  if (z[i] <= *zptr) {
		     *zptr = z[i];
		     passed++;
		  }
		  else {
		     *m = 0;
		  }
	       }
	    }
	 }
	 else {
	    /* Don't update Z buffer */
	    for (i=0;i<n;i++,zptr++,m++) {
	       if (*m) {
		  if (z[i] <= *zptr) {
		     /* pass */
		     passed++;
		  }
		  else {
		     *m = 0;
		  }
	       }
	    }
	 }
	 break;
      case GL_GEQUAL:
	 if (ctx->Depth.Mask) {
	    /* Update Z buffer */
	    for (i=0;i<n;i++,zptr++,m++) {
	       if (*m) {
		  if (z[i] >= *zptr) {
		     *zptr = z[i];
		     passed++;
		  }
		  else {
		     *m = 0;
		  }
	       }
	    }
	 }
	 else {
	    /* Don't update Z buffer */
	    for (i=0;i<n;i++,zptr++,m++) {
	       if (*m) {
		  if (z[i] >= *zptr) {
		     /* pass */
		     passed++;
		  }
		  else {
		     *m = 0;
		  }
	       }
	    }
	 }
	 break;
      case GL_GREATER:
	 if (ctx->Depth.Mask) {
	    /* Update Z buffer */
	    for (i=0;i<n;i++,zptr++,m++) {
	       if (*m) {
		  if (z[i] > *zptr) {
		     *zptr = z[i];
		     passed++;
		  }
		  else {
		     *m = 0;
		  }
	       }
	    }
	 }
	 else {
	    /* Don't update Z buffer */
	    for (i=0;i<n;i++,zptr++,m++) {
	       if (*m) {
		  if (z[i] > *zptr) {
		     /* pass */
		     passed++;
		  }
		  else {
		     *m = 0;
		  }
	       }
	    }
	 }
	 break;
      case GL_NOTEQUAL:
	 if (ctx->Depth.Mask) {
	    /* Update Z buffer */
	    for (i=0;i<n;i++,zptr++,m++) {
	       if (*m) {
		  if (z[i] != *zptr) {
		     *zptr = z[i];
		     passed++;
		  }
		  else {
		     *m = 0;
		  }
	       }
	    }
	 }
	 else {
	    /* Don't update Z buffer */
	    for (i=0;i<n;i++,zptr++,m++) {
	       if (*m) {
		  if (z[i] != *zptr) {
		     /* pass */
		     passed++;
		  }
		  else {
		     *m = 0;
		  }
	       }
	    }
	 }
	 break;
      case GL_EQUAL:
	 if (ctx->Depth.Mask) {
	    /* Update Z buffer */
	    for (i=0;i<n;i++,zptr++,m++) {
	       if (*m) {
		  if (z[i] == *zptr) {
		     *zptr = z[i];
		     passed++;
		  }
		  else {
		     *m =0;
		  }
	       }
	    }
	 }
	 else {
	    /* Don't update Z buffer */
	    for (i=0;i<n;i++,zptr++,m++) {
	       if (*m) {
		  if (z[i] == *zptr) {
		     /* pass */
		     passed++;
		  }
		  else {
		     *m =0;
		  }
	       }
	    }
	 }
	 break;
      case GL_ALWAYS:
	 if (ctx->Depth.Mask) {
	    /* Update Z buffer */
	    for (i=0;i<n;i++,zptr++,m++) {
	       if (*m) {
		  *zptr = z[i];
		  passed++;
	       }
	    }
	 }
	 else {
	    /* Don't update Z buffer or mask */
	    passed = n;
	 }
	 break;
      case GL_NEVER:
	 for (i=0;i<n;i++) {
	    mask[i] = 0;
	 }
	 break;
      default:
         gl_problem(ctx, "Bad depth func in gl_depth_test_span_generic");
   } /*switch*/

   return passed;
}



/*
 * glDepthFunc(GL_LESS) and glDepthMask(GL_TRUE).
 */
GLuint gl_depth_test_span_less( GLcontext* ctx,
                                GLuint n, GLint x, GLint y, const GLdepth z[],
                                GLubyte mask[] )
{
   GLdepth *zptr = Z_ADDRESS( ctx, x, y );
   GLuint i;
   GLuint passed = 0;

   for (i=0; i<n; i++) {
      if (mask[i]) {
         if (z[i] < zptr[i]) {
            /* pass */
            zptr[i] = z[i];
            passed++;
         }
         else {
            /* fail */
            mask[i] = 0;
         }
      }
   }
   return passed;
}


/*
 * glDepthFunc(GL_GREATER) and glDepthMask(GL_TRUE).
 */
GLuint gl_depth_test_span_greater( GLcontext* ctx,
                                   GLuint n, GLint x, GLint y,
                                   const GLdepth z[],
                                   GLubyte mask[] )
{
   GLdepth *zptr = Z_ADDRESS( ctx, x, y );
   GLuint i;
   GLuint passed = 0;

   for (i=0; i<n; i++) {
      if (mask[i]) {
         if (z[i] > zptr[i]) {
            /* pass */
            zptr[i] = z[i];
            passed++;
         }
         else {
            /* fail */
            mask[i] = 0;
         }
      }
   }
   return passed;
}



/*
 * Depth test an array of randomly positioned fragments.
 */


#define ZADDR_SETUP   GLdepth *depthbuffer = ctx->Buffer->Depth; \
                      GLint width = ctx->Buffer->Width;

#define ZADDR( X, Y )   (depthbuffer + (Y) * width + (X) )



/*
 * glDepthFunc( any ) and glDepthMask( GL_TRUE or GL_FALSE ).
 */
void gl_depth_test_pixels_generic( GLcontext* ctx,
                                   GLuint n, const GLint x[], const GLint y[],
                                   const GLdepth z[], GLubyte mask[] )
{
   register GLdepth *zptr;
   register GLuint i;

   /* switch cases ordered from most frequent to less frequent */
   switch (ctx->Depth.Func) {
      case GL_LESS:
         if (ctx->Depth.Mask) {
	    /* Update Z buffer */
	    for (i=0; i<n; i++) {
	       if (mask[i]) {
		  zptr = Z_ADDRESS(ctx,x[i],y[i]);
		  if (z[i] < *zptr) {
		     /* pass */
		     *zptr = z[i];
		  }
		  else {
		     /* fail */
		     mask[i] = 0;
		  }
	       }
	    }
	 }
	 else {
	    /* Don't update Z buffer */
	    for (i=0; i<n; i++) {
	       if (mask[i]) {
		  zptr = Z_ADDRESS(ctx,x[i],y[i]);
		  if (z[i] < *zptr) {
		     /* pass */
		  }
		  else {
		     /* fail */
		     mask[i] = 0;
		  }
	       }
	    }
	 }
	 break;
      case GL_LEQUAL:
         if (ctx->Depth.Mask) {
	    /* Update Z buffer */
	    for (i=0; i<n; i++) {
	       if (mask[i]) {
		  zptr = Z_ADDRESS(ctx,x[i],y[i]);
		  if (z[i] <= *zptr) {
		     /* pass */
		     *zptr = z[i];
		  }
		  else {
		     /* fail */
		     mask[i] = 0;
		  }
	       }
	    }
	 }
	 else {
	    /* Don't update Z buffer */
	    for (i=0; i<n; i++) {
	       if (mask[i]) {
		  zptr = Z_ADDRESS(ctx,x[i],y[i]);
		  if (z[i] <= *zptr) {
		     /* pass */
		  }
		  else {
		     /* fail */
		     mask[i] = 0;
		  }
	       }
	    }
	 }
	 break;
      case GL_GEQUAL:
         if (ctx->Depth.Mask) {
	    /* Update Z buffer */
	    for (i=0; i<n; i++) {
	       if (mask[i]) {
		  zptr = Z_ADDRESS(ctx,x[i],y[i]);
		  if (z[i] >= *zptr) {
		     /* pass */
		     *zptr = z[i];
		  }
		  else {
		     /* fail */
		     mask[i] = 0;
		  }
	       }
	    }
	 }
	 else {
	    /* Don't update Z buffer */
	    for (i=0; i<n; i++) {
	       if (mask[i]) {
		  zptr = Z_ADDRESS(ctx,x[i],y[i]);
		  if (z[i] >= *zptr) {
		     /* pass */
		  }
		  else {
		     /* fail */
		     mask[i] = 0;
		  }
	       }
	    }
	 }
	 break;
      case GL_GREATER:
         if (ctx->Depth.Mask) {
	    /* Update Z buffer */
	    for (i=0; i<n; i++) {
	       if (mask[i]) {
		  zptr = Z_ADDRESS(ctx,x[i],y[i]);
		  if (z[i] > *zptr) {
		     /* pass */
		     *zptr = z[i];
		  }
		  else {
		     /* fail */
		     mask[i] = 0;
		  }
	       }
	    }
	 }
	 else {
	    /* Don't update Z buffer */
	    for (i=0; i<n; i++) {
	       if (mask[i]) {
		  zptr = Z_ADDRESS(ctx,x[i],y[i]);
		  if (z[i] > *zptr) {
		     /* pass */
		  }
		  else {
		     /* fail */
		     mask[i] = 0;
		  }
	       }
	    }
	 }
	 break;
      case GL_NOTEQUAL:
         if (ctx->Depth.Mask) {
	    /* Update Z buffer */
	    for (i=0; i<n; i++) {
	       if (mask[i]) {
		  zptr = Z_ADDRESS(ctx,x[i],y[i]);
		  if (z[i] != *zptr) {
		     /* pass */
		     *zptr = z[i];
		  }
		  else {
		     /* fail */
		     mask[i] = 0;
		  }
	       }
	    }
	 }
	 else {
	    /* Don't update Z buffer */
	    for (i=0; i<n; i++) {
	       if (mask[i]) {
		  zptr = Z_ADDRESS(ctx,x[i],y[i]);
		  if (z[i] != *zptr) {
		     /* pass */
		  }
		  else {
		     /* fail */
		     mask[i] = 0;
		  }
	       }
	    }
	 }
	 break;
      case GL_EQUAL:
         if (ctx->Depth.Mask) {
	    /* Update Z buffer */
	    for (i=0; i<n; i++) {
	       if (mask[i]) {
		  zptr = Z_ADDRESS(ctx,x[i],y[i]);
		  if (z[i] == *zptr) {
		     /* pass */
		     *zptr = z[i];
		  }
		  else {
		     /* fail */
		     mask[i] = 0;
		  }
	       }
	    }
	 }
	 else {
	    /* Don't update Z buffer */
	    for (i=0; i<n; i++) {
	       if (mask[i]) {
		  zptr = Z_ADDRESS(ctx,x[i],y[i]);
		  if (z[i] == *zptr) {
		     /* pass */
		  }
		  else {
		     /* fail */
		     mask[i] = 0;
		  }
	       }
	    }
	 }
	 break;
      case GL_ALWAYS:
	 if (ctx->Depth.Mask) {
	    /* Update Z buffer */
	    for (i=0; i<n; i++) {
	       if (mask[i]) {
		  zptr = Z_ADDRESS(ctx,x[i],y[i]);
		  *zptr = z[i];
	       }
	    }
	 }
	 else {
	    /* Don't update Z buffer or mask */
	 }
	 break;
      case GL_NEVER:
	 /* depth test never passes */
	 for (i=0;i<n;i++) {
	    mask[i] = 0;
	 }
	 break;
      default:
         gl_problem(ctx, "Bad depth func in gl_depth_test_pixels_generic");
   } /*switch*/
}



/*
 * glDepthFunc( GL_LESS ) and glDepthMask( GL_TRUE ).
 */
void gl_depth_test_pixels_less( GLcontext* ctx,
                                GLuint n, const GLint x[], const GLint y[],
                                const GLdepth z[], GLubyte mask[] )
{
   GLdepth *zptr;
   GLuint i;

   for (i=0; i<n; i++) {
      if (mask[i]) {
         zptr = Z_ADDRESS(ctx,x[i],y[i]);
         if (z[i] < *zptr) {
            /* pass */
            *zptr = z[i];
         }
         else {
            /* fail */
            mask[i] = 0;
         }
      }
   }
}


/*
 * glDepthFunc( GL_GREATER ) and glDepthMask( GL_TRUE ).
 */
void gl_depth_test_pixels_greater( GLcontext* ctx,
                                   GLuint n, const GLint x[], const GLint y[],
                                   const GLdepth z[], GLubyte mask[] )
{
   GLdepth *zptr;
   GLuint i;

   for (i=0; i<n; i++) {
      if (mask[i]) {
         zptr = Z_ADDRESS(ctx,x[i],y[i]);
         if (z[i] > *zptr) {
            /* pass */
            *zptr = z[i];
         }
         else {
            /* fail */
            mask[i] = 0;
         }
      }
   }
}




/**********************************************************************/
/*****                      Read Depth Buffer                     *****/
/**********************************************************************/


/*
 * Return a span of depth values from the depth buffer as floats in [0,1].
 * This function is only called through Driver.read_depth_span_float()
 * Input:  n - how many pixels
 *         x,y - location of first pixel
 * Output:  depth - the array of depth values
 */
void gl_read_depth_span_float( GLcontext* ctx,
                               GLuint n, GLint x, GLint y, GLfloat depth[] )
{
   GLdepth *zptr;
   GLfloat scale;
   GLuint i;

   scale = 1.0F / DEPTH_SCALE;

   if (ctx->Buffer->Depth) {
      zptr = Z_ADDRESS( ctx, x, y );
      for (i=0;i<n;i++) {
	 depth[i] = (GLfloat) zptr[i] * scale;
      }
   }
   else {
      for (i=0;i<n;i++) {
	 depth[i] = 0.0F;
      }
   }
}


/*
 * Return a span of depth values from the depth buffer as integers in
 * [0,MAX_DEPTH].
 * This function is only called through Driver.read_depth_span_int()
 * Input:  n - how many pixels
 *         x,y - location of first pixel
 * Output:  depth - the array of depth values
 */
void gl_read_depth_span_int( GLcontext* ctx,
                             GLuint n, GLint x, GLint y, GLdepth depth[] )
{
   if (ctx->Buffer->Depth) {
      GLdepth *zptr = Z_ADDRESS( ctx, x, y );
      MEMCPY( depth, zptr, n * sizeof(GLdepth) );
   }
   else {
      GLuint i;
      for (i=0;i<n;i++) {
	 depth[i] = 0.0;
      }
   }
}



/**********************************************************************/
/*****                Allocate and Clear Depth Buffer             *****/
/**********************************************************************/



/*
 * Allocate a new depth buffer.  If there's already a depth buffer allocated
 * it will be free()'d.  The new depth buffer will be uniniitalized.
 * This function is only called through Driver.alloc_depth_buffer.
 */
void gl_alloc_depth_buffer( GLcontext* ctx )
{
   /* deallocate current depth buffer if present */
   if (ctx->Buffer->Depth) {
      free(ctx->Buffer->Depth);
      ctx->Buffer->Depth = NULL;
   }

   /* allocate new depth buffer, but don't initialize it */
   ctx->Buffer->Depth = (GLdepth *) malloc( ctx->Buffer->Width
                                            * ctx->Buffer->Height
                                            * sizeof(GLdepth) );
   if (!ctx->Buffer->Depth) {
      /* out of memory */
      ctx->Depth.Test = GL_FALSE;
      gl_error( ctx, GL_OUT_OF_MEMORY, "Couldn't allocate depth buffer" );
   }
}




/*
 * Clear the depth buffer.  If the depth buffer doesn't exist yet we'll
 * allocate it now.
 * This function is only called through Driver.clear_depth_buffer.
 */
void gl_clear_depth_buffer( GLcontext* ctx )
{
   GLdepth clear_value = (GLdepth) (ctx->Depth.Clear * DEPTH_SCALE);

   if (ctx->Visual->DepthBits==0 || !ctx->Buffer->Depth) {
      /* no depth buffer */
      return;
   }

   /* The loops in this function have been written so the IRIX 5.3
    * C compiler can unroll them.  Hopefully other compilers can too!
    */

   if (ctx->Scissor.Enabled) {
      /* only clear scissor region */
      GLint y;
      for (y=ctx->Buffer->Ymin; y<=ctx->Buffer->Ymax; y++) {
         GLdepth *d = Z_ADDRESS( ctx, ctx->Buffer->Xmin, y );
         GLint n = ctx->Buffer->Xmax - ctx->Buffer->Xmin + 1;
         do {
            *d++ = clear_value;
            n--;
         } while (n);
      }
   }
   else {
      /* clear whole buffer */
      if (sizeof(GLdepth)==2 && (clear_value&0xff)==(clear_value>>8)) {
         /* lower and upper bytes of clear_value are same, use MEMSET */
         MEMSET( ctx->Buffer->Depth, clear_value&0xff,
                 2*ctx->Buffer->Width*ctx->Buffer->Height);
      }
      else {
         GLdepth *d = ctx->Buffer->Depth;
         GLint n = ctx->Buffer->Width * ctx->Buffer->Height;
         while (n>=16) {
            d[0] = clear_value;    d[1] = clear_value;
            d[2] = clear_value;    d[3] = clear_value;
            d[4] = clear_value;    d[5] = clear_value;
            d[6] = clear_value;    d[7] = clear_value;
            d[8] = clear_value;    d[9] = clear_value;
            d[10] = clear_value;   d[11] = clear_value;
            d[12] = clear_value;   d[13] = clear_value;
            d[14] = clear_value;   d[15] = clear_value;
            d += 16;
            n -= 16;
         }
         while (n>0) {
            *d++ = clear_value;
            n--;
         }
      }
   }
}


/* $Id: span.c,v 1.12 1997/08/14 01:12:37 brianp Exp $ */

/*
 * Mesa 3-D graphics library
 * Version:  2.4
 * Copyright (C) 1995-1997  Brian Paul
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


/*
 * $Log: span.c,v $
 * Revision 1.12  1997/08/14 01:12:37  brianp
 * replaced a few for loops with MEMSET calls
 *
 * Revision 1.11  1997/07/24 01:21:56  brianp
 * changed precompiled header symbol from PCH to PC_HEADER
 *
 * Revision 1.10  1997/05/28 03:26:29  brianp
 * added precompiled header (PCH) support
 *
 * Revision 1.9  1997/05/03 00:51:30  brianp
 * new texturing function call: gl_texture_pixels()
 *
 * Revision 1.8  1997/04/16 23:54:11  brianp
 * do per-pixel fog if texturing is enabled
 *
 * Revision 1.7  1997/02/09 19:53:43  brianp
 * now use TEXTURE_xD enable constants
 *
 * Revision 1.6  1997/02/09 18:43:34  brianp
 * added GL_EXT_texture3D support
 *
 * Revision 1.5  1997/01/28 22:17:44  brianp
 * new RGBA mode logic op support
 *
 * Revision 1.4  1996/09/25 03:22:05  brianp
 * added NO_DRAW_BIT support
 *
 * Revision 1.3  1996/09/15 14:18:55  brianp
 * now use GLframebuffer and GLvisual
 *
 * Revision 1.2  1996/09/15 01:48:58  brianp
 * removed #define NULL 0
 *
 * Revision 1.1  1996/09/13 01:38:16  brianp
 * Initial revision
 *
 */


/*
 * pixel span rasterization:
 * These functions simulate the rasterization pipeline.
 */


#ifdef PC_HEADER
#include "all.h"
#else
#include <string.h>
#include "alpha.h"
#include "alphabuf.h"
#include "blend.h"
#include "depth.h"
#include "fog.h"
#include "logic.h"
#include "macros.h"
#include "masking.h"
#include "scissor.h"
#include "span.h"
#include "stencil.h"
#include "texture.h"
#include "types.h"
#endif

#include "loop_speculator.h" 
#include <iostream>
using namespace std;
loop_speculator parallel_gtt;


/*
 * Apply the current polygon stipple pattern to a span of pixels.
 */
static void stipple_polygon_span( GLcontext *ctx,
                                  GLuint n, GLint x, GLint y, GLubyte mask[] )
{
   register GLuint i, m, stipple, highbit=0x80000000;

   stipple = ctx->PolygonStipple[y % 32];
   m = highbit >> (GLuint) (x % 32);

   for (i=0;i<n;i++) {
      if ((m & stipple)==0) {
	 mask[i] = 0;
      }
      m = m >> 1;
      if (m==0) {
	 m = 0x80000000;
      }
   }
}



/*
 * Clip a pixel span to the current buffer/window boundaries.
 * Return:  0 = all pixels clipped
 *          1 = at least one pixel is visible
 */
static GLuint clip_span( GLcontext *ctx,
                         GLint n, GLint x, GLint y, GLubyte mask[] )
{
   GLint i;

   /* Clip to top and bottom */
   if (y<0 || y>=ctx->Buffer->Height) {
      return 0;
   }

   /* Clip to left and right */
   if (x>=0 && x+n<=ctx->Buffer->Width) {
      /* no clipping needed */
      return 1;
   }
   else if (x+n<=0) {
      /* completely off left side */
      return 0;
   }
   else if (x>=ctx->Buffer->Width) {
      /* completely off right side */
      return 0;
   }
   else {
      /* clip-test each pixel, this could be done better */
      for (i=0;i<n;i++) {
         if (x+i<0 || x+i>=ctx->Buffer->Width) {
            mask[i] = 0;
         }
      }
      return 1;
   }
}



/*
 * Write a horizontal span of color index pixels to the frame buffer.
 * Stenciling, Depth-testing, etc. are done as needed.
 * Input:  n - number of pixels in the span
 *         x, y - location of leftmost pixel in the span
 *         z - array of [n] z-values
 *         index - array of [n] color indexes
 *         primitive - either GL_POINT, GL_LINE, GL_POLYGON, or GL_BITMAP
 */
void gl_write_index_span( GLcontext *ctx,
                          GLuint n, GLint x, GLint y, GLdepth z[],
			  GLuint index[], GLenum primitive )
{
   GLubyte mask[MAX_WIDTH];
   GLuint index_save[MAX_WIDTH];

   /* init mask to 1's (all pixels are to be written) */
   MEMSET(mask, 1, n);

   if ((ctx->RasterMask & WINCLIP_BIT) || primitive==GL_BITMAP) {
      if (clip_span(ctx,n,x,y,mask)==0) {
	 return;
      }
   }

   /* Per-pixel fog */
   if (ctx->Fog.Enabled
       && (ctx->Hint.Fog==GL_NICEST || primitive==GL_BITMAP)) {
      gl_fog_index_pixels( ctx, n, z, index );
   }

   /* Do the scissor test */
   if (ctx->Scissor.Enabled) {
      if (gl_scissor_span( ctx, n, x, y, mask )==0) {
	 return;
      }
   }

   /* Polygon Stippling */
   if (ctx->Polygon.StippleFlag && primitive==GL_POLYGON) {
      stipple_polygon_span( ctx, n, x, y, mask );
   }

   if (ctx->Stencil.Enabled) {
      /* first stencil test */
      if (gl_stencil_span( ctx, n, x, y, mask )==0) {
	 return;
      }
      /* depth buffering w/ stencil */
      gl_depth_stencil_span( ctx, n, x, y, z, mask );
   }
   else if (ctx->Depth.Test) {
      /* regular depth testing */
      if ((*ctx->Driver.DepthTestSpan)( ctx, n, x, y, z, mask )==0)  return;
   }

   if (ctx->RasterMask & NO_DRAW_BIT) {
      /* write no pixels */
      return;
   }

   if (ctx->RasterMask & FRONT_AND_BACK_BIT) {
      /* Save a copy of the indexes since LogicOp and IndexMask
       * may change them
       */
      MEMCPY( index_save, index, n * sizeof(GLuint) );
   }

   if (ctx->Color.SWLogicOpEnabled) {
      gl_logicop_ci_span( ctx, n, x, y, index, mask );
   }
   if (ctx->Color.SWmasking) {
      gl_mask_index_span( ctx, n, x, y, index );
   }

   /* write pixels */
   (*ctx->Driver.WriteIndexSpan)( ctx, n, x, y, index, mask );


   if (ctx->RasterMask & FRONT_AND_BACK_BIT) {
      /*** Also draw to back buffer ***/
      (*ctx->Driver.SetBuffer)( ctx, GL_BACK );
      MEMCPY( index, index_save, n * sizeof(GLuint) );
      if (ctx->Color.SWLogicOpEnabled) {
         gl_logicop_ci_span( ctx, n, x, y, index, mask );
      }
      if (ctx->Color.SWmasking) {
         gl_mask_index_span( ctx, n, x, y, index );
      }
      (*ctx->Driver.WriteIndexSpan)( ctx, n, x, y, index, mask );
      (*ctx->Driver.SetBuffer)( ctx, GL_FRONT );
   }
}




void gl_write_monoindex_span( GLcontext *ctx,
                              GLuint n, GLint x, GLint y, GLdepth z[],
			      GLuint index, GLenum primitive )
{
   GLuint i;
   GLubyte mask[MAX_WIDTH];
   GLuint index_save[MAX_WIDTH];

   /* init mask to 1's (all pixels are to be written) */
   MEMSET(mask, 1, n);

   if ((ctx->RasterMask & WINCLIP_BIT) || primitive==GL_BITMAP) {
      if (clip_span( ctx,n,x,y,mask)==0) {
	 return;
      }
   }

   /* Do the scissor test */
   if (ctx->Scissor.Enabled) {
      if (gl_scissor_span( ctx, n, x, y, mask )==0) {
	 return;
      }
   }

   /* Polygon Stippling */
   if (ctx->Polygon.StippleFlag && primitive==GL_POLYGON) {
      stipple_polygon_span( ctx, n, x, y, mask );
   }

   if (ctx->Stencil.Enabled) {
      /* first stencil test */
      if (gl_stencil_span( ctx, n, x, y, mask )==0) {
	 return;
      }
      /* depth buffering w/ stencil */
      gl_depth_stencil_span( ctx, n, x, y, z, mask );
   }
   else if (ctx->Depth.Test) {
      /* regular depth testing */
      if ((*ctx->Driver.DepthTestSpan)( ctx, n, x, y, z, mask )==0)  return;
   }

   if (ctx->RasterMask & NO_DRAW_BIT) {
      /* write no pixels */
      return;
   }

   if ((ctx->Fog.Enabled && (ctx->Hint.Fog==GL_NICEST || primitive==GL_BITMAP))
        || ctx->Color.SWLogicOpEnabled || ctx->Color.SWmasking) {
      GLuint ispan[MAX_WIDTH];
      /* index may change, replicate single index into an array */
      for (i=0;i<n;i++) {
	 ispan[i] = index;
      }

      if (ctx->Fog.Enabled
          && (ctx->Hint.Fog==GL_NICEST || primitive==GL_BITMAP)) {
	 gl_fog_index_pixels( ctx, n, z, ispan );
      }

      if (ctx->RasterMask & FRONT_AND_BACK_BIT) {
         MEMCPY( index_save, index, n * sizeof(GLuint) );
      }

      if (ctx->Color.SWLogicOpEnabled) {
	 gl_logicop_ci_span( ctx, n, x, y, ispan, mask );
      }

      if (ctx->Color.SWmasking) {
         gl_mask_index_span( ctx, n, x, y, ispan );
      }

      (*ctx->Driver.WriteIndexSpan)( ctx, n, x, y, ispan, mask );

      if (ctx->RasterMask & FRONT_AND_BACK_BIT) {
         /*** Also draw to back buffer ***/
         (*ctx->Driver.SetBuffer)( ctx, GL_BACK );
         for (i=0;i<n;i++) {
            ispan[i] = index;
         }
         if (ctx->Color.SWLogicOpEnabled) {
            gl_logicop_ci_span( ctx, n, x, y, ispan, mask );
         }
         if (ctx->Color.SWmasking) {
            gl_mask_index_span( ctx, n, x, y, ispan );
         }
         (*ctx->Driver.WriteIndexSpan)( ctx, n, x, y, ispan, mask );
         (*ctx->Driver.SetBuffer)( ctx, GL_FRONT );
      }
   }
   else {
      (*ctx->Driver.WriteMonoindexSpan)( ctx, n, x, y, mask );

      if (ctx->RasterMask & FRONT_AND_BACK_BIT) {
         /*** Also draw to back buffer ***/
         (*ctx->Driver.SetBuffer)( ctx, GL_BACK );
         (*ctx->Driver.WriteMonoindexSpan)( ctx, n, x, y, mask );
         (*ctx->Driver.SetBuffer)( ctx, GL_FRONT );
      }
   }
}



void gl_write_color_span( GLcontext *ctx,
                          GLuint n, GLint x, GLint y, GLdepth z[],
			  GLubyte r[], GLubyte g[],
			  GLubyte b[], GLubyte a[],
			  GLenum primitive )
{
   GLubyte mask[MAX_WIDTH];
   GLboolean write_all = GL_TRUE;
   GLubyte rtmp[MAX_WIDTH], gtmp[MAX_WIDTH], btmp[MAX_WIDTH], atmp[MAX_WIDTH];
   GLubyte *red, *green, *blue, *alpha;

   /* init mask to 1's (all pixels are to be written) */
   MEMSET(mask, 1, n);

   if ((ctx->RasterMask & WINCLIP_BIT) || primitive==GL_BITMAP) {
      if (clip_span( ctx,n,x,y,mask)==0) {
	 return;
      }
      write_all = GL_FALSE;
   }

   if ((primitive==GL_BITMAP && ctx->MutablePixels)
       || (ctx->RasterMask & FRONT_AND_BACK_BIT)) {
      /* must make a copy of the colors since they may be modified */
      MEMCPY( rtmp, r, n * sizeof(GLubyte) );
      MEMCPY( gtmp, g, n * sizeof(GLubyte) );
      MEMCPY( btmp, b, n * sizeof(GLubyte) );
      MEMCPY( atmp, a, n * sizeof(GLubyte) );
      red = rtmp;
      green = gtmp;
      blue = btmp;
      alpha = atmp;
   }
   else {
      red   = r;
      green = g;
      blue  = b;
      alpha = a;
   }

   /* Per-pixel fog */
   if (ctx->Fog.Enabled && (ctx->Hint.Fog==GL_NICEST || primitive==GL_BITMAP
                            || ctx->Texture.Enabled)) {
      gl_fog_color_pixels( ctx, n, z, red, green, blue, alpha );
   }

   /* Do the scissor test */
   if (ctx->Scissor.Enabled) {
      if (gl_scissor_span( ctx, n, x, y, mask )==0) {
	 return;
      }
      write_all = GL_FALSE;
   }

   /* Polygon Stippling */
   if (ctx->Polygon.StippleFlag && primitive==GL_POLYGON) {
      stipple_polygon_span( ctx, n, x, y, mask );
      write_all = GL_FALSE;
   }

   /* Do the alpha test */
   if (ctx->Color.AlphaEnabled) {
      if (gl_alpha_test( ctx, n, alpha, mask )==0) {
	 return;
      }
      write_all = GL_FALSE;
   }

   if (ctx->Stencil.Enabled) {
      /* first stencil test */
      if (gl_stencil_span( ctx, n, x, y, mask )==0) {
	 return;
      }
      /* depth buffering w/ stencil */
      gl_depth_stencil_span( ctx, n, x, y, z, mask );
      write_all = GL_FALSE;
   }
   else if (ctx->Depth.Test) {
      /* regular depth testing */
      GLuint m = (*ctx->Driver.DepthTestSpan)( ctx, n, x, y, z, mask );
      if (m==0) {
         return;
      }
      if (m<n) {
         write_all = GL_FALSE;
      }
   }

   if (ctx->RasterMask & NO_DRAW_BIT) {
      /* write no pixels */
      return;
   }

   /* logic op or blending */
   if (ctx->Color.SWLogicOpEnabled) {
      gl_logicop_rgba_span( ctx, n, x, y, red, green, blue, alpha, mask );
   }
   else if (ctx->Color.BlendEnabled) {
      gl_blend_span( ctx, n, x, y, red, green, blue, alpha, mask );
   }

   /* Color component masking */
   if (ctx->Color.SWmasking) {
      gl_mask_color_span( ctx, n, x, y, red, green, blue, alpha );
   }

   /* write pixels */
   (*ctx->Driver.WriteColorSpan)( ctx, n, x, y, red, green, blue, alpha,
                                  write_all ? NULL : mask );
   if (ctx->RasterMask & ALPHABUF_BIT) {
      gl_write_alpha_span( ctx, n, x, y, alpha, write_all ? NULL : mask );
   }

   if (ctx->RasterMask & FRONT_AND_BACK_BIT) {
      /*** Also render to back buffer ***/
      MEMCPY( rtmp, r, n * sizeof(GLubyte) );
      MEMCPY( gtmp, g, n * sizeof(GLubyte) );
      MEMCPY( btmp, b, n * sizeof(GLubyte) );
      MEMCPY( atmp, a, n * sizeof(GLubyte) );
      (*ctx->Driver.SetBuffer)( ctx, GL_BACK );
      if (ctx->Color.SWLogicOpEnabled) {
         gl_logicop_rgba_span( ctx, n, x, y, red, green, blue, alpha, mask );
      }
      else  if (ctx->Color.BlendEnabled) {
         gl_blend_span( ctx, n, x, y, red, green, blue, alpha, mask );
      }
      if (ctx->Color.SWmasking) {
         gl_mask_color_span( ctx, n, x, y, red, green, blue, alpha );
      }
      (*ctx->Driver.WriteColorSpan)( ctx, n, x, y, red, green, blue, alpha,
                              write_all ? NULL : mask );
      if (ctx->RasterMask & ALPHABUF_BIT) {
         ctx->Buffer->Alpha = ctx->Buffer->BackAlpha;
         gl_write_alpha_span( ctx, n, x, y, alpha, write_all ? NULL : mask );
         ctx->Buffer->Alpha = ctx->Buffer->FrontAlpha;
      }
      (*ctx->Driver.SetBuffer)( ctx, GL_FRONT );
   }

}



/*
 * Write a horizontal span of color pixels to the frame buffer.
 * The color is initially constant for the whole span.
 * Alpha-testing, stenciling, depth-testing, and blending are done as needed.
 * Input:  n - number of pixels in the span
 *         x, y - location of leftmost pixel in the span
 *         z - array of [n] z-values
 *         r, g, b, a - the color of the pixels
 *         primitive - either GL_POINT, GL_LINE, GL_POLYGON or GL_BITMAP.
 */
void gl_write_monocolor_span( GLcontext *ctx,
                              GLuint n, GLint x, GLint y, GLdepth z[],
			      GLint r, GLint g, GLint b, GLint a,
                              GLenum primitive )
{
   GLuint i;
   GLubyte mask[MAX_WIDTH];
   GLboolean write_all = GL_TRUE;
   GLubyte red[MAX_WIDTH], green[MAX_WIDTH], blue[MAX_WIDTH], alpha[MAX_WIDTH];

   /* init mask to 1's (all pixels are to be written) */
   MEMSET(mask, 1, n);

   if ((ctx->RasterMask & WINCLIP_BIT) || primitive==GL_BITMAP) {
      if (clip_span( ctx,n,x,y,mask)==0) {
	 return;
      }
      write_all = GL_FALSE;
   }

   /* Do the scissor test */
   if (ctx->Scissor.Enabled) {
      if (gl_scissor_span( ctx, n, x, y, mask )==0) {
	 return;
      }
      write_all = GL_FALSE;
   }

   /* Polygon Stippling */
   if (ctx->Polygon.StippleFlag && primitive==GL_POLYGON) {
      stipple_polygon_span( ctx, n, x, y, mask );
      write_all = GL_FALSE;
   }

   /* Do the alpha test */
   if (ctx->Color.AlphaEnabled) {
      GLubyte alpha[MAX_WIDTH];
      for (i=0;i<n;i++) {
         alpha[i] = a;
      }
      if (gl_alpha_test( ctx, n, alpha, mask )==0) {
	 return;
      }
      write_all = GL_FALSE;
   }

   if (ctx->Stencil.Enabled) {
      /* first stencil test */
      if (gl_stencil_span( ctx, n, x, y, mask )==0) {
	 return;
      }
      /* depth buffering w/ stencil */
      gl_depth_stencil_span( ctx, n, x, y, z, mask );
      write_all = GL_FALSE;
   }
   else if (ctx->Depth.Test) {
      /* regular depth testing */
      GLuint m = (*ctx->Driver.DepthTestSpan)( ctx, n, x, y, z, mask );
      if (m==0) {
         return;
      }
      if (m<n) {
         write_all = GL_FALSE;
      }
   }

   if (ctx->RasterMask & NO_DRAW_BIT) {
      /* write no pixels */
      return;
   }

   if (ctx->Color.BlendEnabled || ctx->Color.SWLogicOpEnabled
       || ctx->Color.SWmasking) {
      /* assign same color to each pixel */
      for (i=0;i<n;i++) {
	 if (mask[i]) {
	    red[i]   = r;
	    green[i] = g;
	    blue[i]  = b;
	    alpha[i] = a;
	 }
      }

      if (ctx->Color.SWLogicOpEnabled) {
         gl_logicop_rgba_span( ctx, n, x, y, red, green, blue, alpha, mask );
      }
      else if (ctx->Color.BlendEnabled) {
         gl_blend_span( ctx, n, x, y, red, green, blue, alpha, mask );
      }

      /* Color component masking */
      if (ctx->Color.SWmasking) {
         gl_mask_color_span( ctx, n, x, y, red, green, blue, alpha );
      }

      /* write pixels */
      (*ctx->Driver.WriteColorSpan)( ctx, n, x, y, red, green, blue, alpha,
                                     write_all ? NULL : mask );
      if (ctx->RasterMask & ALPHABUF_BIT) {
         gl_write_alpha_span( ctx, n, x, y, alpha, write_all ? NULL : mask );
      }

      if (ctx->RasterMask & FRONT_AND_BACK_BIT) {
         /*** Also draw to back buffer ***/
         for (i=0;i<n;i++) {
            if (mask[i]) {
               red[i]   = r;
               green[i] = g;
               blue[i]  = b;
               alpha[i] = a;
            }
         }
         (*ctx->Driver.SetBuffer)( ctx, GL_BACK );
         if (ctx->Color.SWLogicOpEnabled) {
            gl_logicop_rgba_span( ctx, n, x, y, red, green, blue, alpha, mask);
         }
         else if (ctx->Color.BlendEnabled) {
            gl_blend_span( ctx, n, x, y, red, green, blue, alpha, mask );
         }
         if (ctx->Color.SWmasking) {
            gl_mask_color_span( ctx, n, x, y, red, green, blue, alpha );
         }
         (*ctx->Driver.WriteColorSpan)( ctx, n, x, y, red, green, blue, alpha,
                                        write_all ? NULL : mask );
         (*ctx->Driver.SetBuffer)( ctx, GL_FRONT );
         if (ctx->RasterMask & ALPHABUF_BIT) {
            ctx->Buffer->Alpha = ctx->Buffer->BackAlpha;
            gl_write_alpha_span( ctx, n, x, y, alpha,
                                 write_all ? NULL : mask );
            ctx->Buffer->Alpha = ctx->Buffer->FrontAlpha;
         }
      }
   }
   else {
      (*ctx->Driver.WriteMonocolorSpan)( ctx, n, x, y, mask );
      if (ctx->RasterMask & ALPHABUF_BIT) {
         gl_write_mono_alpha_span( ctx, n, x, y, a, write_all ? NULL : mask );
      }
      if (ctx->RasterMask & FRONT_AND_BACK_BIT) {
         /* Also draw to back buffer */
         (*ctx->Driver.SetBuffer)( ctx, GL_BACK );
         (*ctx->Driver.WriteMonocolorSpan)( ctx, n, x, y, mask );
         (*ctx->Driver.SetBuffer)( ctx, GL_FRONT );
         if (ctx->RasterMask & ALPHABUF_BIT) {
            ctx->Buffer->Alpha = ctx->Buffer->BackAlpha;
            gl_write_mono_alpha_span( ctx, n, x, y, a,
                                      write_all ? NULL : mask );
            ctx->Buffer->Alpha = ctx->Buffer->FrontAlpha;
         }
      }
   }
}


/***********************************************************************/
/*
 * Write a horizontal span of textured pixels to the frame buffer.
 * The color of each pixel is different.
 * Alpha-testing, stenciling, depth-testing, and blending are done
 * as needed.
 * Input:  n - number of pixels in the span
 *         x, y - location of leftmost pixel in the span
 *         z - array of [n] z-values
 *         s, t - array of (s,t) texture coordinates for each pixel
 *         lambda - array of texture lambda values
 *         red, green, blue, alpha - array of [n] color components
 *         primitive - either GL_POINT, GL_LINE, GL_POLYGON or GL_BITMAP.
 */
void* parallel_gtt_f(void*);

void parallel_gl_write_texture_span( GLcontext *ctx,
                            GLuint n, GLint x, GLint y, GLdepth z[],
			    GLfloat s[], GLfloat t[], GLfloat u[],
                            GLfloat lambda[],
			    GLubyte r[], GLubyte g[],
			    GLubyte b[], GLubyte a[],
			    GLenum primitive )
{
   GLubyte mask[MAX_WIDTH];
   GLboolean write_all = GL_TRUE;
   GLubyte rtmp[MAX_WIDTH], gtmp[MAX_WIDTH], btmp[MAX_WIDTH], atmp[MAX_WIDTH];
   GLubyte *red, *green, *blue, *alpha;

   /* init mask to 1's (all pixels are to be written) */
   MEMSET(mask, 1, n);

   if ((ctx->RasterMask & WINCLIP_BIT) || primitive==GL_BITMAP) {
      if (clip_span( ctx,n,x,y,mask)==0) {
	 return;
      }
      write_all = GL_FALSE;
   }


   if (primitive==GL_BITMAP || (ctx->RasterMask & FRONT_AND_BACK_BIT)) {
      /* must make a copy of the colors since they may be modified */
      MEMCPY( rtmp, r, n * sizeof(GLubyte) );
      MEMCPY( gtmp, g, n * sizeof(GLubyte) );
      MEMCPY( btmp, b, n * sizeof(GLubyte) );
      MEMCPY( atmp, a, n * sizeof(GLubyte) );
      red = rtmp;
      green = gtmp;
      blue = btmp;
      alpha = atmp;
   }
   else {
      red   = r;
      green = g;
      blue  = b;
      alpha = a;
   }

   /* Texture */
   ASSERT(ctx->Texture.Enabled);
   gl_texture_pixels( ctx, n, s, t, u, lambda, red, green, blue, alpha );

   /* Per-pixel fog */
   if (ctx->Fog.Enabled && (ctx->Hint.Fog==GL_NICEST || primitive==GL_BITMAP
                            || ctx->Texture.Enabled)) {
      gl_fog_color_pixels( ctx, n, z, red, green, blue, alpha );
   }

   /* Do the scissor test */
   if (ctx->Scissor.Enabled) {
      if (gl_scissor_span( ctx, n, x, y, mask )==0) {
	 return;
      }
      write_all = GL_FALSE;
   }

   /* Polygon Stippling */
   if (ctx->Polygon.StippleFlag && primitive==GL_POLYGON) {
      stipple_polygon_span( ctx, n, x, y, mask );
      write_all = GL_FALSE;
   }

   /* Do the alpha test */
   if (ctx->Color.AlphaEnabled) {
      if (gl_alpha_test( ctx, n, alpha, mask )==0) {
	 return;
      }
      write_all = GL_FALSE;
   }

   if (ctx->Stencil.Enabled) {
      /* first stencil test */
      if (gl_stencil_span( ctx, n, x, y, mask )==0) {
	 return;
      }
      /* depth buffering w/ stencil */
      gl_depth_stencil_span( ctx, n, x, y, z, mask );
      write_all = GL_FALSE;
   }
   else if (ctx->Depth.Test) {
      /* regular depth testing */
      GLuint m = (*ctx->Driver.DepthTestSpan)( ctx, n, x, y, z, mask );
      if (!(global_it_count+1>=global_max_it && it_finished)){
		parallel_gtt.append(parallel_gtt_f, (void*)0);
      }
      if (m==0) {
         return;
      }
      if (m<n) {
         write_all = GL_FALSE;
      }
   }

   if (ctx->RasterMask & NO_DRAW_BIT) {
      /* write no pixels */
      return;
   }

   /* blending */
   if (ctx->Color.SWLogicOpEnabled) {
      gl_logicop_rgba_span( ctx, n, x, y, red, green, blue, alpha, mask );
   }
   else  if (ctx->Color.BlendEnabled) {
      gl_blend_span( ctx, n, x, y, red, green, blue, alpha, mask );
   }

   if (ctx->Color.SWmasking) {
      gl_mask_color_span( ctx, n, x, y, red, green, blue, alpha );
   }

   /* write pixels */
   (*ctx->Driver.WriteColorSpan)( ctx, n, x, y, red, green, blue, alpha,
                                  write_all ? NULL : mask );
   if (ctx->RasterMask & ALPHABUF_BIT) {
      gl_write_alpha_span( ctx, n, x, y, alpha, write_all ? NULL : mask );
   }

   if (ctx->RasterMask & FRONT_AND_BACK_BIT) {
      /* Also draw to back buffer */
      MEMCPY( rtmp, r, n * sizeof(GLubyte) );
      MEMCPY( gtmp, g, n * sizeof(GLubyte) );
      MEMCPY( btmp, b, n * sizeof(GLubyte) );
      MEMCPY( atmp, a, n * sizeof(GLubyte) );
      (*ctx->Driver.SetBuffer)( ctx, GL_BACK );
      if (ctx->Color.SWLogicOpEnabled) {
         gl_logicop_rgba_span( ctx, n, x, y, red, green, blue, alpha, mask );
      }
      else if (ctx->Color.BlendEnabled) {
         gl_blend_span( ctx, n, x, y, red, green, blue, alpha, mask );
      }
      if (ctx->Color.SWmasking) {
         gl_mask_color_span( ctx, n, x, y, red, green, blue, alpha );
      }
      (*ctx->Driver.WriteColorSpan)( ctx, n, x, y, red, green, blue, alpha,
                                     write_all ? NULL : mask );
      (*ctx->Driver.SetBuffer)( ctx, GL_FRONT );
      if (ctx->RasterMask & ALPHABUF_BIT) {
         ctx->Buffer->Alpha = ctx->Buffer->BackAlpha;
         gl_write_alpha_span( ctx, n, x, y, alpha, write_all ? NULL : mask );
         ctx->Buffer->Alpha = ctx->Buffer->FrontAlpha;
      }
   }
}

/***********************************************************************/
/*
 * Write a horizontal span of textured pixels to the frame buffer.
 * The color of each pixel is different.
 * Alpha-testing, stenciling, depth-testing, and blending are done
 * as needed.
 * Input:  n - number of pixels in the span
 *         x, y - location of leftmost pixel in the span
 *         z - array of [n] z-values
 *         s, t - array of (s,t) texture coordinates for each pixel
 *         lambda - array of texture lambda values
 *         red, green, blue, alpha - array of [n] color components
 *         primitive - either GL_POINT, GL_LINE, GL_POLYGON or GL_BITMAP.
 */
void gl_write_texture_span( GLcontext *ctx,
                            GLuint n, GLint x, GLint y, GLdepth z[],
			    GLfloat s[], GLfloat t[], GLfloat u[],
                            GLfloat lambda[],
			    GLubyte r[], GLubyte g[],
			    GLubyte b[], GLubyte a[],
			    GLenum primitive )
{
   GLubyte mask[MAX_WIDTH];
   GLboolean write_all = GL_TRUE;
   GLubyte rtmp[MAX_WIDTH], gtmp[MAX_WIDTH], btmp[MAX_WIDTH], atmp[MAX_WIDTH];
   GLubyte *red, *green, *blue, *alpha;

   /* init mask to 1's (all pixels are to be written) */
   MEMSET(mask, 1, n);

   if ((ctx->RasterMask & WINCLIP_BIT) || primitive==GL_BITMAP) {
      if (clip_span( ctx,n,x,y,mask)==0) {
	 return;
      }
      write_all = GL_FALSE;
   }


   if (primitive==GL_BITMAP || (ctx->RasterMask & FRONT_AND_BACK_BIT)) {
      /* must make a copy of the colors since they may be modified */
      MEMCPY( rtmp, r, n * sizeof(GLubyte) );
      MEMCPY( gtmp, g, n * sizeof(GLubyte) );
      MEMCPY( btmp, b, n * sizeof(GLubyte) );
      MEMCPY( atmp, a, n * sizeof(GLubyte) );
      red = rtmp;
      green = gtmp;
      blue = btmp;
      alpha = atmp;
   }
   else {
      red   = r;
      green = g;
      blue  = b;
      alpha = a;
   }

   /* Texture */
   ASSERT(ctx->Texture.Enabled);
   gl_texture_pixels( ctx, n, s, t, u, lambda, red, green, blue, alpha );

   /* Per-pixel fog */
   if (ctx->Fog.Enabled && (ctx->Hint.Fog==GL_NICEST || primitive==GL_BITMAP
                            || ctx->Texture.Enabled)) {
      gl_fog_color_pixels( ctx, n, z, red, green, blue, alpha );
   }

   /* Do the scissor test */
   if (ctx->Scissor.Enabled) {
      if (gl_scissor_span( ctx, n, x, y, mask )==0) {
	 return;
      }
      write_all = GL_FALSE;
   }

   /* Polygon Stippling */
   if (ctx->Polygon.StippleFlag && primitive==GL_POLYGON) {
      stipple_polygon_span( ctx, n, x, y, mask );
      write_all = GL_FALSE;
   }

   /* Do the alpha test */
   if (ctx->Color.AlphaEnabled) {
      if (gl_alpha_test( ctx, n, alpha, mask )==0) {
	 return;
      }
      write_all = GL_FALSE;
   }

   if (ctx->Stencil.Enabled) {
      /* first stencil test */
      if (gl_stencil_span( ctx, n, x, y, mask )==0) {
	 return;
      }
      /* depth buffering w/ stencil */
      gl_depth_stencil_span( ctx, n, x, y, z, mask );
      write_all = GL_FALSE;
   }
   else if (ctx->Depth.Test) {
      /* regular depth testing */
      GLuint m = (*ctx->Driver.DepthTestSpan)( ctx, n, x, y, z, mask );
      //MSJ: Introducir append aqui...
      if (m==0) {
         return;
      }
      if (m<n) {
         write_all = GL_FALSE;
      }
   }

   if (ctx->RasterMask & NO_DRAW_BIT) {
      /* write no pixels */
      return;
   }

   /* blending */
   if (ctx->Color.SWLogicOpEnabled) {
      gl_logicop_rgba_span( ctx, n, x, y, red, green, blue, alpha, mask );
   }
   else  if (ctx->Color.BlendEnabled) {
      gl_blend_span( ctx, n, x, y, red, green, blue, alpha, mask );
   }

   if (ctx->Color.SWmasking) {
      gl_mask_color_span( ctx, n, x, y, red, green, blue, alpha );
   }

   /* write pixels */
   (*ctx->Driver.WriteColorSpan)( ctx, n, x, y, red, green, blue, alpha,
                                  write_all ? NULL : mask );
   if (ctx->RasterMask & ALPHABUF_BIT) {
      gl_write_alpha_span( ctx, n, x, y, alpha, write_all ? NULL : mask );
   }

   if (ctx->RasterMask & FRONT_AND_BACK_BIT) {
      /* Also draw to back buffer */
      MEMCPY( rtmp, r, n * sizeof(GLubyte) );
      MEMCPY( gtmp, g, n * sizeof(GLubyte) );
      MEMCPY( btmp, b, n * sizeof(GLubyte) );
      MEMCPY( atmp, a, n * sizeof(GLubyte) );
      (*ctx->Driver.SetBuffer)( ctx, GL_BACK );
      if (ctx->Color.SWLogicOpEnabled) {
         gl_logicop_rgba_span( ctx, n, x, y, red, green, blue, alpha, mask );
      }
      else if (ctx->Color.BlendEnabled) {
         gl_blend_span( ctx, n, x, y, red, green, blue, alpha, mask );
      }
      if (ctx->Color.SWmasking) {
         gl_mask_color_span( ctx, n, x, y, red, green, blue, alpha );
      }
      (*ctx->Driver.WriteColorSpan)( ctx, n, x, y, red, green, blue, alpha,
                                     write_all ? NULL : mask );
      (*ctx->Driver.SetBuffer)( ctx, GL_FRONT );
      if (ctx->RasterMask & ALPHABUF_BIT) {
         ctx->Buffer->Alpha = ctx->Buffer->BackAlpha;
         gl_write_alpha_span( ctx, n, x, y, alpha, write_all ? NULL : mask );
         ctx->Buffer->Alpha = ctx->Buffer->FrontAlpha;
      }
   }
}



/*
 * Read RGBA pixels from frame buffer.  Clipping will be done to prevent
 * reading ouside the buffer's boundaries.
 */
void gl_read_color_span( GLcontext *ctx,
                         GLuint n, GLint x, GLint y,
			 GLubyte red[], GLubyte green[],
			 GLubyte blue[], GLubyte alpha[] )
{
   register GLuint i;

   if (y<0 || y>=ctx->Buffer->Height || x>=ctx->Buffer->Width) {
      /* completely above, below, or right */
      for (i=0;i<n;i++) {
	 red[i] = green[i] = blue[i] = alpha[i] = 0;
      }
   }
   else {
      if (x>=0 && x+n<=ctx->Buffer->Width) {
	 /* OK */
	 (*ctx->Driver.ReadColorSpan)( ctx, n, x, y, red, green, blue, alpha );
         if (ctx->RasterMask & ALPHABUF_BIT) {
            gl_read_alpha_span( ctx, n, x, y, alpha );
         }
      }
      else {
	 i = 0;
	 if (x<0) {
	    while (x<0 && n>0) {
	       red[i] = green[i] =  blue[i] = alpha[i] = 0;
	       x++;
	       n--;
	       i++;
	    }
	 }
	 n = MIN2( n, ctx->Buffer->Width - x );
	 (*ctx->Driver.ReadColorSpan)( ctx, n, x, y, red+i, green+i, blue+i, alpha+i);
         if (ctx->RasterMask & ALPHABUF_BIT) {
            gl_read_alpha_span( ctx, n, x, y, alpha+i );
         }
      }
   }
}




/*
 * Read CI pixels from frame buffer.  Clipping will be done to prevent
 * reading ouside the buffer's boundaries.
 */
void gl_read_index_span( GLcontext *ctx,
                         GLuint n, GLint x, GLint y, GLuint indx[] )
{
   register GLuint i;

   if (y<0 || y>=ctx->Buffer->Height || x>=ctx->Buffer->Width) {
      /* completely above, below, or right */
      for (i=0;i<n;i++) {
	 indx[i] = 0;
      }
   }
   else {
      if (x>=0 && x+n<=ctx->Buffer->Width) {
	 /* OK */
	 (*ctx->Driver.ReadIndexSpan)( ctx, n, x, y, indx );
      }
      else {
	 i = 0;
	 if (x<0) {
	    while (x<0 && n>0) {
	       indx[i] = 0;
	       x++;
	       n--;
	       i++;
	    }
	 }
	 n = MIN2( n, ctx->Buffer->Width - x );
	 (*ctx->Driver.ReadIndexSpan)( ctx, n, x, y, indx+i );
      }
   }
}


/* $Id: triangle.c,v 1.30 1997/08/27 01:20:05 brianp Exp $ */

/*
 * Mesa 3-D graphics library
 * Version:  2.4
 * Copyright (C) 1995-1997  Brian Paul
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


/*
 * $Log: triangle.c,v $
 * Revision 1.30  1997/08/27 01:20:05  brianp
 * moved texture completeness test out one level (Karl Anders Oygard)
 *
 * Revision 1.29  1997/07/24 01:26:05  brianp
 * changed precompiled header symbol from PCH to PC_HEADER
 *
 * Revision 1.28  1997/07/21 22:18:10  brianp
 * fixed bug in compute_lambda() thanks to Magnus Lundin
 *
 * Revision 1.27  1997/06/23 00:40:03  brianp
 * added a DEFARRAY/UNDEFARRAY for the Mac
 *
 * Revision 1.26  1997/06/20 02:51:38  brianp
 * changed color components from GLfixed to GLubyte
 *
 * Revision 1.25  1997/06/03 01:38:22  brianp
 * fixed divide by zero problem in feedback function (William Mitchell)
 *
 * Revision 1.24  1997/05/28 03:26:49  brianp
 * added precompiled header (PCH) support
 *
 * Revision 1.23  1997/05/17 03:40:55  brianp
 * refined textured triangle selection code (Mats Lofkvist)
 *
 * Revision 1.22  1997/05/03 00:51:02  brianp
 * removed calls to gl_texturing_enabled()
 *
 * Revision 1.21  1997/04/14 21:38:15  brianp
 * fixed a typo (dtdx instead of dudx) in lambda_textured_triangle()
 *
 * Revision 1.20  1997/04/14 02:00:39  brianp
 * #include "texstate.h" instead of "texture.h"
 *
 * Revision 1.19  1997/04/12 12:27:16  brianp
 * replaced ctx->TriangleFunc with ctx->Driver.TriangleFunc
 *
 * Revision 1.18  1997/04/02 03:12:06  brianp
 * replaced ctx->IdentityTexMat with ctx->TextureMatrixType
 *
 * Revision 1.17  1997/03/13 03:05:31  brianp
 * removed unused shift variable in feedback_triangle()
 *
 * Revision 1.16  1997/03/08 02:04:27  brianp
 * better implementation of feedback function
 *
 * Revision 1.15  1997/03/04 18:54:13  brianp
 * renamed mipmap_textured_triangle() to lambda_textured_triangle()
 * better comments about lambda and mipmapping
 *
 * Revision 1.14  1997/02/20 23:47:35  brianp
 * triangle feedback colors were wrong when using smooth shading
 *
 * Revision 1.13  1997/02/19 10:24:26  brianp
 * use a GLdouble instead of a GLfloat for wwvvInv (perspective correction)
 *
 * Revision 1.12  1997/02/09 19:53:43  brianp
 * now use TEXTURE_xD enable constants
 *
 * Revision 1.11  1997/02/09 18:51:02  brianp
 * added GL_EXT_texture3D support
 *
 * Revision 1.10  1997/01/16 03:36:43  brianp
 * added #include "texture.h"
 *
 * Revision 1.9  1997/01/09 19:50:49  brianp
 * now call gl_texturing_enabled()
 *
 * Revision 1.8  1996/12/20 20:23:30  brianp
 * the test for using general_textured_triangle() was wrong
 *
 * Revision 1.7  1996/12/12 22:37:30  brianp
 * projective textures didn't work right
 *
 * Revision 1.6  1996/11/08 02:21:21  brianp
 * added null drawing function for GL_NO_RASTER
 *
 * Revision 1.5  1996/10/01 03:31:17  brianp
 * use new FixedToDepth() macro
 *
 * Revision 1.4  1996/09/27 01:30:37  brianp
 * removed unneeded INTERP_ALPHA from flat_rgba_triangle()
 *
 * Revision 1.3  1996/09/15 14:19:16  brianp
 * now use GLframebuffer and GLvisual
 *
 * Revision 1.2  1996/09/15 01:48:58  brianp
 * removed #define NULL 0
 *
 * Revision 1.1  1996/09/13 01:38:16  brianp
 * Initial revision
 *
 */


/*
 * Triangle rasterizers
 */


#ifdef PC_HEADER
#include "all.h"
#else
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "depth.h"
#include "feedback.h"
#include "macros.h"
#include "span.h"
#include "texstate.h"
#include "triangle.h"
#include "types.h"
#include "vb.h"
#endif


/*
 * Put triangle in feedback buffer.
 */
static void feedback_triangle( GLcontext *ctx,
                               GLuint v0, GLuint v1, GLuint v2, GLuint pv )
{
   struct vertex_buffer *VB = ctx->VB;
   GLfloat color[4];
   GLuint i;
   GLfloat invRedScale   = ctx->Visual->InvRedScale;
   GLfloat invGreenScale = ctx->Visual->InvGreenScale;
   GLfloat invBlueScale  = ctx->Visual->InvBlueScale;
   GLfloat invAlphaScale = ctx->Visual->InvAlphaScale;

   FEEDBACK_TOKEN( ctx, (GLfloat) GL_POLYGON_TOKEN );
   FEEDBACK_TOKEN( ctx, (GLfloat) 3 );        /* three vertices */

   if (ctx->Light.ShadeModel==GL_FLAT) {
      /* flat shading - same color for each vertex */
      color[0] = (GLfloat) VB->Color[pv][0] * invRedScale;
      color[1] = (GLfloat) VB->Color[pv][1] * invGreenScale;
      color[2] = (GLfloat) VB->Color[pv][2] * invBlueScale;
      color[3] = (GLfloat) VB->Color[pv][3] * invAlphaScale;
   }

   for (i=0;i<3;i++) {
      GLfloat x, y, z, w;
      GLfloat tc[4];
      GLuint v;
      GLfloat invq;

      if (i==0)       v = v0;
      else if (i==1)  v = v1;
      else            v = v2;

      x = VB->Win[v][0];
      y = VB->Win[v][1];
      z = VB->Win[v][2] / DEPTH_SCALE;
      w = VB->Clip[v][3];

      if (ctx->Light.ShadeModel==GL_SMOOTH) {
         /* smooth shading - different color for each vertex */
         color[0] = VB->Color[v][0] * invRedScale;
         color[1] = VB->Color[v][1] * invGreenScale;
         color[2] = VB->Color[v][2] * invBlueScale;
         color[3] = VB->Color[v][3] * invAlphaScale;
      }

      invq = (VB->TexCoord[v][3]==0.0) ? 1.0 : (1.0F / VB->TexCoord[v][3]);
      tc[0] = VB->TexCoord[v][0] * invq;
      tc[1] = VB->TexCoord[v][1] * invq;
      tc[2] = VB->TexCoord[v][2] * invq;
      tc[3] = VB->TexCoord[v][3];

      gl_feedback_vertex( ctx, x, y, z, w, color, (GLfloat) VB->Index[v], tc );
   }
}



/*
 * Put triangle in selection buffer.
 */
static void select_triangle( GLcontext *ctx,
                             GLuint v0, GLuint v1, GLuint v2, GLuint pv )
{
   struct vertex_buffer *VB = ctx->VB;

   gl_update_hitflag( ctx, VB->Win[v0][2] / DEPTH_SCALE );
   gl_update_hitflag( ctx, VB->Win[v1][2] / DEPTH_SCALE );
   gl_update_hitflag( ctx, VB->Win[v2][2] / DEPTH_SCALE );
}



/*
 * Render a flat-shaded color index triangle.
 */
static void flat_ci_triangle( GLcontext *ctx,
                              GLuint v0, GLuint v1, GLuint v2, GLuint pv )
{
#define INTERP_Z 1

#define SETUP_CODE				\
   GLuint index = VB->Index[pv];		\
   if (!VB->MonoColor) {			\
      /* set the color index */			\
      (*ctx->Driver.Index)( ctx, index );	\
   }

#define INNER_LOOP( LEFT, RIGHT, Y )				\
	{							\
	   GLint i, n = RIGHT-LEFT;				\
	   GLdepth zspan[MAX_WIDTH];				\
	   if (n>0) {						\
	      for (i=0;i<n;i++) {				\
		 zspan[i] = FixedToDepth(ffz);			\
		 ffz += fdzdx;					\
	      }							\
	      gl_write_monoindex_span( ctx, n, LEFT, Y,		\
	                            zspan, index, GL_POLYGON );	\
	   }							\
	}

#include "tritemp.h"	      
}



/*
 * Render a smooth-shaded color index triangle.
 */
static void smooth_ci_triangle( GLcontext *ctx,
                                GLuint v0, GLuint v1, GLuint v2, GLuint pv )
{
#define INTERP_Z 1
#define INTERP_INDEX 1

#define INNER_LOOP( LEFT, RIGHT, Y )				\
	{							\
	   GLint i, n = RIGHT-LEFT;				\
	   GLdepth zspan[MAX_WIDTH];				\
           GLuint index[MAX_WIDTH];				\
	   if (n>0) {						\
	      for (i=0;i<n;i++) {				\
		 zspan[i] = FixedToDepth(ffz);			\
                 index[i] = FixedToInt(ffi);			\
		 ffz += fdzdx;					\
		 ffi += fdidx;					\
	      }							\
	      gl_write_index_span( ctx, n, LEFT, Y, zspan,	\
	                           index, GL_POLYGON );		\
	   }							\
	}

#include "tritemp.h"
}



/*
 * Render a flat-shaded RGBA triangle.
 */
static void flat_rgba_triangle( GLcontext *ctx,
                                GLuint v0, GLuint v1, GLuint v2, GLuint pv )
{
#define INTERP_Z 1

#define SETUP_CODE				\
   if (!VB->MonoColor) {			\
      /* set the color */			\
      GLubyte r = VB->Color[pv][0];		\
      GLubyte g = VB->Color[pv][1];		\
      GLubyte b = VB->Color[pv][2];		\
      GLubyte a = VB->Color[pv][3];		\
      (*ctx->Driver.Color)( ctx, r, g, b, a );	\
   }

#define INNER_LOOP( LEFT, RIGHT, Y )				\
	{							\
	   GLint i, n = RIGHT-LEFT;				\
	   GLdepth zspan[MAX_WIDTH];				\
	   if (n>0) {						\
	      for (i=0;i<n;i++) {				\
		 zspan[i] = FixedToDepth(ffz);			\
		 ffz += fdzdx;					\
	      }							\
              gl_write_monocolor_span( ctx, n, LEFT, Y, zspan,	\
                             VB->Color[pv][0], VB->Color[pv][1],\
                             VB->Color[pv][2], VB->Color[pv][3],\
			     GL_POLYGON );			\
	   }							\
	}

#include "tritemp.h"
}



/*
 * Render a smooth-shaded RGBA triangle.
 */
static void smooth_rgba_triangle( GLcontext *ctx,
                                  GLuint v0, GLuint v1, GLuint v2, GLuint pv )
{
#define INTERP_Z 1
#define INTERP_RGB 1
#define INTERP_ALPHA 1

#define INNER_LOOP( LEFT, RIGHT, Y )				\
	{							\
	   GLint i, n = RIGHT-LEFT;				\
	   GLdepth zspan[MAX_WIDTH];				\
	   GLubyte red[MAX_WIDTH], green[MAX_WIDTH];		\
	   GLubyte blue[MAX_WIDTH], alpha[MAX_WIDTH];		\
	   if (n>0) {						\
	      for (i=0;i<n;i++) {				\
		 zspan[i] = FixedToDepth(ffz);			\
		 red[i]   = FixedToInt(ffr);			\
		 green[i] = FixedToInt(ffg);			\
		 blue[i]  = FixedToInt(ffb);			\
		 alpha[i] = FixedToInt(ffa);			\
		 ffz += fdzdx;					\
		 ffr += fdrdx;					\
		 ffg += fdgdx;					\
		 ffb += fdbdx;					\
		 ffa += fdadx;					\
	      }							\
	      gl_write_color_span( ctx, n, LEFT, Y, zspan,	\
	                           red, green, blue, alpha,	\
				   GL_POLYGON );		\
	   }							\
	}

#include "tritemp.h"
}



/*
 * Render an RGB, GL_DECAL, textured triangle.
 * Interpolate S,T only w/out mipmapping or perspective correction.
 */
static void simple_textured_triangle( GLcontext *ctx, GLuint v0, GLuint v1,
                                      GLuint v2, GLuint pv )
{
#define INTERP_ST 1
#define S_SCALE twidth
#define T_SCALE theight
#define SETUP_CODE							\
   GLfloat twidth = (GLfloat) ctx->Texture.Current2D->Image[0]->Width;	\
   GLfloat theight = (GLfloat) ctx->Texture.Current2D->Image[0]->Height;\
   GLint twidth_log2 = ctx->Texture.Current2D->Image[0]->WidthLog2;	\
   GLubyte *texture = ctx->Texture.Current2D->Image[0]->Data;		\
   GLint smask = ctx->Texture.Current2D->Image[0]->Width - 1;		\
   GLint tmask = ctx->Texture.Current2D->Image[0]->Height - 1;

#define INNER_LOOP( LEFT, RIGHT, Y )				\
	{							\
	   GLint i, n = RIGHT-LEFT;				\
	   GLubyte red[MAX_WIDTH], green[MAX_WIDTH];		\
	   GLubyte blue[MAX_WIDTH], alpha[MAX_WIDTH];		\
	   if (n>0) {						\
	      for (i=0;i<n;i++) {				\
                 GLint s = FixedToInt(ffs) & smask;		\
                 GLint t = FixedToInt(fft) & tmask;		\
                 GLint pos = (t << twidth_log2) + s;		\
                 pos = pos + pos  + pos;  /* multiply by 3 */	\
                 red[i]   = texture[pos];			\
                 green[i] = texture[pos+1];			\
                 blue[i]  = texture[pos+2];			\
                 alpha[i] = 255;				\
		 ffs += fdsdx;					\
		 fft += fdtdx;					\
	      }							\
              (*ctx->Driver.WriteColorSpan)( ctx, n, LEFT, Y,	\
                             red, green, blue, alpha, NULL );	\
	   }							\
	}

#include "tritemp.h"
}



/*
 * Render an RGB, GL_DECAL, textured triangle.
 * Interpolate S,T, GL_LESS depth test, w/out mipmapping or
 * perspective correction.
 */
static void simple_z_textured_triangle( GLcontext *ctx, GLuint v0, GLuint v1,
                                      GLuint v2, GLuint pv )
{
#define INTERP_Z 1
#define INTERP_ST 1
#define S_SCALE twidth
#define T_SCALE theight
#define SETUP_CODE							\
   GLfloat twidth = (GLfloat) ctx->Texture.Current2D->Image[0]->Width;	\
   GLfloat theight = (GLfloat) ctx->Texture.Current2D->Image[0]->Height;\
   GLint twidth_log2 = ctx->Texture.Current2D->Image[0]->WidthLog2;	\
   GLubyte *texture = ctx->Texture.Current2D->Image[0]->Data;		\
   GLint smask = ctx->Texture.Current2D->Image[0]->Width - 1;		\
   GLint tmask = ctx->Texture.Current2D->Image[0]->Height - 1;

#define INNER_LOOP( LEFT, RIGHT, Y )				\
	{							\
	   GLint i, n = RIGHT-LEFT;				\
	   GLubyte red[MAX_WIDTH], green[MAX_WIDTH];		\
	   GLubyte blue[MAX_WIDTH], alpha[MAX_WIDTH];		\
           GLubyte mask[MAX_WIDTH];				\
	   if (n>0) {						\
	      for (i=0;i<n;i++) {				\
                 GLdepth z = FixedToDepth(ffz);			\
                 if (z < zRow[i]) {				\
                    GLint s = FixedToInt(ffs) & smask;		\
                    GLint t = FixedToInt(fft) & tmask;		\
                    GLint pos = (t << twidth_log2) + s;		\
                    pos = pos + pos  + pos;  /* multiply by 3 */\
                    red[i]   = texture[pos];			\
                    green[i] = texture[pos+1];			\
                    blue[i]  = texture[pos+2];			\
                    alpha[i] = 255;				\
		    zRow[i] = z;				\
                    mask[i] = 1;				\
                 }						\
                 else {						\
                    mask[i] = 0;				\
                 }						\
		 ffz += fdzdx;					\
		 ffs += fdsdx;					\
		 fft += fdtdx;					\
	      }							\
              (*ctx->Driver.WriteColorSpan)( ctx, n, LEFT, Y,	\
                             red, green, blue, alpha, mask );	\
	   }							\
	}

#include "tritemp.h"
}


#ifdef PROFILE
#  define START_PROFILE				\
	{					\
	   GLdouble t0 = gl_time();

#  define END_PROFILE( TIMER, COUNTER, INCR )	\
	   TIMER += (gl_time() - t0);		\
	   COUNTER += INCR;			\
	}
#else
#  define START_PROFILE
#  define END_PROFILE( TIMER, COUNTER, INCR )
#endif


/*FIRST CHUNK*********************************************************************************************/

   typedef struct {
        GLint v0, v1;     /* Y(v0) < Y(v1) */
	GLfloat dx;	/* X(v1) - X(v0) */
	GLfloat dy;	/* Y(v1) - Y(v0) */
	GLfixed fdxdy;	/* dx/dy in fixed-point */
	GLfixed fsx;	/* first sample point x coord */
	GLfixed fsy;
	GLfloat adjy;	/* adjust from v[0]->fy to fsy, scaled */
	GLint lines;	/* number of lines to be sampled on this edge */
	GLfixed fx0;	/* fixed pt X of lower endpoint */
   } EdgeT;

   struct vertex_buffer *VB = ctx->VB;
   EdgeT eMaj, eTop, eBot;
   GLfloat oneOverArea, area;
   int vMin, vMid, vMax; 
   GLfloat y0, y1, y2;
   GLfixed vMin_fx, vMid_fx, vMin_fy, vMid_fy, vMax_fy;
      GLint ltor;		/* true if scanning left-to-right */
      GLfloat dzdx, dzdy;      GLfixed fdzdx;
      GLfloat drdx, drdy;      GLfixed fdrdx;
      GLfloat dgdx, dgdy;      GLfixed fdgdx;
      GLfloat dbdx, dbdy;      GLfixed fdbdx;
      GLfloat dadx, dady;      GLfixed fdadx;
      GLfloat dsdx, dsdy;
      GLfloat dtdx, dtdy;
      GLfloat dwdx, dwdy;
      GLfloat dudx, dudy;
      GLfloat dvdx, dvdy;
         int subTriangle;
         GLfixed fx, fxLeftEdge, fxRightEdge, fdxLeftEdge, fdxRightEdge;
         GLfixed fdxOuter;
         int idxOuter;
         float dxOuter;
         GLfixed fError, fdError;
         float adjx, adjy;
         GLfixed fy;
         int iy;
         GLfixed fr, fdrOuter, fdrInner;
         GLfixed fg, fdgOuter, fdgInner;
         GLfixed fb, fdbOuter, fdbInner;
         GLdepth *zRow;
         int dZRowOuter, dZRowInner;  /* offset in bytes */
         GLfixed fz, fdzOuter, fdzInner;
         GLfixed fa, fdaOuter, fdaInner;
         GLfloat sLeft, dsOuter, dsInner;
         GLfloat tLeft, dtOuter, dtInner;
         GLfloat wLeft, dwOuter, dwInner;
         GLfloat uLeft, duOuter, duInner;
         GLfloat vLeft, dvOuter, dvInner;

            EdgeT *eLeft, *eRight;
            int setupLeft, setupRight;
            int lines;
GLfixed ffz;

GLfixed ffr, ffg, ffb, ffa, ffrend, ffgend, ffbend, ffaend; GLfloat ss, uu, tt, ww, vv; GLint left, right;

	   GLubyte red[MAX_WIDTH], green[MAX_WIDTH];		
	   GLubyte blue[MAX_WIDTH], alpha[MAX_WIDTH];		
	   GLdepth zspan[MAX_WIDTH];
           GLfloat s[MAX_WIDTH], t[MAX_WIDTH], u[MAX_WIDTH];
/*
 * Render a smooth-shaded, textured, RGBA triangle.
 * Interpolate S,T,U with perspective correction, w/out mipmapping.
 * Note: we use texture coordinates S,T,U,V instead of S,T,R,Q because
 * R is already used for red.
 */
static void first_chunk_general_textured_triangle( GLcontext *ctx, GLuint v0, GLuint v1,
                                       GLuint v2, GLuint pv )
{

START_PROFILE

#define INTERP_Z 1
#define INTERP_RGB 1
#define INTERP_ALPHA 1
#define INTERP_STW 1
#define INTERP_UV 1
#define SETUP_CODE						\
   GLboolean flat_shade = (ctx->Light.ShadeModel==GL_FLAT);	\
   GLint r, g, b, a;						\
   if (flat_shade) {						\
      r = VB->Color[pv][0];					\
      g = VB->Color[pv][1];					\
      b = VB->Color[pv][2];					\
      a = VB->Color[pv][3];					\
   }
#define INNER_LOOP( LEFT, RIGHT, Y )				\
	{							\
	   GLint i, n = RIGHT-LEFT;				\
	   if (n>0) {						\
              if (flat_shade) {					\
                 for (i=0;i<n;i++) {				\
		    GLdouble wwvvInv = 1.0 / (ww*vv);		\
		    zspan[i] = FixedToDepth(ffz);		\
		    red[i]   = r;				\
		    green[i] = g;				\
		    blue[i]  = b;				\
		    alpha[i] = a;				\
		    s[i] = ss*wwvvInv;				\
		    t[i] = tt*wwvvInv;				\
		    u[i] = uu*wwvvInv;				\
		    ffz += fdzdx;				\
		    ss += dsdx;					\
		    tt += dtdx;					\
		    uu += dudx;					\
		    vv += dvdx;					\
		    ww += dwdx;					\
		 }						\
              }							\
              else {						\
                 for (i=0;i<n;i++) {				\
		    GLdouble wwvvInv = 1.0 / (ww*vv);		\
		    zspan[i] = FixedToDepth(ffz);		\
		    red[i]   = FixedToInt(ffr);			\
		    green[i] = FixedToInt(ffg);			\
		    blue[i]  = FixedToInt(ffb);			\
		    alpha[i] = FixedToInt(ffa);			\
		    s[i] = ss*wwvvInv;				\
		    t[i] = tt*wwvvInv;				\
		    u[i] = uu*wwvvInv;				\
		    ffz += fdzdx;				\
		    ffr += fdrdx;				\
		    ffg += fdgdx;				\
		    ffb += fdbdx;				\
		    ffa += fdadx;				\
		    ss += dsdx;					\
		    tt += dtdx;					\
		    uu += dudx;					\
		    ww += dwdx;					\
		    vv += dvdx;					\
		 }						\
              }							\
	      parallel_gtt.append(parallel_gtt_f, (void*)0); 	\
	      /*Era: parallel_gl_write_texture_span( ctx, n, LEFT, Y, zspan,	\
                                     s, t, u, NULL, 		\
	                             red, green, blue, alpha,	\
				     GL_POLYGON );		\*/
	   }							\
	}

/*void triangle( GLcontext *ctx, GLuint v0, GLuint v1, GLuint v2, GLuint pv )*/
{
      /* vertex indexes:  Y(vMin)<=Y(vMid)<=Y(vMax) */

   /* find the order of the 3 vertices along the Y axis */
   {
      y0 = VB->Win[v0][1];
      y1 = VB->Win[v1][1];
      y2 = VB->Win[v2][1];

      if (y0<=y1) {
	 if (y1<=y2) {
	    vMin = v0;   vMid = v1;   vMax = v2;   /* y0<=y1<=y2 */
	 }
	 else if (y2<=y0) {
	    vMin = v2;   vMid = v0;   vMax = v1;   /* y2<=y0<=y1 */
	 }
	 else {
	    vMin = v0;   vMid = v2;   vMax = v1;   /* y0<=y2<=y1 */
	 }
      }
      else {
	 if (y0<=y2) {
	    vMin = v1;   vMid = v0;   vMax = v2;   /* y1<=y0<=y2 */
	 }
	 else if (y2<=y1) {
	    vMin = v2;   vMid = v1;   vMax = v0;   /* y2<=y1<=y0 */
	 }
	 else {
	    vMin = v1;   vMid = v2;   vMax = v0;   /* y1<=y2<=y0 */
	 }
      }
   }

   /* vertex/edge relationship */
   eMaj.v0 = vMin;   eMaj.v1 = vMax;   /*TODO: .v1's not needed */
   eTop.v0 = vMid;   eTop.v1 = vMax;
   eBot.v0 = vMin;   eBot.v1 = vMid;

   /* compute deltas for each edge:  vertex[v1] - vertex[v0] */
   eMaj.dx = VB->Win[vMax][0] - VB->Win[vMin][0];
   eMaj.dy = VB->Win[vMax][1] - VB->Win[vMin][1];
   eTop.dx = VB->Win[vMax][0] - VB->Win[vMid][0];
   eTop.dy = VB->Win[vMax][1] - VB->Win[vMid][1];
   eBot.dx = VB->Win[vMid][0] - VB->Win[vMin][0];
   eBot.dy = VB->Win[vMid][1] - VB->Win[vMin][1];

   /* compute oneOverArea */
   {
      area = eMaj.dx * eBot.dy - eBot.dx * eMaj.dy;
      if (area>-0.05f && area<0.05f) {
         return;  /* very small; CULLED */
      }
      oneOverArea = 1.0F / area;
   }

   /* Edge setup.  For a triangle strip these could be reused... */
   {
      /* fixed point Y coordinates */
      vMin_fx = FloatToFixed(VB->Win[vMin][0] + 0.5F);
      vMin_fy = FloatToFixed(VB->Win[vMin][1] - 0.5F);
      vMid_fx = FloatToFixed(VB->Win[vMid][0] + 0.5F);
      vMid_fy = FloatToFixed(VB->Win[vMid][1] - 0.5F);
      vMax_fy = FloatToFixed(VB->Win[vMax][1] - 0.5F);

      eMaj.fsy = FixedCeil(vMin_fy);
      eMaj.lines = FixedToInt(vMax_fy + FIXED_ONE - FIXED_EPSILON - eMaj.fsy);
      if (eMaj.lines > 0) {
         GLfloat dxdy = eMaj.dx / eMaj.dy;
         eMaj.fdxdy = SignedFloatToFixed(dxdy);
         eMaj.adjy = (GLfloat) (eMaj.fsy - vMin_fy);  /* SCALED! */
         eMaj.fx0 = vMin_fx;
         eMaj.fsx = eMaj.fx0 + (GLfixed) (eMaj.adjy * dxdy);
      }
      else {
         return;  /*CULLED*/
      }

      eTop.fsy = FixedCeil(vMid_fy);
      eTop.lines = FixedToInt(vMax_fy + FIXED_ONE - FIXED_EPSILON - eTop.fsy);
      if (eTop.lines > 0) {
         GLfloat dxdy = eTop.dx / eTop.dy;
         eTop.fdxdy = SignedFloatToFixed(dxdy);
         eTop.adjy = (GLfloat) (eTop.fsy - vMid_fy); /* SCALED! */
         eTop.fx0 = vMid_fx;
         eTop.fsx = eTop.fx0 + (GLfixed) (eTop.adjy * dxdy);
      }

      eBot.fsy = FixedCeil(vMin_fy);
      eBot.lines = FixedToInt(vMid_fy + FIXED_ONE - FIXED_EPSILON - eBot.fsy);
      if (eBot.lines > 0) {
         GLfloat dxdy = eBot.dx / eBot.dy;
         eBot.fdxdy = SignedFloatToFixed(dxdy);
         eBot.adjy = (GLfloat) (eBot.fsy - vMin_fy);  /* SCALED! */
         eBot.fx0 = vMin_fx;
         eBot.fsx = eBot.fx0 + (GLfixed) (eBot.adjy * dxdy);
      }
   }

   /*
    * Conceptually, we view a triangle as two subtriangles
    * separated by a perfectly horizontal line.  The edge that is
    * intersected by this line is one with maximal absolute dy; we
    * call it a ``major'' edge.  The other two edges are the
    * ``top'' edge (for the upper subtriangle) and the ``bottom''
    * edge (for the lower subtriangle).  If either of these two
    * edges is horizontal or very close to horizontal, the
    * corresponding subtriangle might cover zero sample points;
    * we take care to handle such cases, for performance as well
    * as correctness.
    *
    * By stepping rasterization parameters along the major edge,
    * we can avoid recomputing them at the discontinuity where
    * the top and bottom edges meet.  However, this forces us to
    * be able to scan both left-to-right and right-to-left. 
    * Also, we must determine whether the major edge is at the
    * left or right side of the triangle.  We do this by
    * computing the magnitude of the cross-product of the major
    * and top edges.  Since this magnitude depends on the sine of
    * the angle between the two edges, its sign tells us whether
    * we turn to the left or to the right when travelling along
    * the major edge to the top edge, and from this we infer
    * whether the major edge is on the left or the right.
    *
    * Serendipitously, this cross-product magnitude is also a
    * value we need to compute the iteration parameter
    * derivatives for the triangle, and it can be used to perform
    * backface culling because its sign tells us whether the
    * triangle is clockwise or counterclockwise.  In this code we
    * refer to it as ``area'' because it's also proportional to
    * the pixel area of the triangle.
    */

   {

      /*
       * Execute user-supplied setup code
       */
      SETUP_CODE

      ltor = (oneOverArea < 0.0F);

      /* compute d?/dx and d?/dy derivatives */
      {
         GLfloat eMaj_dz, eBot_dz;
         eMaj_dz = VB->Win[vMax][2] - VB->Win[vMin][2];
         eBot_dz = VB->Win[vMid][2] - VB->Win[vMin][2];
         dzdx = oneOverArea * (eMaj_dz * eBot.dy - eMaj.dy * eBot_dz);
         if (dzdx>DEPTH_SCALE || dzdx<-DEPTH_SCALE) {
            /* probably a sliver triangle */
            dzdx = 0.0;
            dzdy = 0.0;
         }
         else {
            dzdy = oneOverArea * (eMaj.dx * eBot_dz - eMaj_dz * eBot.dx);
         }
#if DEPTH_BITS==16
         fdzdx = SignedFloatToFixed(dzdx);
#else
         fdzdx = (GLint) dzdx;
#endif
      }
#if INTERP_RGB
      {
         GLfloat eMaj_dr, eBot_dr;
         eMaj_dr = (GLint) VB->Color[vMax][0] - (GLint) VB->Color[vMin][0];
         eBot_dr = (GLint) VB->Color[vMid][0] - (GLint) VB->Color[vMin][0];
         drdx = oneOverArea * (eMaj_dr * eBot.dy - eMaj.dy * eBot_dr);
         fdrdx = SignedFloatToFixed(drdx);
         drdy = oneOverArea * (eMaj.dx * eBot_dr - eMaj_dr * eBot.dx);
      }
      {
         GLfloat eMaj_dg, eBot_dg;
         eMaj_dg = (GLint) VB->Color[vMax][1] - (GLint) VB->Color[vMin][1];
	 eBot_dg = (GLint) VB->Color[vMid][1] - (GLint) VB->Color[vMin][1];
         dgdx = oneOverArea * (eMaj_dg * eBot.dy - eMaj.dy * eBot_dg);
         fdgdx = SignedFloatToFixed(dgdx);
         dgdy = oneOverArea * (eMaj.dx * eBot_dg - eMaj_dg * eBot.dx);
      }
      {
         GLfloat eMaj_db, eBot_db;
         eMaj_db = (GLint) VB->Color[vMax][2] - (GLint) VB->Color[vMin][2];
         eBot_db = (GLint) VB->Color[vMid][2] - (GLint) VB->Color[vMin][2];
         dbdx = oneOverArea * (eMaj_db * eBot.dy - eMaj.dy * eBot_db);
         fdbdx = SignedFloatToFixed(dbdx);
	 dbdy = oneOverArea * (eMaj.dx * eBot_db - eMaj_db * eBot.dx);
      }
#endif
#if INTERP_ALPHA
      {
         GLfloat eMaj_da, eBot_da;
         eMaj_da = (GLint) VB->Color[vMax][3] - (GLint) VB->Color[vMin][3];
         eBot_da = (GLint) VB->Color[vMid][3] - (GLint) VB->Color[vMin][3];
         dadx = oneOverArea * (eMaj_da * eBot.dy - eMaj.dy * eBot_da);
         fdadx = SignedFloatToFixed(dadx);
         dady = oneOverArea * (eMaj.dx * eBot_da - eMaj_da * eBot.dx);
      }
#endif
#if INTERP_INDEX
      {
         GLfloat eMaj_di, eBot_di;
         eMaj_di = (GLint) VB->Index[vMax] - (GLint) VB->Index[vMin];
         eBot_di = (GLint) VB->Index[vMid] - (GLint) VB->Index[vMin];
         didx = oneOverArea * (eMaj_di * eBot.dy - eMaj.dy * eBot_di);
         fdidx = SignedFloatToFixed(didx);
         didy = oneOverArea * (eMaj.dx * eBot_di - eMaj_di * eBot.dx);
      }
#endif
#if INTERP_ST
      {
         GLfloat eMaj_ds, eBot_ds;
         eMaj_ds = (VB->TexCoord[vMax][0] - VB->TexCoord[vMin][0]) * S_SCALE;
         eBot_ds = (VB->TexCoord[vMid][0] - VB->TexCoord[vMin][0]) * S_SCALE;
         dsdx = oneOverArea * (eMaj_ds * eBot.dy - eMaj.dy * eBot_ds);
         fdsdx = SignedFloatToFixed(dsdx);
         dsdy = oneOverArea * (eMaj.dx * eBot_ds - eMaj_ds * eBot.dx);
      }
      {
         GLfloat eMaj_dt, eBot_dt;
         eMaj_dt = (VB->TexCoord[vMax][1] - VB->TexCoord[vMin][1]) * T_SCALE;
         eBot_dt = (VB->TexCoord[vMid][1] - VB->TexCoord[vMin][1]) * T_SCALE;
         dtdx = oneOverArea * (eMaj_dt * eBot.dy - eMaj.dy * eBot_dt);
         fdtdx = SignedFloatToFixed(dtdx);
         dtdy = oneOverArea * (eMaj.dx * eBot_dt - eMaj_dt * eBot.dx);
      }
#endif
#if INTERP_STW
      {
         GLfloat wMax = 1.0F / VB->Clip[vMax][3];
         GLfloat wMin = 1.0F / VB->Clip[vMin][3];
         GLfloat wMid = 1.0F / VB->Clip[vMid][3];
         GLfloat eMaj_dw, eBot_dw;
         GLfloat eMaj_ds, eBot_ds;
         GLfloat eMaj_dt, eBot_dt;
#if INTERP_UV
         GLfloat eMaj_du, eBot_du;
         GLfloat eMaj_dv, eBot_dv;
#endif
         eMaj_dw = wMax - wMin;
         eBot_dw = wMid - wMin;
         dwdx = oneOverArea * (eMaj_dw * eBot.dy - eMaj.dy * eBot_dw);
         dwdy = oneOverArea * (eMaj.dx * eBot_dw - eMaj_dw * eBot.dx);

         eMaj_ds = VB->TexCoord[vMax][0]*wMax - VB->TexCoord[vMin][0]*wMin;
         eBot_ds = VB->TexCoord[vMid][0]*wMid - VB->TexCoord[vMin][0]*wMin;
         dsdx = oneOverArea * (eMaj_ds * eBot.dy - eMaj.dy * eBot_ds);
         dsdy = oneOverArea * (eMaj.dx * eBot_ds - eMaj_ds * eBot.dx);

         eMaj_dt = VB->TexCoord[vMax][1]*wMax - VB->TexCoord[vMin][1]*wMin;
         eBot_dt = VB->TexCoord[vMid][1]*wMid - VB->TexCoord[vMin][1]*wMin;
         dtdx = oneOverArea * (eMaj_dt * eBot.dy - eMaj.dy * eBot_dt);
         dtdy = oneOverArea * (eMaj.dx * eBot_dt - eMaj_dt * eBot.dx);
#if INTERP_UV
         eMaj_du = VB->TexCoord[vMax][2]*wMax - VB->TexCoord[vMin][2]*wMin;
         eBot_du = VB->TexCoord[vMid][2]*wMid - VB->TexCoord[vMin][2]*wMin;
         dudx = oneOverArea * (eMaj_du * eBot.dy - eMaj.dy * eBot_du);
         dudy = oneOverArea * (eMaj.dx * eBot_du - eMaj_du * eBot.dx);

         /* Note: don't divide V component by W */
         eMaj_dv = VB->TexCoord[vMax][3] - VB->TexCoord[vMin][3];
         eBot_dv = VB->TexCoord[vMid][3] - VB->TexCoord[vMin][3];
         dvdx = oneOverArea * (eMaj_dv * eBot.dy - eMaj.dy * eBot_dv);
         dvdy = oneOverArea * (eMaj.dx * eBot_dv - eMaj_dv * eBot.dx);
#endif
      }
#endif

      /*
       * We always sample at pixel centers.  However, we avoid
       * explicit half-pixel offsets in this code by incorporating
       * the proper offset in each of x and y during the
       * transformation to window coordinates.
       *
       * We also apply the usual rasterization rules to prevent
       * cracks and overlaps.  A pixel is considered inside a
       * subtriangle if it meets all of four conditions: it is on or
       * to the right of the left edge, strictly to the left of the
       * right edge, on or below the top edge, and strictly above
       * the bottom edge.  (Some edges may be degenerate.)
       *
       * The following discussion assumes left-to-right scanning
       * (that is, the major edge is on the left); the right-to-left
       * case is a straightforward variation.
       *
       * We start by finding the half-integral y coordinate that is
       * at or below the top of the triangle.  This gives us the
       * first scan line that could possibly contain pixels that are
       * inside the triangle.
       *
       * Next we creep down the major edge until we reach that y,
       * and compute the corresponding x coordinate on the edge. 
       * Then we find the half-integral x that lies on or just
       * inside the edge.  This is the first pixel that might lie in
       * the interior of the triangle.  (We won't know for sure
       * until we check the other edges.)
       *
       * As we rasterize the triangle, we'll step down the major
       * edge.  For each step in y, we'll move an integer number
       * of steps in x.  There are two possible x step sizes, which
       * we'll call the ``inner'' step (guaranteed to land on the
       * edge or inside it) and the ``outer'' step (guaranteed to
       * land on the edge or outside it).  The inner and outer steps
       * differ by one.  During rasterization we maintain an error
       * term that indicates our distance from the true edge, and
       * select either the inner step or the outer step, whichever
       * gets us to the first pixel that falls inside the triangle.
       *
       * All parameters (z, red, etc.) as well as the buffer
       * addresses for color and z have inner and outer step values,
       * so that we can increment them appropriately.  This method
       * eliminates the need to adjust parameters by creeping a
       * sub-pixel amount into the triangle at each scanline.
       */

      {
#ifdef PIXEL_ADDRESS
         PIXEL_TYPE *pRow;
         int dPRowOuter, dPRowInner;  /* offset in bytes */
#endif

#if INTERP_INDEX
         GLfixed fi, fdiOuter, fdiInner;
#endif
#if INTERP_ST
         GLfixed fs, fdsOuter, fdsInner;
         GLfixed ft, fdtOuter, fdtInner;
#endif
	 subTriangle=0;
	 int subTriangle2;
	 int lines2;
         for (subTriangle2=0; subTriangle2<=1; subTriangle2++) { //SUBTRIANGLE FOR...

		if (ltor) {  eLeft = &eMaj;   eRight = &eBot; lines = eRight->lines; lines2=lines; setupLeft = 1; setupRight = 1; }
               else { eLeft = &eBot; eRight = &eMaj; lines = eLeft->lines; lines2=lines; setupLeft = 1; setupRight = 1; }
            if (setupLeft && eLeft->lines>0) {
               GLint vLower;   GLfixed fsx = eLeft->fsx;  fx = FixedCeil(fsx);fError = fx - fsx - FIXED_ONE;
               fxLeftEdge = fsx - FIXED_EPSILON; fdxLeftEdge = eLeft->fdxdy; fdxOuter = FixedFloor(fdxLeftEdge - FIXED_EPSILON);
               fdError = fdxOuter - fdxLeftEdge + FIXED_ONE; idxOuter = FixedToInt(fdxOuter); dxOuter = (float) idxOuter;
               fy = eLeft->fsy; iy = FixedToInt(fy);  adjx = (float)(fx - eLeft->fx0);  /* SCALED! */
               adjy = eLeft->adjy;		 /* SCALED! */
               vLower = eLeft->v0;
#ifdef PIXEL_ADDRESS
               {  pRow = PIXEL_ADDRESS( FixedToInt(fxLeftEdge), iy );  dPRowOuter = -BYTES_PER_ROW + idxOuter * sizeof(PIXEL_TYPE); 
		/* negative because Y=0 at bottom and increases upward */  }
#endif
               /* Now we need the set of parameter (z, color, etc.) values at the point (fx, fy).  This gives us properly-sampled parameter
                * values that we can step from pixel to pixel.  Furthermore, although we might have intermediate results that overflow
                * the normal parameter range when we step temporarily outside the triangle, we shouldn't overflow or underflow for any
                * pixel that's actually inside the triangle.*/

#if INTERP_Z
               {
                  GLfloat z0, tmp;
                  z0 = VB->Win[vLower][2] + ctx->PolygonZoffset;
#if DEPTH_BITS==16
                  /* interpolate fixed-pt values */
                  tmp = (z0 * FIXED_SCALE + dzdx * adjx + dzdy * adjy) + FIXED_HALF;
                  if (tmp < MAX_GLUINT/2)
                     fz = (GLfixed) tmp;
                  else
                     fz = MAX_GLUINT/2;
                  fdzOuter = SignedFloatToFixed(dzdy + dxOuter * dzdx);
#else
                  /* interpolate depth values exactly */
                  fz = (GLint) (z0 + dzdx*FixedToFloat(adjx) + dzdy*FixedToFloat(adjy));
                  fdzOuter = (GLint) (dzdy + dxOuter * dzdx);
#endif
                  zRow = Z_ADDRESS( ctx, FixedToInt(fxLeftEdge), iy );
                  dZRowOuter = (ctx->Buffer->Width + idxOuter) * sizeof(GLdepth);
               }
#endif
#if INTERP_RGB
               fr = (GLfixed)(IntToFixed(VB->Color[vLower][0]) + drdx * adjx + drdy * adjy)
                    + FIXED_HALF;
               fdrOuter = SignedFloatToFixed(drdy + dxOuter * drdx);

               fg = (GLfixed)(IntToFixed(VB->Color[vLower][1]) + dgdx * adjx + dgdy * adjy)
                    + FIXED_HALF;
               fdgOuter = SignedFloatToFixed(dgdy + dxOuter * dgdx);

               fb = (GLfixed)(IntToFixed(VB->Color[vLower][2]) + dbdx * adjx + dbdy * adjy)
                    + FIXED_HALF;
               fdbOuter = SignedFloatToFixed(dbdy + dxOuter * dbdx);
#endif
#if INTERP_ALPHA
               fa = (GLfixed)(IntToFixed(VB->Color[vLower][3]) + dadx * adjx + dady * adjy)
                    + FIXED_HALF;
               fdaOuter = SignedFloatToFixed(dady + dxOuter * dadx);
#endif
#if INTERP_INDEX
               fi = (GLfixed)(VB->Index[vLower] * FIXED_SCALE + didx * adjx
                              + didy * adjy) + FIXED_HALF;
               fdiOuter = SignedFloatToFixed(didy + dxOuter * didx);
#endif
#if INTERP_ST
               {
                  GLfloat s0, t0;
                  s0 = VB->TexCoord[vLower][0] * S_SCALE;
                  fs = (GLfixed)(s0 * FIXED_SCALE + dsdx * adjx + dsdy * adjy) + FIXED_HALF;
                  fdsOuter = SignedFloatToFixed(dsdy + dxOuter * dsdx);
                  t0 = VB->TexCoord[vLower][1] * T_SCALE;
                  ft = (GLfixed)(t0 * FIXED_SCALE + dtdx * adjx + dtdy * adjy) + FIXED_HALF;
                  fdtOuter = SignedFloatToFixed(dtdy + dxOuter * dtdx);
               }
#endif
#if INTERP_STW
               {
                  GLfloat w0 = 1.0F / VB->Clip[vLower][3];
                  GLfloat s0, t0, u0, v0;
                  wLeft = w0 + (dwdx * adjx + dwdy * adjy) * (1.0F/FIXED_SCALE);
		  dwOuter = dwdy + dxOuter * dwdx;
                  s0 = VB->TexCoord[vLower][0] * w0;
                  sLeft = s0 + (dsdx * adjx + dsdy * adjy) * (1.0F/FIXED_SCALE);
                  dsOuter = dsdy + dxOuter * dsdx;
                  t0 = VB->TexCoord[vLower][1] * w0;
                  tLeft = t0 + (dtdx * adjx + dtdy * adjy) * (1.0F/FIXED_SCALE);
                  dtOuter = dtdy + dxOuter * dtdx;
#if INTERP_UV
                  u0 = VB->TexCoord[vLower][2] * w0;
                  uLeft = u0 + (dudx * adjx + dudy * adjy) * (1.0F/FIXED_SCALE);
                  duOuter = dudy + dxOuter * dudx;
                  /* Note: don't divide V component by W */
                  v0 = VB->TexCoord[vLower][3];
                  vLeft = v0 + (dvdx * adjx + dvdy * adjy) * (1.0F/FIXED_SCALE);
                  dvOuter = dvdy + dxOuter * dvdx;
#endif
               }
#endif

            } /*if setupLeft*/


            if (setupRight && eRight->lines2>0) {
               fxRightEdge = eRight->fsx - FIXED_EPSILON;
               fdxRightEdge = eRight->fdxdy;
            }

            if (lines2==0) {
               continue;
            }


            /* Rasterize setup */
#ifdef PIXEL_ADDRESS
            dPRowInner = dPRowOuter + sizeof(PIXEL_TYPE);
#endif
#if INTERP_Z
            dZRowInner = dZRowOuter + sizeof(GLdepth);
            fdzInner = fdzOuter + fdzdx;
#endif
#if INTERP_RGB
            fdrInner = fdrOuter + fdrdx;
            fdgInner = fdgOuter + fdgdx;
            fdbInner = fdbOuter + fdbdx;
#endif
#if INTERP_ALPHA
            fdaInner = fdaOuter + fdadx;
#endif
#if INTERP_STW
	    dwInner = dwOuter + dwdx;
	    dsInner = dsOuter + dsdx;
	    dtInner = dtOuter + dtdx;
#if INTERP_UV
	    duInner = duOuter + dudx;
	    dvInner = dvOuter + dvdx;
#endif
#endif

            while (lines2>0) {
               /* initialize the span interpolants to the leftmost value */
               /* ff = fixed-pt fragment */
#if INTERP_Z
               ffz = fz;
               /*GLdepth *zp = zRow;*/
#endif
#if INTERP_RGB
               ffr = fr;  ffg = fg;  ffb = fb;
#endif
#if INTERP_ALPHA
               ffa = fa;
#endif
#if INTERP_INDEX
               GLfixed ffi = fi;
#endif
#if INTERP_ST
               GLfixed ffs = fs,  fft = ft;
#endif
#if INTERP_STW
               ss = sLeft;  tt = tLeft;  ww = wLeft;
#endif
#if INTERP_UV
               uu = uLeft;  vv = vLeft;
#endif
               left = FixedToInt(fxLeftEdge);
               right = FixedToInt(fxRightEdge);

#if INTERP_RGB
               {
                  /* need this to accomodate round-off errors */
                  ffrend = ffr+(right-left-1)*fdrdx;
                  ffgend = ffg+(right-left-1)*fdgdx;
                  ffbend = ffb+(right-left-1)*fdbdx;
                  if (ffrend<0) ffr -= ffrend;
                  if (ffgend<0) ffg -= ffgend;
                  if (ffbend<0) ffb -= ffbend;
                  if (ffr<0) ffr = 0;
                  if (ffg<0) ffg = 0;
                  if (ffb<0) ffb = 0;
               }

#endif
#if INTERP_ALPHA
               {
                  ffaend = ffa+(right-left-1)*fdadx;
                  if (ffaend<0) ffa -= ffaend;
                  if (ffa<0) ffa = 0;
               }
#endif
#if INTERP_INDEX
               if (ffi<0) ffi = 0;
#endif

	       iy++;
	       lines--;
               INNER_LOOP( left, right, iy-1 ); //En inner loop es que me voy al otro, pero ese otro debería regresarme a acá... 
               lines2=-1;
	    } /*while lines>0*/
            subTriangle2=3;
         } /* for subTriangle */

      }
   }
}

#undef SETUP_CODE
#undef INNER_LOOP

/*#undef PIXEL_TYPE
#undef BYTES_PER_ROW
#undef PIXEL_ADDRESS

#undef INTERP_Z
#undef INTERP_RGB
#undef INTERP_ALPHA
#undef INTERP_INDEX
#undef INTERP_ST
#undef INTERP_STW
#undef INTERP_UV

#undef S_SCALE
#undef T_SCALE*/
}

/*NEW IT :::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::*/

/*
 * Render a smooth-shaded, textured, RGBA triangle.
 * Interpolate S,T,U with perspective correction, w/out mipmapping.
 * Note: we use texture coordinates S,T,U,V instead of S,T,R,Q because
 * R is already used for red.
 */

	//AQUI COMIENZO EJECUTANDO B, AL TERMINAR C EVALUO SI APPEND UNA IGUAL.
	//TERMINO LO QUE FALTÓ DE A, LO QUE PUEDE SIGNIFICAR: Tengo que utilizar ademas la señal it_finished para indicar si puede append o no en la ultima 		//it.
	// LLEGAR HASTA EL PUNTO DE LA SIGUIENTE INVOCACIÓN DE B EN LA MISMA ITERACION (QUE YA FUE APPENDED)
	// TERMINAR LA ITERACIÓN, INVOCANDO AL END_PROFILE Y COMENZAR CON UNA NUEVA INVOCANDO AL START PROFILE, HASTA EL PUNTO DE B QUE YA FUE APPENDED
	// TERMINAR LA ITERACION Y TERMINAR EL MODELO (NADA FUE APPENDED)

static void new_it_chunk ( GLcontext *ctx, GLuint v0, GLuint v1,
                                       GLuint v2, GLuint pv )
{

START_PROFILE

/*#define INTERP_Z 1
#define INTERP_RGB 1
#define INTERP_ALPHA 1
#define INTERP_STW 1
#define INTERP_UV 1*/
#define SETUP_CODE						\
   GLboolean flat_shade = (ctx->Light.ShadeModel==GL_FLAT);	\
   GLint r, g, b, a;						\
   if (flat_shade) {						\
      r = VB->Color[pv][0];					\
      g = VB->Color[pv][1];					\
      b = VB->Color[pv][2];					\
      a = VB->Color[pv][3];					\
   }
#define INNER_LOOP( LEFT, RIGHT, Y )				\
	{							\
	   GLint i, n = RIGHT-LEFT;				\
	   if (n>0) {						\
              if (flat_shade) {					\
                 for (i=0;i<n;i++) {				\
		    GLdouble wwvvInv = 1.0 / (ww*vv);		\
		    zspan[i] = FixedToDepth(ffz);		\
		    red[i]   = r;				\
		    green[i] = g;				\
		    blue[i]  = b;				\
		    alpha[i] = a;				\
		    s[i] = ss*wwvvInv;				\
		    t[i] = tt*wwvvInv;				\
		    u[i] = uu*wwvvInv;				\
		    ffz += fdzdx;				\
		    ss += dsdx;					\
		    tt += dtdx;					\
		    uu += dudx;					\
		    vv += dvdx;					\
		    ww += dwdx;					\
		 }						\
              }							\
              else {						\
                 for (i=0;i<n;i++) {				\
		    GLdouble wwvvInv = 1.0 / (ww*vv);		\
		    zspan[i] = FixedToDepth(ffz);		\
		    red[i]   = FixedToInt(ffr);			\
		    green[i] = FixedToInt(ffg);			\
		    blue[i]  = FixedToInt(ffb);			\
		    alpha[i] = FixedToInt(ffa);			\
		    s[i] = ss*wwvvInv;				\
		    t[i] = tt*wwvvInv;				\
		    u[i] = uu*wwvvInv;				\
		    ffz += fdzdx;				\
		    ffr += fdrdx;				\
		    ffg += fdgdx;				\
		    ffb += fdbdx;				\
		    ffa += fdadx;				\
		    ss += dsdx;					\
		    tt += dtdx;					\
		    uu += dudx;					\
		    ww += dwdx;					\
		    vv += dvdx;					\
		 }						\
              }							\
	   }							\
	}

/*void triangle( GLcontext *ctx, GLuint v0, GLuint v1, GLuint v2, GLuint pv )*/
{

   /* find the order of the 3 vertices along the Y axis */
   {
      y0 = VB->Win[v0][1];
      y1 = VB->Win[v1][1];
      y2 = VB->Win[v2][1];

      if (y0<=y1) {
	 if (y1<=y2) {
	    vMin = v0;   vMid = v1;   vMax = v2;   /* y0<=y1<=y2 */
	 }
	 else if (y2<=y0) {
	    vMin = v2;   vMid = v0;   vMax = v1;   /* y2<=y0<=y1 */
	 }
	 else {
	    vMin = v0;   vMid = v2;   vMax = v1;   /* y0<=y2<=y1 */
	 }
      }
      else {
	 if (y0<=y2) {
	    vMin = v1;   vMid = v0;   vMax = v2;   /* y1<=y0<=y2 */
	 }
	 else if (y2<=y1) {
	    vMin = v2;   vMid = v1;   vMax = v0;   /* y2<=y1<=y0 */
	 }
	 else {
	    vMin = v1;   vMid = v2;   vMax = v0;   /* y1<=y2<=y0 */
	 }
      }
   }

   /* vertex/edge relationship */
   eMaj.v0 = vMin;   eMaj.v1 = vMax;   /*TODO: .v1's not needed */
   eTop.v0 = vMid;   eTop.v1 = vMax;
   eBot.v0 = vMin;   eBot.v1 = vMid;

   /* compute deltas for each edge:  vertex[v1] - vertex[v0] */
   eMaj.dx = VB->Win[vMax][0] - VB->Win[vMin][0];
   eMaj.dy = VB->Win[vMax][1] - VB->Win[vMin][1];
   eTop.dx = VB->Win[vMax][0] - VB->Win[vMid][0];
   eTop.dy = VB->Win[vMax][1] - VB->Win[vMid][1];
   eBot.dx = VB->Win[vMid][0] - VB->Win[vMin][0];
   eBot.dy = VB->Win[vMid][1] - VB->Win[vMin][1];

   /* compute oneOverArea */
   {
      area = eMaj.dx * eBot.dy - eBot.dx * eMaj.dy;
      if (area>-0.05f && area<0.05f) {
         return;  /* very small; CULLED */
      }
      oneOverArea = 1.0F / area;
   }

   /* Edge setup.  For a triangle strip these could be reused... */
   {
      /* fixed point Y coordinates */
      vMin_fx = FloatToFixed(VB->Win[vMin][0] + 0.5F);
      vMin_fy = FloatToFixed(VB->Win[vMin][1] - 0.5F);
      vMid_fx = FloatToFixed(VB->Win[vMid][0] + 0.5F);
      vMid_fy = FloatToFixed(VB->Win[vMid][1] - 0.5F);
      vMax_fy = FloatToFixed(VB->Win[vMax][1] - 0.5F);

      eMaj.fsy = FixedCeil(vMin_fy);
      eMaj.lines = FixedToInt(vMax_fy + FIXED_ONE - FIXED_EPSILON - eMaj.fsy);
      if (eMaj.lines > 0) {
         GLfloat dxdy = eMaj.dx / eMaj.dy;
         eMaj.fdxdy = SignedFloatToFixed(dxdy);
         eMaj.adjy = (GLfloat) (eMaj.fsy - vMin_fy);  /* SCALED! */
         eMaj.fx0 = vMin_fx;
         eMaj.fsx = eMaj.fx0 + (GLfixed) (eMaj.adjy * dxdy);
      }
      else {
         return;  /*CULLED*/
      }

      eTop.fsy = FixedCeil(vMid_fy);
      eTop.lines = FixedToInt(vMax_fy + FIXED_ONE - FIXED_EPSILON - eTop.fsy);
      if (eTop.lines > 0) {
         GLfloat dxdy = eTop.dx / eTop.dy;
         eTop.fdxdy = SignedFloatToFixed(dxdy);
         eTop.adjy = (GLfloat) (eTop.fsy - vMid_fy); /* SCALED! */
         eTop.fx0 = vMid_fx;
         eTop.fsx = eTop.fx0 + (GLfixed) (eTop.adjy * dxdy);
      }

      eBot.fsy = FixedCeil(vMin_fy);
      eBot.lines = FixedToInt(vMid_fy + FIXED_ONE - FIXED_EPSILON - eBot.fsy);
      if (eBot.lines > 0) {
         GLfloat dxdy = eBot.dx / eBot.dy;
         eBot.fdxdy = SignedFloatToFixed(dxdy);
         eBot.adjy = (GLfloat) (eBot.fsy - vMin_fy);  /* SCALED! */
         eBot.fx0 = vMin_fx;
         eBot.fsx = eBot.fx0 + (GLfixed) (eBot.adjy * dxdy);
      }
   }

   /*
    * Conceptually, we view a triangle as two subtriangles
    * separated by a perfectly horizontal line.  The edge that is
    * intersected by this line is one with maximal absolute dy; we
    * call it a ``major'' edge.  The other two edges are the
    * ``top'' edge (for the upper subtriangle) and the ``bottom''
    * edge (for the lower subtriangle).  If either of these two
    * edges is horizontal or very close to horizontal, the
    * corresponding subtriangle might cover zero sample points;
    * we take care to handle such cases, for performance as well
    * as correctness.
    *
    * By stepping rasterization parameters along the major edge,
    * we can avoid recomputing them at the discontinuity where
    * the top and bottom edges meet.  However, this forces us to
    * be able to scan both left-to-right and right-to-left. 
    * Also, we must determine whether the major edge is at the
    * left or right side of the triangle.  We do this by
    * computing the magnitude of the cross-product of the major
    * and top edges.  Since this magnitude depends on the sine of
    * the angle between the two edges, its sign tells us whether
    * we turn to the left or to the right when travelling along
    * the major edge to the top edge, and from this we infer
    * whether the major edge is on the left or the right.
    *
    * Serendipitously, this cross-product magnitude is also a
    * value we need to compute the iteration parameter
    * derivatives for the triangle, and it can be used to perform
    * backface culling because its sign tells us whether the
    * triangle is clockwise or counterclockwise.  In this code we
    * refer to it as ``area'' because it's also proportional to
    * the pixel area of the triangle.
    */

   {
     
#ifdef SETUP_CODE
      SETUP_CODE
#endif

      ltor = (oneOverArea < 0.0F);

      /* compute d?/dx and d?/dy derivatives */
      {
         GLfloat eMaj_dz, eBot_dz;
         eMaj_dz = VB->Win[vMax][2] - VB->Win[vMin][2];
         eBot_dz = VB->Win[vMid][2] - VB->Win[vMin][2];
         dzdx = oneOverArea * (eMaj_dz * eBot.dy - eMaj.dy * eBot_dz);
         if (dzdx>DEPTH_SCALE || dzdx<-DEPTH_SCALE) {
            /* probably a sliver triangle */
            dzdx = 0.0;
            dzdy = 0.0;
         }
         else {
            dzdy = oneOverArea * (eMaj.dx * eBot_dz - eMaj_dz * eBot.dx);
         }
#if DEPTH_BITS==16
         fdzdx = SignedFloatToFixed(dzdx);
#else
         fdzdx = (GLint) dzdx;
#endif
      }

#if INTERP_RGB
      {
         GLfloat eMaj_dr, eBot_dr;
         eMaj_dr = (GLint) VB->Color[vMax][0] - (GLint) VB->Color[vMin][0];
         eBot_dr = (GLint) VB->Color[vMid][0] - (GLint) VB->Color[vMin][0];
         drdx = oneOverArea * (eMaj_dr * eBot.dy - eMaj.dy * eBot_dr);
         fdrdx = SignedFloatToFixed(drdx);
         drdy = oneOverArea * (eMaj.dx * eBot_dr - eMaj_dr * eBot.dx);
      }
      {
         GLfloat eMaj_dg, eBot_dg;
         eMaj_dg = (GLint) VB->Color[vMax][1] - (GLint) VB->Color[vMin][1];
	 eBot_dg = (GLint) VB->Color[vMid][1] - (GLint) VB->Color[vMin][1];
         dgdx = oneOverArea * (eMaj_dg * eBot.dy - eMaj.dy * eBot_dg);
         fdgdx = SignedFloatToFixed(dgdx);
         dgdy = oneOverArea * (eMaj.dx * eBot_dg - eMaj_dg * eBot.dx);
      }
      {
         GLfloat eMaj_db, eBot_db;
         eMaj_db = (GLint) VB->Color[vMax][2] - (GLint) VB->Color[vMin][2];
         eBot_db = (GLint) VB->Color[vMid][2] - (GLint) VB->Color[vMin][2];
         dbdx = oneOverArea * (eMaj_db * eBot.dy - eMaj.dy * eBot_db);
         fdbdx = SignedFloatToFixed(dbdx);
	 dbdy = oneOverArea * (eMaj.dx * eBot_db - eMaj_db * eBot.dx);
      }
#endif
#if INTERP_ALPHA
      {
         GLfloat eMaj_da, eBot_da;
         eMaj_da = (GLint) VB->Color[vMax][3] - (GLint) VB->Color[vMin][3];
         eBot_da = (GLint) VB->Color[vMid][3] - (GLint) VB->Color[vMin][3];
         dadx = oneOverArea * (eMaj_da * eBot.dy - eMaj.dy * eBot_da);
         fdadx = SignedFloatToFixed(dadx);
         dady = oneOverArea * (eMaj.dx * eBot_da - eMaj_da * eBot.dx);
      }
#endif
#if INTERP_INDEX
      {
         GLfloat eMaj_di, eBot_di;
         eMaj_di = (GLint) VB->Index[vMax] - (GLint) VB->Index[vMin];
         eBot_di = (GLint) VB->Index[vMid] - (GLint) VB->Index[vMin];
         didx = oneOverArea * (eMaj_di * eBot.dy - eMaj.dy * eBot_di);
         fdidx = SignedFloatToFixed(didx);
         didy = oneOverArea * (eMaj.dx * eBot_di - eMaj_di * eBot.dx);
      }
#endif
#if INTERP_ST
      {
         GLfloat eMaj_ds, eBot_ds;
         eMaj_ds = (VB->TexCoord[vMax][0] - VB->TexCoord[vMin][0]) * S_SCALE;
         eBot_ds = (VB->TexCoord[vMid][0] - VB->TexCoord[vMin][0]) * S_SCALE;
         dsdx = oneOverArea * (eMaj_ds * eBot.dy - eMaj.dy * eBot_ds);
         fdsdx = SignedFloatToFixed(dsdx);
         dsdy = oneOverArea * (eMaj.dx * eBot_ds - eMaj_ds * eBot.dx);
      }
      {
         GLfloat eMaj_dt, eBot_dt;
         eMaj_dt = (VB->TexCoord[vMax][1] - VB->TexCoord[vMin][1]) * T_SCALE;
         eBot_dt = (VB->TexCoord[vMid][1] - VB->TexCoord[vMin][1]) * T_SCALE;
         dtdx = oneOverArea * (eMaj_dt * eBot.dy - eMaj.dy * eBot_dt);
         fdtdx = SignedFloatToFixed(dtdx);
         dtdy = oneOverArea * (eMaj.dx * eBot_dt - eMaj_dt * eBot.dx);
      }
#endif
#if INTERP_STW
      {
         GLfloat wMax = 1.0F / VB->Clip[vMax][3];
         GLfloat wMin = 1.0F / VB->Clip[vMin][3];
         GLfloat wMid = 1.0F / VB->Clip[vMid][3];
         GLfloat eMaj_dw, eBot_dw;
         GLfloat eMaj_ds, eBot_ds;
         GLfloat eMaj_dt, eBot_dt;
#if INTERP_UV
         GLfloat eMaj_du, eBot_du;
         GLfloat eMaj_dv, eBot_dv;
#endif
         eMaj_dw = wMax - wMin;
         eBot_dw = wMid - wMin;
         dwdx = oneOverArea * (eMaj_dw * eBot.dy - eMaj.dy * eBot_dw);
         dwdy = oneOverArea * (eMaj.dx * eBot_dw - eMaj_dw * eBot.dx);

         eMaj_ds = VB->TexCoord[vMax][0]*wMax - VB->TexCoord[vMin][0]*wMin;
         eBot_ds = VB->TexCoord[vMid][0]*wMid - VB->TexCoord[vMin][0]*wMin;
         dsdx = oneOverArea * (eMaj_ds * eBot.dy - eMaj.dy * eBot_ds);
         dsdy = oneOverArea * (eMaj.dx * eBot_ds - eMaj_ds * eBot.dx);

         eMaj_dt = VB->TexCoord[vMax][1]*wMax - VB->TexCoord[vMin][1]*wMin;
         eBot_dt = VB->TexCoord[vMid][1]*wMid - VB->TexCoord[vMin][1]*wMin;
         dtdx = oneOverArea * (eMaj_dt * eBot.dy - eMaj.dy * eBot_dt);
         dtdy = oneOverArea * (eMaj.dx * eBot_dt - eMaj_dt * eBot.dx);
#if INTERP_UV
         eMaj_du = VB->TexCoord[vMax][2]*wMax - VB->TexCoord[vMin][2]*wMin;
         eBot_du = VB->TexCoord[vMid][2]*wMid - VB->TexCoord[vMin][2]*wMin;
         dudx = oneOverArea * (eMaj_du * eBot.dy - eMaj.dy * eBot_du);
         dudy = oneOverArea * (eMaj.dx * eBot_du - eMaj_du * eBot.dx);

         /* Note: don't divide V component by W */
         eMaj_dv = VB->TexCoord[vMax][3] - VB->TexCoord[vMin][3];
         eBot_dv = VB->TexCoord[vMid][3] - VB->TexCoord[vMin][3];
         dvdx = oneOverArea * (eMaj_dv * eBot.dy - eMaj.dy * eBot_dv);
         dvdy = oneOverArea * (eMaj.dx * eBot_dv - eMaj_dv * eBot.dx);
#endif
      }
#endif
      /*
       * We always sample at pixel centers.  However, we avoid
       * explicit half-pixel offsets in this code by incorporating
       * the proper offset in each of x and y during the
       * transformation to window coordinates.
       *
       * We also apply the usual rasterization rules to prevent
       * cracks and overlaps.  A pixel is considered inside a
       * subtriangle if it meets all of four conditions: it is on or
       * to the right of the left edge, strictly to the left of the
       * right edge, on or below the top edge, and strictly above
       * the bottom edge.  (Some edges may be degenerate.)
       *
       * The following discussion assumes left-to-right scanning
       * (that is, the major edge is on the left); the right-to-left
       * case is a straightforward variation.
       *
       * We start by finding the half-integral y coordinate that is
       * at or below the top of the triangle.  This gives us the
       * first scan line that could possibly contain pixels that are
       * inside the triangle.
       *
       * Next we creep down the major edge until we reach that y,
       * and compute the corresponding x coordinate on the edge. 
       * Then we find the half-integral x that lies on or just
       * inside the edge.  This is the first pixel that might lie in
       * the interior of the triangle.  (We won't know for sure
       * until we check the other edges.)
       *
       * As we rasterize the triangle, we'll step down the major
       * edge.  For each step in y, we'll move an integer number
       * of steps in x.  There are two possible x step sizes, which
       * we'll call the ``inner'' step (guaranteed to land on the
       * edge or inside it) and the ``outer'' step (guaranteed to
       * land on the edge or outside it).  The inner and outer steps
       * differ by one.  During rasterization we maintain an error
       * term that indicates our distance from the true edge, and
       * select either the inner step or the outer step, whichever
       * gets us to the first pixel that falls inside the triangle.
       *
       * All parameters (z, red, etc.) as well as the buffer
       * addresses for color and z have inner and outer step values,
       * so that we can increment them appropriately.  This method
       * eliminates the need to adjust parameters by creeping a
       * sub-pixel amount into the triangle at each scanline.
       */

      {
#ifdef PIXEL_ADDRESS
         PIXEL_TYPE *pRow;
         int dPRowOuter, dPRowInner;  /* offset in bytes */
#endif
#if INTERP_INDEX
         GLfixed fi, fdiOuter, fdiInner;
#endif
#if INTERP_ST
         GLfixed fs, fdsOuter, fdsInner;
         GLfixed ft, fdtOuter, fdtInner;
#endif
	 subTriangle=0;
	 for (int subTriangle2=0; subTriangle2<=1; subTriangle2++) { //SUBTRIANGLE FOR...

            if (subTriangle2==0) {
               /* bottom half */
               if (ltor) {
                  eLeft = &eMaj;
                  eRight = &eBot;
                  lines = eRight->lines;
		  lines2=lines;
                  setupLeft = 1;
                  setupRight = 1;
               }
               else {
                  eLeft = &eBot;
                  eRight = &eMaj;
                  lines = eLeft->lines;
	          lines2=lines;
                  setupLeft = 1;
                  setupRight = 1;
               }
            }

            if (setupLeft && eLeft->lines>0) {
               GLint vLower;
               GLfixed fsx = eLeft->fsx;
               fx = FixedCeil(fsx);
               fError = fx - fsx - FIXED_ONE;
               fxLeftEdge = fsx - FIXED_EPSILON;
               fdxLeftEdge = eLeft->fdxdy;
               fdxOuter = FixedFloor(fdxLeftEdge - FIXED_EPSILON);
               fdError = fdxOuter - fdxLeftEdge + FIXED_ONE;
               idxOuter = FixedToInt(fdxOuter);
               dxOuter = (float) idxOuter;

               fy = eLeft->fsy;
               iy = FixedToInt(fy);

               adjx = (float)(fx - eLeft->fx0);  /* SCALED! */
               adjy = eLeft->adjy;		 /* SCALED! */

               vLower = eLeft->v0;

#ifdef PIXEL_ADDRESS
               {
                  pRow = PIXEL_ADDRESS( FixedToInt(fxLeftEdge), iy );
                  dPRowOuter = -BYTES_PER_ROW + idxOuter * sizeof(PIXEL_TYPE);
                  /* negative because Y=0 at bottom and increases upward */
               }
#endif
               /*
                * Now we need the set of parameter (z, color, etc.) values at
                * the point (fx, fy).  This gives us properly-sampled parameter
                * values that we can step from pixel to pixel.  Furthermore,
                * although we might have intermediate results that overflow
                * the normal parameter range when we step temporarily outside
                * the triangle, we shouldn't overflow or underflow for any
                * pixel that's actually inside the triangle.
                */

#if INTERP_Z
               {
                  GLfloat z0, tmp;
                  z0 = VB->Win[vLower][2] + ctx->PolygonZoffset;
#if DEPTH_BITS==16
                  /* interpolate fixed-pt values */
                  tmp = (z0 * FIXED_SCALE + dzdx * adjx + dzdy * adjy) + FIXED_HALF;
                  if (tmp < MAX_GLUINT/2)
                     fz = (GLfixed) tmp;
                  else
                     fz = MAX_GLUINT/2;
                  fdzOuter = SignedFloatToFixed(dzdy + dxOuter * dzdx);
#else
                  /* interpolate depth values exactly */
                  fz = (GLint) (z0 + dzdx*FixedToFloat(adjx) + dzdy*FixedToFloat(adjy));
                  fdzOuter = (GLint) (dzdy + dxOuter * dzdx);
#endif
                  zRow = Z_ADDRESS( ctx, FixedToInt(fxLeftEdge), iy );
                  dZRowOuter = (ctx->Buffer->Width + idxOuter) * sizeof(GLdepth);
               }
#endif
#if INTERP_RGB
               fr = (GLfixed)(IntToFixed(VB->Color[vLower][0]) + drdx * adjx + drdy * adjy)
                    + FIXED_HALF;
               fdrOuter = SignedFloatToFixed(drdy + dxOuter * drdx);

               fg = (GLfixed)(IntToFixed(VB->Color[vLower][1]) + dgdx * adjx + dgdy * adjy)
                    + FIXED_HALF;
               fdgOuter = SignedFloatToFixed(dgdy + dxOuter * dgdx);

               fb = (GLfixed)(IntToFixed(VB->Color[vLower][2]) + dbdx * adjx + dbdy * adjy)
                    + FIXED_HALF;
               fdbOuter = SignedFloatToFixed(dbdy + dxOuter * dbdx);
#endif
#if INTERP_ALPHA
               fa = (GLfixed)(IntToFixed(VB->Color[vLower][3]) + dadx * adjx + dady * adjy)
                    + FIXED_HALF;
               fdaOuter = SignedFloatToFixed(dady + dxOuter * dadx);
#endif
#if INTERP_INDEX
               fi = (GLfixed)(VB->Index[vLower] * FIXED_SCALE + didx * adjx
                              + didy * adjy) + FIXED_HALF;
               fdiOuter = SignedFloatToFixed(didy + dxOuter * didx);
#endif
#if INTERP_ST
               {
                  GLfloat s0, t0;
                  s0 = VB->TexCoord[vLower][0] * S_SCALE;
                  fs = (GLfixed)(s0 * FIXED_SCALE + dsdx * adjx + dsdy * adjy) + FIXED_HALF;
                  fdsOuter = SignedFloatToFixed(dsdy + dxOuter * dsdx);
                  t0 = VB->TexCoord[vLower][1] * T_SCALE;
                  ft = (GLfixed)(t0 * FIXED_SCALE + dtdx * adjx + dtdy * adjy) + FIXED_HALF;
                  fdtOuter = SignedFloatToFixed(dtdy + dxOuter * dtdx);
               }
#endif
#if INTERP_STW
               {
                  GLfloat w0 = 1.0F / VB->Clip[vLower][3];
                  GLfloat s0, t0, u0, v0;
                  wLeft = w0 + (dwdx * adjx + dwdy * adjy) * (1.0F/FIXED_SCALE);
		  dwOuter = dwdy + dxOuter * dwdx;
                  s0 = VB->TexCoord[vLower][0] * w0;
                  sLeft = s0 + (dsdx * adjx + dsdy * adjy) * (1.0F/FIXED_SCALE);
                  dsOuter = dsdy + dxOuter * dsdx;
                  t0 = VB->TexCoord[vLower][1] * w0;
                  tLeft = t0 + (dtdx * adjx + dtdy * adjy) * (1.0F/FIXED_SCALE);
                  dtOuter = dtdy + dxOuter * dtdx;
#if INTERP_UV
                  u0 = VB->TexCoord[vLower][2] * w0;
                  uLeft = u0 + (dudx * adjx + dudy * adjy) * (1.0F/FIXED_SCALE);
                  duOuter = dudy + dxOuter * dudx;
                  /* Note: don't divide V component by W */
                  v0 = VB->TexCoord[vLower][3];
                  vLeft = v0 + (dvdx * adjx + dvdy * adjy) * (1.0F/FIXED_SCALE);
                  dvOuter = dvdy + dxOuter * dvdx;
#endif
               }
#endif

            } /*if setupLeft*/


            if (setupRight && eRight->lines>0) {
               fxRightEdge = eRight->fsx - FIXED_EPSILON;
               fdxRightEdge = eRight->fdxdy;
            }

            if (lines==0) {
               continue;
            }


            /* Rasterize setup */
#ifdef PIXEL_ADDRESS
            dPRowInner = dPRowOuter + sizeof(PIXEL_TYPE);
#endif
#if INTERP_Z
            dZRowInner = dZRowOuter + sizeof(GLdepth);
            fdzInner = fdzOuter + fdzdx;
#endif
#if INTERP_RGB
            fdrInner = fdrOuter + fdrdx;
            fdgInner = fdgOuter + fdgdx;
            fdbInner = fdbOuter + fdbdx;
#endif
#if INTERP_ALPHA
            fdaInner = fdaOuter + fdadx;
#endif
#if INTERP_INDEX
            fdiInner = fdiOuter + fdidx;
#endif
#if INTERP_ST
            fdsInner = fdsOuter + fdsdx;
            fdtInner = fdtOuter + fdtdx;
#endif
#if INTERP_STW
	    dwInner = dwOuter + dwdx;
	    dsInner = dsOuter + dsdx;
	    dtInner = dtOuter + dtdx;
#if INTERP_UV
	    duInner = duOuter + dudx;
	    dvInner = dvOuter + dvdx;
#endif
#endif

            while (lines2>0) {
               /* initialize the span interpolants to the leftmost value */
               /* ff = fixed-pt fragment */
#if INTERP_Z
               ffz = fz;
               /*GLdepth *zp = zRow;*/
#endif
#if INTERP_RGB
               ffr = fr;  ffg = fg;  ffb = fb;
#endif
#if INTERP_ALPHA
               ffa = fa;
#endif
#if INTERP_INDEX
               GLfixed ffi = fi;
#endif
#if INTERP_ST
               ffs = fs,  fft = ft;
#endif
#if INTERP_STW
               ss = sLeft;  tt = tLeft;  ww = wLeft;
#endif
#if INTERP_UV
		uu = uLeft;  vv = vLeft;
#endif
               left = FixedToInt(fxLeftEdge);
               right = FixedToInt(fxRightEdge);

#if INTERP_RGB
               {
                  /* need this to accomodate round-off errors */
                  GLfixed ffrend = ffr+(right-left-1)*fdrdx;
                  GLfixed ffgend = ffg+(right-left-1)*fdgdx;
                  GLfixed ffbend = ffb+(right-left-1)*fdbdx;
                  if (ffrend<0) ffr -= ffrend;
                  if (ffgend<0) ffg -= ffgend;
                  if (ffbend<0) ffb -= ffbend;
                  if (ffr<0) ffr = 0;
                  if (ffg<0) ffg = 0;
                  if (ffb<0) ffb = 0;
               }
#endif
#if INTERP_ALPHA
               {
                  GLfixed ffaend = ffa+(right-left-1)*fdadx;
                  if (ffaend<0) ffa -= ffaend;
                  if (ffa<0) ffa = 0;
               }
#endif
#if INTERP_INDEX
               if (ffi<0) ffi = 0;
#endif

	       iy++;
	       lines--;
               INNER_LOOP( left, right, iy-1 ); //En inner loop es que me voy al otro, pero ese otro debería regresarme a acá... 
               lines2=-1;

            } /*while lines>0*/
         subTriangle2=3;
         } /* for subTriangle */
      }
   }
}

#undef SETUP_CODE
#undef INNER_LOOP

/*#undef PIXEL_TYPE
#undef BYTES_PER_ROW
#undef PIXEL_ADDRESS

#undef INTERP_Z
#undef INTERP_RGB
#undef INTERP_ALPHA
#undef INTERP_INDEX
#undef INTERP_ST
#undef INTERP_STW
#undef INTERP_UV

#undef S_SCALE
#undef T_SCALE*/
}


/*REGULAR CHUNKS------------------------------------------------------------------------------*/

/*
 * Render a smooth-shaded, textured, RGBA triangle.
 * Interpolate S,T,U with perspective correction, w/out mipmapping.
 * Note: we use texture coordinates S,T,U,V instead of S,T,R,Q because
 * R is already used for red.
 */

	//AQUI COMIENZO EJECUTANDO B, AL TERMINAR C EVALUO SI APPEND UNA IGUAL.
	//TERMINO LO QUE FALTÓ DE A, LO QUE PUEDE SIGNIFICAR: Tengo que utilizar ademas la señal it_finished para indicar si puede append o no en la ultima 		//it.
	// LLEGAR HASTA EL PUNTO DE LA SIGUIENTE INVOCACIÓN DE B EN LA MISMA ITERACION (QUE YA FUE APPENDED)
	// TERMINAR LA ITERACIÓN, INVOCANDO AL END_PROFILE Y COMENZAR CON UNA NUEVA INVOCANDO AL START PROFILE, HASTA EL PUNTO DE B QUE YA FUE APPENDED
	// TERMINAR LA ITERACION Y TERMINAR EL MODELO (NADA FUE APPENDED)

static void parallel_general_textured_triangle( GLcontext *ctx, GLuint v0, GLuint v1,
                                       GLuint v2, GLuint pv )
{

/*#define INTERP_Z 1
#define INTERP_RGB 1
#define INTERP_ALPHA 1
#define INTERP_STW 1
#define INTERP_UV 1*/
#define INNER_LOOP( LEFT, RIGHT, Y )				\
	{							\
	   GLint i, n = RIGHT-LEFT;				\
	   if (n>0) {						\
              if (flat_shade) {					\
                 for (i=0;i<n;i++) {				\
		    GLdouble wwvvInv = 1.0 / (ww*vv);		\
		    zspan[i] = FixedToDepth(ffz);		\
		    red[i]   = r;				\
		    green[i] = g;				\
		    blue[i]  = b;				\
		    alpha[i] = a;				\
		    s[i] = ss*wwvvInv;				\
		    t[i] = tt*wwvvInv;				\
		    u[i] = uu*wwvvInv;				\
		    ffz += fdzdx;				\
		    ss += dsdx;					\
		    tt += dtdx;					\
		    uu += dudx;					\
		    vv += dvdx;					\
		    ww += dwdx;					\
		 }						\
              }							\
              else {						\
                 for (i=0;i<n;i++) {				\
		    GLdouble wwvvInv = 1.0 / (ww*vv);		\
		    zspan[i] = FixedToDepth(ffz);		\
		    red[i]   = FixedToInt(ffr);			\
		    green[i] = FixedToInt(ffg);			\
		    blue[i]  = FixedToInt(ffb);			\
		    alpha[i] = FixedToInt(ffa);			\
		    s[i] = ss*wwvvInv;				\
		    t[i] = tt*wwvvInv;				\
		    u[i] = uu*wwvvInv;				\
		    ffz += fdzdx;				\
		    ffr += fdrdx;				\
		    ffg += fdgdx;				\
		    ffb += fdbdx;				\
		    ffa += fdadx;				\
		    ss += dsdx;					\
		    tt += dtdx;					\
		    uu += dudx;					\
		    ww += dwdx;					\
		    vv += dvdx;					\
		 }						\
              }							\
	   }							\
	}
      {

      GLdepth zspan[MAX_WIDTH];				\
      parallel_gl_write_texture_span( global_ctx, right-left, left, iy-1, zspan, 
                                     s, t, u, NULL, red, green, blue, alpha,	\
				     GL_POLYGON );		\
         for (subTriangle=0; subTriangle<=1; subTriangle++) { //SUBTRIANGLE FOR...
            EdgeT *eLeft, *eRight;
            int setupLeft, setupRight;
            int lines;

            if (subTriangle==0) {
               /* bottom half */
               if (ltor) {
                  eLeft = &eMaj;
                  eRight = &eBot;
                  lines = eRight->lines;
                  setupLeft = 1;
                  setupRight = 1;
               }
               else {
                  eLeft = &eBot;
                  eRight = &eMaj;
                  lines = eLeft->lines;
                  setupLeft = 1;
                  setupRight = 1;
               }
            }
            else {
               /* top half */
               if (ltor) {
                  eLeft = &eMaj;
                  eRight = &eTop;
                  lines = eRight->lines;
                  setupLeft = 0;
                  setupRight = 1;
               }
               else {
                  eLeft = &eTop;
                  eRight = &eMaj;
                  lines = eLeft->lines;
                  setupLeft = 1;
                  setupRight = 0;
               }
               if (lines==0) return;
            }

            if (setupLeft && eLeft->lines>0) {
               GLint vLower;
               GLfixed fsx = eLeft->fsx;
               fx = FixedCeil(fsx);
               fError = fx - fsx - FIXED_ONE;
               fxLeftEdge = fsx - FIXED_EPSILON;
               fdxLeftEdge = eLeft->fdxdy;
               fdxOuter = FixedFloor(fdxLeftEdge - FIXED_EPSILON);
               fdError = fdxOuter - fdxLeftEdge + FIXED_ONE;
               idxOuter = FixedToInt(fdxOuter);
               dxOuter = (float) idxOuter;

               fy = eLeft->fsy;
               iy = FixedToInt(fy);

               adjx = (float)(fx - eLeft->fx0);  /* SCALED! */
               adjy = eLeft->adjy;		 /* SCALED! */

               vLower = eLeft->v0;

#ifdef PIXEL_ADDRESS
               {
                  pRow = PIXEL_ADDRESS( FixedToInt(fxLeftEdge), iy );
                  dPRowOuter = -BYTES_PER_ROW + idxOuter * sizeof(PIXEL_TYPE);
                  /* negative because Y=0 at bottom and increases upward */
               }
#endif
               /*
                * Now we need the set of parameter (z, color, etc.) values at
                * the point (fx, fy).  This gives us properly-sampled parameter
                * values that we can step from pixel to pixel.  Furthermore,
                * although we might have intermediate results that overflow
                * the normal parameter range when we step temporarily outside
                * the triangle, we shouldn't overflow or underflow for any
                * pixel that's actually inside the triangle.
                */

#if INTERP_Z
               {
                  GLfloat z0, tmp;
                  z0 = VB->Win[vLower][2] + ctx->PolygonZoffset;
#if DEPTH_BITS==16
                  /* interpolate fixed-pt values */
                  tmp = (z0 * FIXED_SCALE + dzdx * adjx + dzdy * adjy) + FIXED_HALF;
                  if (tmp < MAX_GLUINT/2)
                     fz = (GLfixed) tmp;
                  else
                     fz = MAX_GLUINT/2;
                  fdzOuter = SignedFloatToFixed(dzdy + dxOuter * dzdx);
#else
                  /* interpolate depth values exactly */
                  fz = (GLint) (z0 + dzdx*FixedToFloat(adjx) + dzdy*FixedToFloat(adjy));
                  fdzOuter = (GLint) (dzdy + dxOuter * dzdx);
#endif
                  zRow = Z_ADDRESS( ctx, FixedToInt(fxLeftEdge), iy );
                  dZRowOuter = (ctx->Buffer->Width + idxOuter) * sizeof(GLdepth);
               }
#endif
#if INTERP_RGB
               fr = (GLfixed)(IntToFixed(VB->Color[vLower][0]) + drdx * adjx + drdy * adjy)
                    + FIXED_HALF;
               fdrOuter = SignedFloatToFixed(drdy + dxOuter * drdx);

               fg = (GLfixed)(IntToFixed(VB->Color[vLower][1]) + dgdx * adjx + dgdy * adjy)
                    + FIXED_HALF;
               fdgOuter = SignedFloatToFixed(dgdy + dxOuter * dgdx);

               fb = (GLfixed)(IntToFixed(VB->Color[vLower][2]) + dbdx * adjx + dbdy * adjy)
                    + FIXED_HALF;
               fdbOuter = SignedFloatToFixed(dbdy + dxOuter * dbdx);
#endif
#if INTERP_ALPHA
               fa = (GLfixed)(IntToFixed(VB->Color[vLower][3]) + dadx * adjx + dady * adjy)
                    + FIXED_HALF;
               fdaOuter = SignedFloatToFixed(dady + dxOuter * dadx);
#endif
#if INTERP_INDEX
               fi = (GLfixed)(VB->Index[vLower] * FIXED_SCALE + didx * adjx
                              + didy * adjy) + FIXED_HALF;
               fdiOuter = SignedFloatToFixed(didy + dxOuter * didx);
#endif
#if INTERP_ST
               {
                  GLfloat s0, t0;
                  s0 = VB->TexCoord[vLower][0] * S_SCALE;
                  fs = (GLfixed)(s0 * FIXED_SCALE + dsdx * adjx + dsdy * adjy) + FIXED_HALF;
                  fdsOuter = SignedFloatToFixed(dsdy + dxOuter * dsdx);
                  t0 = VB->TexCoord[vLower][1] * T_SCALE;
                  ft = (GLfixed)(t0 * FIXED_SCALE + dtdx * adjx + dtdy * adjy) + FIXED_HALF;
                  fdtOuter = SignedFloatToFixed(dtdy + dxOuter * dtdx);
               }
#endif
#if INTERP_STW
               {
                  GLfloat w0 = 1.0F / VB->Clip[vLower][3];
                  GLfloat s0, t0, u0, v0;
                  wLeft = w0 + (dwdx * adjx + dwdy * adjy) * (1.0F/FIXED_SCALE);
		  dwOuter = dwdy + dxOuter * dwdx;
                  s0 = VB->TexCoord[vLower][0] * w0;
                  sLeft = s0 + (dsdx * adjx + dsdy * adjy) * (1.0F/FIXED_SCALE);
                  dsOuter = dsdy + dxOuter * dsdx;
                  t0 = VB->TexCoord[vLower][1] * w0;
                  tLeft = t0 + (dtdx * adjx + dtdy * adjy) * (1.0F/FIXED_SCALE);
                  dtOuter = dtdy + dxOuter * dtdx;
#if INTERP_UV
                  u0 = VB->TexCoord[vLower][2] * w0;
                  uLeft = u0 + (dudx * adjx + dudy * adjy) * (1.0F/FIXED_SCALE);
                  duOuter = dudy + dxOuter * dudx;
                  /* Note: don't divide V component by W */
                  v0 = VB->TexCoord[vLower][3];
                  vLeft = v0 + (dvdx * adjx + dvdy * adjy) * (1.0F/FIXED_SCALE);
                  dvOuter = dvdy + dxOuter * dvdx;
#endif
               }
#endif

            } /*if setupLeft*/


            if (setupRight && eRight->lines>0) {
               fxRightEdge = eRight->fsx - FIXED_EPSILON;
               fdxRightEdge = eRight->fdxdy;
            }

            if (lines==0) {
               continue;
            }


            /* Rasterize setup */
#ifdef PIXEL_ADDRESS
            dPRowInner = dPRowOuter + sizeof(PIXEL_TYPE);
#endif
#if INTERP_Z
            dZRowInner = dZRowOuter + sizeof(GLdepth);
            fdzInner = fdzOuter + fdzdx;
#endif
#if INTERP_RGB
            fdrInner = fdrOuter + fdrdx;
            fdgInner = fdgOuter + fdgdx;
            fdbInner = fdbOuter + fdbdx;
#endif
#if INTERP_ALPHA
            fdaInner = fdaOuter + fdadx;
#endif
#if INTERP_INDEX
            fdiInner = fdiOuter + fdidx;
#endif
#if INTERP_ST
            fdsInner = fdsOuter + fdsdx;
            fdtInner = fdtOuter + fdtdx;
#endif
#if INTERP_STW
	    dwInner = dwOuter + dwdx;
	    dsInner = dsOuter + dsdx;
	    dtInner = dtOuter + dtdx;
#if INTERP_UV
	    duInner = duOuter + dudx;
	    dvInner = dvOuter + dvdx;
#endif
#endif

            while (lines>0) {
               /* initialize the span interpolants to the leftmost value */
               /* ff = fixed-pt fragment */
#if INTERP_Z
               GLfixed ffz = fz;
               /*GLdepth *zp = zRow;*/
#endif
#if INTERP_RGB
               GLfixed ffr = fr,  ffg = fg,  ffb = fb;
#endif
#if INTERP_ALPHA
               GLfixed ffa = fa;
#endif
#if INTERP_INDEX
               GLfixed ffi = fi;
#endif
#if INTERP_ST
               GLfixed ffs = fs,  fft = ft;
#endif
#if INTERP_STW
               GLfloat ss = sLeft,  tt = tLeft,  ww = wLeft;
#endif
#if INTERP_UV
               GLfloat uu = uLeft,  vv = vLeft;
#endif
               GLint left = FixedToInt(fxLeftEdge);
               GLint right = FixedToInt(fxRightEdge);

#if INTERP_RGB
               {
                  /* need this to accomodate round-off errors */
                  GLfixed ffrend = ffr+(right-left-1)*fdrdx;
                  GLfixed ffgend = ffg+(right-left-1)*fdgdx;
                  GLfixed ffbend = ffb+(right-left-1)*fdbdx;
                  if (ffrend<0) ffr -= ffrend;
                  if (ffgend<0) ffg -= ffgend;
                  if (ffbend<0) ffb -= ffbend;
                  if (ffr<0) ffr = 0;
                  if (ffg<0) ffg = 0;
                  if (ffb<0) ffb = 0;
               }
#endif
#if INTERP_ALPHA
               {
                  GLfixed ffaend = ffa+(right-left-1)*fdadx;
                  if (ffaend<0) ffa -= ffaend;
                  if (ffa<0) ffa = 0;
               }
#endif
#if INTERP_INDEX
               if (ffi<0) ffi = 0;
#endif

               INNER_LOOP( left, right, iy ); //En inner loop es que me voy al otro, pero ese otro debería regresarme a acá... 

               /*
                * Advance to the next scan line.  Compute the
                * new edge coordinates, and adjust the
                * pixel-center x coordinate so that it stays
                * on or inside the major edge.
                */
               iy++;
               lines--;

               fxLeftEdge += fdxLeftEdge;
               fxRightEdge += fdxRightEdge;


               fError += fdError;
               if (fError >= 0) {
                  fError -= FIXED_ONE;
#ifdef PIXEL_ADDRESS
                  pRow = (PIXEL_TYPE*) ((GLubyte*)pRow + dPRowOuter);
#endif
#if INTERP_Z
                  zRow = (GLdepth*) ((GLubyte*)zRow + dZRowOuter);
                  fz += fdzOuter;
#endif
#if INTERP_RGB
                  fr += fdrOuter;   fg += fdgOuter;   fb += fdbOuter;
#endif
#if INTERP_ALPHA
                  fa += fdaOuter;
#endif
#if INTERP_INDEX
                  fi += fdiOuter;
#endif
#if INTERP_ST
                  fs += fdsOuter;   ft += fdtOuter;
#endif
#if INTERP_STW
		  sLeft += dsOuter;
		  tLeft += dtOuter;
		  wLeft += dwOuter;
#endif
#if INTERP_UV
		  uLeft += duOuter;
		  vLeft += dvOuter;
#endif
               }
               else {
#ifdef PIXEL_ADDRESS
                  pRow = (PIXEL_TYPE*) ((GLubyte*)pRow + dPRowInner);
#endif
#if INTERP_Z
                  zRow = (GLdepth*) ((GLubyte*)zRow + dZRowInner);
                  fz += fdzInner;
#endif
#if INTERP_RGB
                  fr += fdrInner;   fg += fdgInner;   fb += fdbInner;
#endif
#if INTERP_ALPHA
                  fa += fdaInner;
#endif
#if INTERP_INDEX
                  fi += fdiInner;
#endif
#if INTERP_ST
                  fs += fdsInner;   ft += fdtInner;
#endif
#if INTERP_STW
		  sLeft += dsInner;
		  tLeft += dtInner;
		  wLeft += dwInner;
#endif
#if INTERP_UV
		  uLeft += duInner;
		  vLeft += dvInner;
#endif
               }
            } /*while lines>0*/

         } /* for subTriangle */

      }
   }
}

#undef SETUP_CODE
#undef INNER_LOOP

#undef PIXEL_TYPE
#undef BYTES_PER_ROW
#undef PIXEL_ADDRESS

#undef INTERP_Z
#undef INTERP_RGB
#undef INTERP_ALPHA
#undef INTERP_INDEX
#undef INTERP_ST
#undef INTERP_STW
#undef INTERP_UV

#undef S_SCALE
#undef T_SCALE
}





/*********************************************************************************************/
/*
 * Render a smooth-shaded, textured, RGBA triangle.
 * Interpolate S,T,U with perspective correction, w/out mipmapping.
 * Note: we use texture coordinates S,T,U,V instead of S,T,R,Q because
 * R is already used for red.
 */
static void general_textured_triangle( GLcontext *ctx, GLuint v0, GLuint v1,
                                       GLuint v2, GLuint pv )
{

#define INTERP_Z 1
#define INTERP_RGB 1
#define INTERP_ALPHA 1
#define INTERP_STW 1
#define INTERP_UV 1
#define SETUP_CODE						\
   GLboolean flat_shade = (ctx->Light.ShadeModel==GL_FLAT);	\
   GLint r, g, b, a;						\
   if (flat_shade) {						\
      r = VB->Color[pv][0];					\
      g = VB->Color[pv][1];					\
      b = VB->Color[pv][2];					\
      a = VB->Color[pv][3];					\
   }
#define INNER_LOOP( LEFT, RIGHT, Y )				\
	{							\
	   GLint i, n = RIGHT-LEFT;				\
	   GLdepth zspan[MAX_WIDTH];				\
	   GLubyte red[MAX_WIDTH], green[MAX_WIDTH];		\
	   GLubyte blue[MAX_WIDTH], alpha[MAX_WIDTH];		\
           GLfloat s[MAX_WIDTH], t[MAX_WIDTH], u[MAX_WIDTH];	\
	   if (n>0) {						\
              if (flat_shade) {					\
                 for (i=0;i<n;i++) {				\
		    GLdouble wwvvInv = 1.0 / (ww*vv);		\
		    zspan[i] = FixedToDepth(ffz);		\
		    red[i]   = r;				\
		    green[i] = g;				\
		    blue[i]  = b;				\
		    alpha[i] = a;				\
		    s[i] = ss*wwvvInv;				\
		    t[i] = tt*wwvvInv;				\
		    u[i] = uu*wwvvInv;				\
		    ffz += fdzdx;				\
		    ss += dsdx;					\
		    tt += dtdx;					\
		    uu += dudx;					\
		    vv += dvdx;					\
		    ww += dwdx;					\
		 }						\
              }							\
              else {						\
                 for (i=0;i<n;i++) {				\
		    GLdouble wwvvInv = 1.0 / (ww*vv);		\
		    zspan[i] = FixedToDepth(ffz);		\
		    red[i]   = FixedToInt(ffr);			\
		    green[i] = FixedToInt(ffg);			\
		    blue[i]  = FixedToInt(ffb);			\
		    alpha[i] = FixedToInt(ffa);			\
		    s[i] = ss*wwvvInv;				\
		    t[i] = tt*wwvvInv;				\
		    u[i] = uu*wwvvInv;				\
		    ffz += fdzdx;				\
		    ffr += fdrdx;				\
		    ffg += fdgdx;				\
		    ffb += fdbdx;				\
		    ffa += fdadx;				\
		    ss += dsdx;					\
		    tt += dtdx;					\
		    uu += dudx;					\
		    ww += dwdx;					\
		    vv += dvdx;					\
		 }						\
              }							\
	      gl_write_texture_span( ctx, n, LEFT, Y, zspan,	\
                                     s, t, u, NULL, 		\
	                             red, green, blue, alpha,	\
				     GL_POLYGON );		\
	   }							\
	}

/*void triangle( GLcontext *ctx, GLuint v0, GLuint v1, GLuint v2, GLuint pv )*/
{
   typedef struct {
        GLint v0, v1;     /* Y(v0) < Y(v1) */
	GLfloat dx;	/* X(v1) - X(v0) */
	GLfloat dy;	/* Y(v1) - Y(v0) */
	GLfixed fdxdy;	/* dx/dy in fixed-point */
	GLfixed fsx;	/* first sample point x coord */
	GLfixed fsy;
	GLfloat adjy;	/* adjust from v[0]->fy to fsy, scaled */
	GLint lines;	/* number of lines to be sampled on this edge */
	GLfixed fx0;	/* fixed pt X of lower endpoint */
   } EdgeT;

   struct vertex_buffer *VB = ctx->VB;
   EdgeT eMaj, eTop, eBot;
   GLfloat oneOverArea;
   int vMin, vMid, vMax;       /* vertex indexes:  Y(vMin)<=Y(vMid)<=Y(vMax) */

   /* find the order of the 3 vertices along the Y axis */
   {
      GLfloat y0 = VB->Win[v0][1];
      GLfloat y1 = VB->Win[v1][1];
      GLfloat y2 = VB->Win[v2][1];

      if (y0<=y1) {
	 if (y1<=y2) {
	    vMin = v0;   vMid = v1;   vMax = v2;   /* y0<=y1<=y2 */
	 }
	 else if (y2<=y0) {
	    vMin = v2;   vMid = v0;   vMax = v1;   /* y2<=y0<=y1 */
	 }
	 else {
	    vMin = v0;   vMid = v2;   vMax = v1;   /* y0<=y2<=y1 */
	 }
      }
      else {
	 if (y0<=y2) {
	    vMin = v1;   vMid = v0;   vMax = v2;   /* y1<=y0<=y2 */
	 }
	 else if (y2<=y1) {
	    vMin = v2;   vMid = v1;   vMax = v0;   /* y2<=y1<=y0 */
	 }
	 else {
	    vMin = v1;   vMid = v2;   vMax = v0;   /* y1<=y2<=y0 */
	 }
      }
   }

   /* vertex/edge relationship */
   eMaj.v0 = vMin;   eMaj.v1 = vMax;   /*TODO: .v1's not needed */
   eTop.v0 = vMid;   eTop.v1 = vMax;
   eBot.v0 = vMin;   eBot.v1 = vMid;

   /* compute deltas for each edge:  vertex[v1] - vertex[v0] */
   eMaj.dx = VB->Win[vMax][0] - VB->Win[vMin][0];
   eMaj.dy = VB->Win[vMax][1] - VB->Win[vMin][1];
   eTop.dx = VB->Win[vMax][0] - VB->Win[vMid][0];
   eTop.dy = VB->Win[vMax][1] - VB->Win[vMid][1];
   eBot.dx = VB->Win[vMid][0] - VB->Win[vMin][0];
   eBot.dy = VB->Win[vMid][1] - VB->Win[vMin][1];

   /* compute oneOverArea */
   {
      GLfloat area = eMaj.dx * eBot.dy - eBot.dx * eMaj.dy;
      if (area>-0.05f && area<0.05f) {
         return;  /* very small; CULLED */
      }
      oneOverArea = 1.0F / area;
   }

   /* Edge setup.  For a triangle strip these could be reused... */
   {
      /* fixed point Y coordinates */
      GLfixed vMin_fx = FloatToFixed(VB->Win[vMin][0] + 0.5F);
      GLfixed vMin_fy = FloatToFixed(VB->Win[vMin][1] - 0.5F);
      GLfixed vMid_fx = FloatToFixed(VB->Win[vMid][0] + 0.5F);
      GLfixed vMid_fy = FloatToFixed(VB->Win[vMid][1] - 0.5F);
      GLfixed vMax_fy = FloatToFixed(VB->Win[vMax][1] - 0.5F);

      eMaj.fsy = FixedCeil(vMin_fy);
      eMaj.lines = FixedToInt(vMax_fy + FIXED_ONE - FIXED_EPSILON - eMaj.fsy);
      if (eMaj.lines > 0) {
         GLfloat dxdy = eMaj.dx / eMaj.dy;
         eMaj.fdxdy = SignedFloatToFixed(dxdy);
         eMaj.adjy = (GLfloat) (eMaj.fsy - vMin_fy);  /* SCALED! */
         eMaj.fx0 = vMin_fx;
         eMaj.fsx = eMaj.fx0 + (GLfixed) (eMaj.adjy * dxdy);
      }
      else {
         return;  /*CULLED*/
      }

      eTop.fsy = FixedCeil(vMid_fy);
      eTop.lines = FixedToInt(vMax_fy + FIXED_ONE - FIXED_EPSILON - eTop.fsy);
      if (eTop.lines > 0) {
         GLfloat dxdy = eTop.dx / eTop.dy;
         eTop.fdxdy = SignedFloatToFixed(dxdy);
         eTop.adjy = (GLfloat) (eTop.fsy - vMid_fy); /* SCALED! */
         eTop.fx0 = vMid_fx;
         eTop.fsx = eTop.fx0 + (GLfixed) (eTop.adjy * dxdy);
      }

      eBot.fsy = FixedCeil(vMin_fy);
      eBot.lines = FixedToInt(vMid_fy + FIXED_ONE - FIXED_EPSILON - eBot.fsy);
      if (eBot.lines > 0) {
         GLfloat dxdy = eBot.dx / eBot.dy;
         eBot.fdxdy = SignedFloatToFixed(dxdy);
         eBot.adjy = (GLfloat) (eBot.fsy - vMin_fy);  /* SCALED! */
         eBot.fx0 = vMin_fx;
         eBot.fsx = eBot.fx0 + (GLfixed) (eBot.adjy * dxdy);
      }
   }

   /*
    * Conceptually, we view a triangle as two subtriangles
    * separated by a perfectly horizontal line.  The edge that is
    * intersected by this line is one with maximal absolute dy; we
    * call it a ``major'' edge.  The other two edges are the
    * ``top'' edge (for the upper subtriangle) and the ``bottom''
    * edge (for the lower subtriangle).  If either of these two
    * edges is horizontal or very close to horizontal, the
    * corresponding subtriangle might cover zero sample points;
    * we take care to handle such cases, for performance as well
    * as correctness.
    *
    * By stepping rasterization parameters along the major edge,
    * we can avoid recomputing them at the discontinuity where
    * the top and bottom edges meet.  However, this forces us to
    * be able to scan both left-to-right and right-to-left. 
    * Also, we must determine whether the major edge is at the
    * left or right side of the triangle.  We do this by
    * computing the magnitude of the cross-product of the major
    * and top edges.  Since this magnitude depends on the sine of
    * the angle between the two edges, its sign tells us whether
    * we turn to the left or to the right when travelling along
    * the major edge to the top edge, and from this we infer
    * whether the major edge is on the left or the right.
    *
    * Serendipitously, this cross-product magnitude is also a
    * value we need to compute the iteration parameter
    * derivatives for the triangle, and it can be used to perform
    * backface culling because its sign tells us whether the
    * triangle is clockwise or counterclockwise.  In this code we
    * refer to it as ``area'' because it's also proportional to
    * the pixel area of the triangle.
    */

   {
      GLint ltor;		/* true if scanning left-to-right */
#if INTERP_Z
      GLfloat dzdx, dzdy;      GLfixed fdzdx;
#endif
#if INTERP_RGB
      GLfloat drdx, drdy;      GLfixed fdrdx;
      GLfloat dgdx, dgdy;      GLfixed fdgdx;
      GLfloat dbdx, dbdy;      GLfixed fdbdx;
#endif
#if INTERP_ALPHA
      GLfloat dadx, dady;      GLfixed fdadx;
#endif
#if INTERP_INDEX
      GLfloat didx, didy;      GLfixed fdidx;
#endif
#if INTERP_ST
      GLfloat dsdx, dsdy;      GLfixed fdsdx;
      GLfloat dtdx, dtdy;      GLfixed fdtdx;
#endif
#if INTERP_STW
      GLfloat dsdx, dsdy;
      GLfloat dtdx, dtdy;
      GLfloat dwdx, dwdy;
#endif
#if INTERP_UV
      GLfloat dudx, dudy;
      GLfloat dvdx, dvdy;
#endif

      /*
       * Execute user-supplied setup code
       */
#ifdef SETUP_CODE
      SETUP_CODE
#endif

      ltor = (oneOverArea < 0.0F);

      /* compute d?/dx and d?/dy derivatives */
#if INTERP_Z
      {
         GLfloat eMaj_dz, eBot_dz;
         eMaj_dz = VB->Win[vMax][2] - VB->Win[vMin][2];
         eBot_dz = VB->Win[vMid][2] - VB->Win[vMin][2];
         dzdx = oneOverArea * (eMaj_dz * eBot.dy - eMaj.dy * eBot_dz);
         if (dzdx>DEPTH_SCALE || dzdx<-DEPTH_SCALE) {
            /* probably a sliver triangle */
            dzdx = 0.0;
            dzdy = 0.0;
         }
         else {
            dzdy = oneOverArea * (eMaj.dx * eBot_dz - eMaj_dz * eBot.dx);
         }
#if DEPTH_BITS==16
         fdzdx = SignedFloatToFixed(dzdx);
#else
         fdzdx = (GLint) dzdx;
#endif
      }
#endif
#if INTERP_RGB
      {
         GLfloat eMaj_dr, eBot_dr;
         eMaj_dr = (GLint) VB->Color[vMax][0] - (GLint) VB->Color[vMin][0];
         eBot_dr = (GLint) VB->Color[vMid][0] - (GLint) VB->Color[vMin][0];
         drdx = oneOverArea * (eMaj_dr * eBot.dy - eMaj.dy * eBot_dr);
         fdrdx = SignedFloatToFixed(drdx);
         drdy = oneOverArea * (eMaj.dx * eBot_dr - eMaj_dr * eBot.dx);
      }
      {
         GLfloat eMaj_dg, eBot_dg;
         eMaj_dg = (GLint) VB->Color[vMax][1] - (GLint) VB->Color[vMin][1];
	 eBot_dg = (GLint) VB->Color[vMid][1] - (GLint) VB->Color[vMin][1];
         dgdx = oneOverArea * (eMaj_dg * eBot.dy - eMaj.dy * eBot_dg);
         fdgdx = SignedFloatToFixed(dgdx);
         dgdy = oneOverArea * (eMaj.dx * eBot_dg - eMaj_dg * eBot.dx);
      }
      {
         GLfloat eMaj_db, eBot_db;
         eMaj_db = (GLint) VB->Color[vMax][2] - (GLint) VB->Color[vMin][2];
         eBot_db = (GLint) VB->Color[vMid][2] - (GLint) VB->Color[vMin][2];
         dbdx = oneOverArea * (eMaj_db * eBot.dy - eMaj.dy * eBot_db);
         fdbdx = SignedFloatToFixed(dbdx);
	 dbdy = oneOverArea * (eMaj.dx * eBot_db - eMaj_db * eBot.dx);
      }
#endif
#if INTERP_ALPHA
      {
         GLfloat eMaj_da, eBot_da;
         eMaj_da = (GLint) VB->Color[vMax][3] - (GLint) VB->Color[vMin][3];
         eBot_da = (GLint) VB->Color[vMid][3] - (GLint) VB->Color[vMin][3];
         dadx = oneOverArea * (eMaj_da * eBot.dy - eMaj.dy * eBot_da);
         fdadx = SignedFloatToFixed(dadx);
         dady = oneOverArea * (eMaj.dx * eBot_da - eMaj_da * eBot.dx);
      }
#endif
#if INTERP_INDEX
      {
         GLfloat eMaj_di, eBot_di;
         eMaj_di = (GLint) VB->Index[vMax] - (GLint) VB->Index[vMin];
         eBot_di = (GLint) VB->Index[vMid] - (GLint) VB->Index[vMin];
         didx = oneOverArea * (eMaj_di * eBot.dy - eMaj.dy * eBot_di);
         fdidx = SignedFloatToFixed(didx);
         didy = oneOverArea * (eMaj.dx * eBot_di - eMaj_di * eBot.dx);
      }
#endif
#if INTERP_ST
      {
         GLfloat eMaj_ds, eBot_ds;
         eMaj_ds = (VB->TexCoord[vMax][0] - VB->TexCoord[vMin][0]) * S_SCALE;
         eBot_ds = (VB->TexCoord[vMid][0] - VB->TexCoord[vMin][0]) * S_SCALE;
         dsdx = oneOverArea * (eMaj_ds * eBot.dy - eMaj.dy * eBot_ds);
         fdsdx = SignedFloatToFixed(dsdx);
         dsdy = oneOverArea * (eMaj.dx * eBot_ds - eMaj_ds * eBot.dx);
      }
      {
         GLfloat eMaj_dt, eBot_dt;
         eMaj_dt = (VB->TexCoord[vMax][1] - VB->TexCoord[vMin][1]) * T_SCALE;
         eBot_dt = (VB->TexCoord[vMid][1] - VB->TexCoord[vMin][1]) * T_SCALE;
         dtdx = oneOverArea * (eMaj_dt * eBot.dy - eMaj.dy * eBot_dt);
         fdtdx = SignedFloatToFixed(dtdx);
         dtdy = oneOverArea * (eMaj.dx * eBot_dt - eMaj_dt * eBot.dx);
      }
#endif
#if INTERP_STW
      {
         GLfloat wMax = 1.0F / VB->Clip[vMax][3];
         GLfloat wMin = 1.0F / VB->Clip[vMin][3];
         GLfloat wMid = 1.0F / VB->Clip[vMid][3];
         GLfloat eMaj_dw, eBot_dw;
         GLfloat eMaj_ds, eBot_ds;
         GLfloat eMaj_dt, eBot_dt;
#if INTERP_UV
         GLfloat eMaj_du, eBot_du;
         GLfloat eMaj_dv, eBot_dv;
#endif
         eMaj_dw = wMax - wMin;
         eBot_dw = wMid - wMin;
         dwdx = oneOverArea * (eMaj_dw * eBot.dy - eMaj.dy * eBot_dw);
         dwdy = oneOverArea * (eMaj.dx * eBot_dw - eMaj_dw * eBot.dx);

         eMaj_ds = VB->TexCoord[vMax][0]*wMax - VB->TexCoord[vMin][0]*wMin;
         eBot_ds = VB->TexCoord[vMid][0]*wMid - VB->TexCoord[vMin][0]*wMin;
         dsdx = oneOverArea * (eMaj_ds * eBot.dy - eMaj.dy * eBot_ds);
         dsdy = oneOverArea * (eMaj.dx * eBot_ds - eMaj_ds * eBot.dx);

         eMaj_dt = VB->TexCoord[vMax][1]*wMax - VB->TexCoord[vMin][1]*wMin;
         eBot_dt = VB->TexCoord[vMid][1]*wMid - VB->TexCoord[vMin][1]*wMin;
         dtdx = oneOverArea * (eMaj_dt * eBot.dy - eMaj.dy * eBot_dt);
         dtdy = oneOverArea * (eMaj.dx * eBot_dt - eMaj_dt * eBot.dx);
#if INTERP_UV
         eMaj_du = VB->TexCoord[vMax][2]*wMax - VB->TexCoord[vMin][2]*wMin;
         eBot_du = VB->TexCoord[vMid][2]*wMid - VB->TexCoord[vMin][2]*wMin;
         dudx = oneOverArea * (eMaj_du * eBot.dy - eMaj.dy * eBot_du);
         dudy = oneOverArea * (eMaj.dx * eBot_du - eMaj_du * eBot.dx);

         /* Note: don't divide V component by W */
         eMaj_dv = VB->TexCoord[vMax][3] - VB->TexCoord[vMin][3];
         eBot_dv = VB->TexCoord[vMid][3] - VB->TexCoord[vMin][3];
         dvdx = oneOverArea * (eMaj_dv * eBot.dy - eMaj.dy * eBot_dv);
         dvdy = oneOverArea * (eMaj.dx * eBot_dv - eMaj_dv * eBot.dx);
#endif
      }
#endif

      /*
       * We always sample at pixel centers.  However, we avoid
       * explicit half-pixel offsets in this code by incorporating
       * the proper offset in each of x and y during the
       * transformation to window coordinates.
       *
       * We also apply the usual rasterization rules to prevent
       * cracks and overlaps.  A pixel is considered inside a
       * subtriangle if it meets all of four conditions: it is on or
       * to the right of the left edge, strictly to the left of the
       * right edge, on or below the top edge, and strictly above
       * the bottom edge.  (Some edges may be degenerate.)
       *
       * The following discussion assumes left-to-right scanning
       * (that is, the major edge is on the left); the right-to-left
       * case is a straightforward variation.
       *
       * We start by finding the half-integral y coordinate that is
       * at or below the top of the triangle.  This gives us the
       * first scan line that could possibly contain pixels that are
       * inside the triangle.
       *
       * Next we creep down the major edge until we reach that y,
       * and compute the corresponding x coordinate on the edge. 
       * Then we find the half-integral x that lies on or just
       * inside the edge.  This is the first pixel that might lie in
       * the interior of the triangle.  (We won't know for sure
       * until we check the other edges.)
       *
       * As we rasterize the triangle, we'll step down the major
       * edge.  For each step in y, we'll move an integer number
       * of steps in x.  There are two possible x step sizes, which
       * we'll call the ``inner'' step (guaranteed to land on the
       * edge or inside it) and the ``outer'' step (guaranteed to
       * land on the edge or outside it).  The inner and outer steps
       * differ by one.  During rasterization we maintain an error
       * term that indicates our distance from the true edge, and
       * select either the inner step or the outer step, whichever
       * gets us to the first pixel that falls inside the triangle.
       *
       * All parameters (z, red, etc.) as well as the buffer
       * addresses for color and z have inner and outer step values,
       * so that we can increment them appropriately.  This method
       * eliminates the need to adjust parameters by creeping a
       * sub-pixel amount into the triangle at each scanline.
       */

      {
         int subTriangle;
         GLfixed fx, fxLeftEdge, fxRightEdge, fdxLeftEdge, fdxRightEdge;
         GLfixed fdxOuter;
         int idxOuter;
         float dxOuter;
         GLfixed fError, fdError;
         float adjx, adjy;
         GLfixed fy;
         int iy;
#ifdef PIXEL_ADDRESS
         PIXEL_TYPE *pRow;
         int dPRowOuter, dPRowInner;  /* offset in bytes */
#endif
#if INTERP_Z
         GLdepth *zRow;
         int dZRowOuter, dZRowInner;  /* offset in bytes */
         GLfixed fz, fdzOuter, fdzInner;
#endif
#if INTERP_RGB
         GLfixed fr, fdrOuter, fdrInner;
         GLfixed fg, fdgOuter, fdgInner;
         GLfixed fb, fdbOuter, fdbInner;
#endif
#if INTERP_ALPHA
         GLfixed fa, fdaOuter, fdaInner;
#endif
#if INTERP_INDEX
         GLfixed fi, fdiOuter, fdiInner;
#endif
#if INTERP_ST
         GLfixed fs, fdsOuter, fdsInner;
         GLfixed ft, fdtOuter, fdtInner;
#endif
#if INTERP_STW
         GLfloat sLeft, dsOuter, dsInner;
         GLfloat tLeft, dtOuter, dtInner;
         GLfloat wLeft, dwOuter, dwInner;
#endif
#if INTERP_UV
         GLfloat uLeft, duOuter, duInner;
         GLfloat vLeft, dvOuter, dvInner;
#endif

         for (subTriangle=0; subTriangle<=1; subTriangle++) { //SUBTRIANGLE FOR...
            EdgeT *eLeft, *eRight;
            int setupLeft, setupRight;
            int lines;

            if (subTriangle==0) {
               /* bottom half */
               if (ltor) {
                  eLeft = &eMaj;
                  eRight = &eBot;
                  lines = eRight->lines;
                  setupLeft = 1;
                  setupRight = 1;
               }
               else {
                  eLeft = &eBot;
                  eRight = &eMaj;
                  lines = eLeft->lines;
                  setupLeft = 1;
                  setupRight = 1;
               }
            }
            else {
               /* top half */
               if (ltor) {
                  eLeft = &eMaj;
                  eRight = &eTop;
                  lines = eRight->lines;
                  setupLeft = 0;
                  setupRight = 1;
               }
               else {
                  eLeft = &eTop;
                  eRight = &eMaj;
                  lines = eLeft->lines;
                  setupLeft = 1;
                  setupRight = 0;
               }
               if (lines==0) return;
            }

            if (setupLeft && eLeft->lines>0) {
               GLint vLower;
               GLfixed fsx = eLeft->fsx;
               fx = FixedCeil(fsx);
               fError = fx - fsx - FIXED_ONE;
               fxLeftEdge = fsx - FIXED_EPSILON;
               fdxLeftEdge = eLeft->fdxdy;
               fdxOuter = FixedFloor(fdxLeftEdge - FIXED_EPSILON);
               fdError = fdxOuter - fdxLeftEdge + FIXED_ONE;
               idxOuter = FixedToInt(fdxOuter);
               dxOuter = (float) idxOuter;

               fy = eLeft->fsy;
               iy = FixedToInt(fy);

               adjx = (float)(fx - eLeft->fx0);  /* SCALED! */
               adjy = eLeft->adjy;		 /* SCALED! */

               vLower = eLeft->v0;

#ifdef PIXEL_ADDRESS
               {
                  pRow = PIXEL_ADDRESS( FixedToInt(fxLeftEdge), iy );
                  dPRowOuter = -BYTES_PER_ROW + idxOuter * sizeof(PIXEL_TYPE);
                  /* negative because Y=0 at bottom and increases upward */
               }
#endif
               /*
                * Now we need the set of parameter (z, color, etc.) values at
                * the point (fx, fy).  This gives us properly-sampled parameter
                * values that we can step from pixel to pixel.  Furthermore,
                * although we might have intermediate results that overflow
                * the normal parameter range when we step temporarily outside
                * the triangle, we shouldn't overflow or underflow for any
                * pixel that's actually inside the triangle.
                */

#if INTERP_Z
               {
                  GLfloat z0, tmp;
                  z0 = VB->Win[vLower][2] + ctx->PolygonZoffset;
#if DEPTH_BITS==16
                  /* interpolate fixed-pt values */
                  tmp = (z0 * FIXED_SCALE + dzdx * adjx + dzdy * adjy) + FIXED_HALF;
                  if (tmp < MAX_GLUINT/2)
                     fz = (GLfixed) tmp;
                  else
                     fz = MAX_GLUINT/2;
                  fdzOuter = SignedFloatToFixed(dzdy + dxOuter * dzdx);
#else
                  /* interpolate depth values exactly */
                  fz = (GLint) (z0 + dzdx*FixedToFloat(adjx) + dzdy*FixedToFloat(adjy));
                  fdzOuter = (GLint) (dzdy + dxOuter * dzdx);
#endif
                  zRow = Z_ADDRESS( ctx, FixedToInt(fxLeftEdge), iy );
                  dZRowOuter = (ctx->Buffer->Width + idxOuter) * sizeof(GLdepth);
               }
#endif
#if INTERP_RGB
               fr = (GLfixed)(IntToFixed(VB->Color[vLower][0]) + drdx * adjx + drdy * adjy)
                    + FIXED_HALF;
               fdrOuter = SignedFloatToFixed(drdy + dxOuter * drdx);

               fg = (GLfixed)(IntToFixed(VB->Color[vLower][1]) + dgdx * adjx + dgdy * adjy)
                    + FIXED_HALF;
               fdgOuter = SignedFloatToFixed(dgdy + dxOuter * dgdx);

               fb = (GLfixed)(IntToFixed(VB->Color[vLower][2]) + dbdx * adjx + dbdy * adjy)
                    + FIXED_HALF;
               fdbOuter = SignedFloatToFixed(dbdy + dxOuter * dbdx);
#endif
#if INTERP_ALPHA
               fa = (GLfixed)(IntToFixed(VB->Color[vLower][3]) + dadx * adjx + dady * adjy)
                    + FIXED_HALF;
               fdaOuter = SignedFloatToFixed(dady + dxOuter * dadx);
#endif
#if INTERP_INDEX
               fi = (GLfixed)(VB->Index[vLower] * FIXED_SCALE + didx * adjx
                              + didy * adjy) + FIXED_HALF;
               fdiOuter = SignedFloatToFixed(didy + dxOuter * didx);
#endif
#if INTERP_ST
               {
                  GLfloat s0, t0;
                  s0 = VB->TexCoord[vLower][0] * S_SCALE;
                  fs = (GLfixed)(s0 * FIXED_SCALE + dsdx * adjx + dsdy * adjy) + FIXED_HALF;
                  fdsOuter = SignedFloatToFixed(dsdy + dxOuter * dsdx);
                  t0 = VB->TexCoord[vLower][1] * T_SCALE;
                  ft = (GLfixed)(t0 * FIXED_SCALE + dtdx * adjx + dtdy * adjy) + FIXED_HALF;
                  fdtOuter = SignedFloatToFixed(dtdy + dxOuter * dtdx);
               }
#endif
#if INTERP_STW
               {
                  GLfloat w0 = 1.0F / VB->Clip[vLower][3];
                  GLfloat s0, t0, u0, v0;
                  wLeft = w0 + (dwdx * adjx + dwdy * adjy) * (1.0F/FIXED_SCALE);
		  dwOuter = dwdy + dxOuter * dwdx;
                  s0 = VB->TexCoord[vLower][0] * w0;
                  sLeft = s0 + (dsdx * adjx + dsdy * adjy) * (1.0F/FIXED_SCALE);
                  dsOuter = dsdy + dxOuter * dsdx;
                  t0 = VB->TexCoord[vLower][1] * w0;
                  tLeft = t0 + (dtdx * adjx + dtdy * adjy) * (1.0F/FIXED_SCALE);
                  dtOuter = dtdy + dxOuter * dtdx;
#if INTERP_UV
                  u0 = VB->TexCoord[vLower][2] * w0;
                  uLeft = u0 + (dudx * adjx + dudy * adjy) * (1.0F/FIXED_SCALE);
                  duOuter = dudy + dxOuter * dudx;
                  /* Note: don't divide V component by W */
                  v0 = VB->TexCoord[vLower][3];
                  vLeft = v0 + (dvdx * adjx + dvdy * adjy) * (1.0F/FIXED_SCALE);
                  dvOuter = dvdy + dxOuter * dvdx;
#endif
               }
#endif

            } /*if setupLeft*/


            if (setupRight && eRight->lines>0) {
               fxRightEdge = eRight->fsx - FIXED_EPSILON;
               fdxRightEdge = eRight->fdxdy;
            }

            if (lines==0) {
               continue;
            }


            /* Rasterize setup */
#ifdef PIXEL_ADDRESS
            dPRowInner = dPRowOuter + sizeof(PIXEL_TYPE);
#endif
#if INTERP_Z
            dZRowInner = dZRowOuter + sizeof(GLdepth);
            fdzInner = fdzOuter + fdzdx;
#endif
#if INTERP_RGB
            fdrInner = fdrOuter + fdrdx;
            fdgInner = fdgOuter + fdgdx;
            fdbInner = fdbOuter + fdbdx;
#endif
#if INTERP_ALPHA
            fdaInner = fdaOuter + fdadx;
#endif
#if INTERP_INDEX
            fdiInner = fdiOuter + fdidx;
#endif
#if INTERP_ST
            fdsInner = fdsOuter + fdsdx;
            fdtInner = fdtOuter + fdtdx;
#endif
#if INTERP_STW
	    dwInner = dwOuter + dwdx;
	    dsInner = dsOuter + dsdx;
	    dtInner = dtOuter + dtdx;
#if INTERP_UV
	    duInner = duOuter + dudx;
	    dvInner = dvOuter + dvdx;
#endif
#endif

            while (lines>0) {
               /* initialize the span interpolants to the leftmost value */
               /* ff = fixed-pt fragment */
#if INTERP_Z
               GLfixed ffz = fz;
               /*GLdepth *zp = zRow;*/
#endif
#if INTERP_RGB
               GLfixed ffr = fr,  ffg = fg,  ffb = fb;
#endif
#if INTERP_ALPHA
               GLfixed ffa = fa;
#endif
#if INTERP_INDEX
               GLfixed ffi = fi;
#endif
#if INTERP_ST
               GLfixed ffs = fs,  fft = ft;
#endif
#if INTERP_STW
               GLfloat ss = sLeft,  tt = tLeft,  ww = wLeft;
#endif
#if INTERP_UV
               GLfloat uu = uLeft,  vv = vLeft;
#endif
               GLint left = FixedToInt(fxLeftEdge);
               GLint right = FixedToInt(fxRightEdge);

#if INTERP_RGB
               {
                  /* need this to accomodate round-off errors */
                  GLfixed ffrend = ffr+(right-left-1)*fdrdx;
                  GLfixed ffgend = ffg+(right-left-1)*fdgdx;
                  GLfixed ffbend = ffb+(right-left-1)*fdbdx;
                  if (ffrend<0) ffr -= ffrend;
                  if (ffgend<0) ffg -= ffgend;
                  if (ffbend<0) ffb -= ffbend;
                  if (ffr<0) ffr = 0;
                  if (ffg<0) ffg = 0;
                  if (ffb<0) ffb = 0;
               }
#endif
#if INTERP_ALPHA
               {
                  GLfixed ffaend = ffa+(right-left-1)*fdadx;
                  if (ffaend<0) ffa -= ffaend;
                  if (ffa<0) ffa = 0;
               }
#endif
#if INTERP_INDEX
               if (ffi<0) ffi = 0;
#endif

               INNER_LOOP( left, right, iy ); //En inner loop es que me voy al otro, pero ese otro debería regresarme a acá... 

               /*
                * Advance to the next scan line.  Compute the
                * new edge coordinates, and adjust the
                * pixel-center x coordinate so that it stays
                * on or inside the major edge.
                */
               iy++;
               lines--;

               fxLeftEdge += fdxLeftEdge;
               fxRightEdge += fdxRightEdge;


               fError += fdError;
               if (fError >= 0) {
                  fError -= FIXED_ONE;
#ifdef PIXEL_ADDRESS
                  pRow = (PIXEL_TYPE*) ((GLubyte*)pRow + dPRowOuter);
#endif
#if INTERP_Z
                  zRow = (GLdepth*) ((GLubyte*)zRow + dZRowOuter);
                  fz += fdzOuter;
#endif
#if INTERP_RGB
                  fr += fdrOuter;   fg += fdgOuter;   fb += fdbOuter;
#endif
#if INTERP_ALPHA
                  fa += fdaOuter;
#endif
#if INTERP_INDEX
                  fi += fdiOuter;
#endif
#if INTERP_ST
                  fs += fdsOuter;   ft += fdtOuter;
#endif
#if INTERP_STW
		  sLeft += dsOuter;
		  tLeft += dtOuter;
		  wLeft += dwOuter;
#endif
#if INTERP_UV
		  uLeft += duOuter;
		  vLeft += dvOuter;
#endif
               }
               else {
#ifdef PIXEL_ADDRESS
                  pRow = (PIXEL_TYPE*) ((GLubyte*)pRow + dPRowInner);
#endif
#if INTERP_Z
                  zRow = (GLdepth*) ((GLubyte*)zRow + dZRowInner);
                  fz += fdzInner;
#endif
#if INTERP_RGB
                  fr += fdrInner;   fg += fdgInner;   fb += fdbInner;
#endif
#if INTERP_ALPHA
                  fa += fdaInner;
#endif
#if INTERP_INDEX
                  fi += fdiInner;
#endif
#if INTERP_ST
                  fs += fdsInner;   ft += fdtInner;
#endif
#if INTERP_STW
		  sLeft += dsInner;
		  tLeft += dtInner;
		  wLeft += dwInner;
#endif
#if INTERP_UV
		  uLeft += duInner;
		  vLeft += dvInner;
#endif
               }
            } /*while lines>0*/

         } /* for subTriangle */

      }
   }
}

#undef SETUP_CODE
#undef INNER_LOOP

#undef PIXEL_TYPE
#undef BYTES_PER_ROW
#undef PIXEL_ADDRESS

#undef INTERP_Z
#undef INTERP_RGB
#undef INTERP_ALPHA
#undef INTERP_INDEX
#undef INTERP_ST
#undef INTERP_STW
#undef INTERP_UV

#undef S_SCALE
#undef T_SCALE
}



/*
 * Compute the lambda value (texture level value) for a fragment.
 */
static GLfloat compute_lambda( GLfloat s, GLfloat t,
                               GLfloat dsdx, GLfloat dsdy,
                               GLfloat dtdx, GLfloat dtdy,
                               GLfloat w, GLfloat dwdx, GLfloat dwdy,
                               GLfloat width, GLfloat height ) 
{
   /* TODO: this function can probably be optimized a bit */
   GLfloat invw = 1.0 / w;
   GLfloat dudx, dudy, dvdx, dvdy;
   GLfloat r1, r2, rho2;

   dudx = (dsdx - s*dwdx) * invw * width;
   dudy = (dsdy - s*dwdy) * invw * width;
   dvdx = (dtdx - t*dwdx) * invw * height;
   dvdy = (dtdy - t*dwdy) * invw * height;

   r1 = dudx * dudx + dudy * dudy;
   r2 = dvdx * dvdx + dvdy * dvdy;

   /* rho2 = MAX2(r1,r2); */
   rho2 = r1 + r2;
   if (rho2 <= 0.0F) {
      return 0.0F;
   }
   else {
      /* return log base 2 of rho */
      return log(rho2) * 1.442695 * 0.5;       /* 1.442695 = 1/log(2) */
   }
}



/*
 * Render a smooth-shaded, textured, RGBA triangle.
 * Interpolate S,T,U with perspective correction and compute lambda for
 * each fragment.  Lambda is used to determine whether to use the
 * minification or magnification filter.  If minification and using
 * mipmaps, lambda is also used to select the texture level of detail.
 */
static void lambda_textured_triangle( GLcontext *ctx, GLuint v0, GLuint v1,
                                      GLuint v2, GLuint pv )
{
#define INTERP_Z 1
#define INTERP_RGB 1
#define INTERP_ALPHA 1
#define INTERP_STW 1
#define INTERP_UV 1

#define SETUP_CODE							\
   GLboolean flat_shade = (ctx->Light.ShadeModel==GL_FLAT);		\
   GLint r, g, b, a;							\
   GLfloat twidth, theight;						\
   if (ctx->Texture.Enabled & TEXTURE_3D) {				\
      twidth = (GLfloat) ctx->Texture.Current3D->Image[0]->Width;	\
      theight = (GLfloat) ctx->Texture.Current3D->Image[0]->Height;	\
   }									\
   else if (ctx->Texture.Enabled & TEXTURE_2D) {			\
      twidth = (GLfloat) ctx->Texture.Current2D->Image[0]->Width;	\
      theight = (GLfloat) ctx->Texture.Current2D->Image[0]->Height;	\
   }									\
   else {								\
      twidth = (GLfloat) ctx->Texture.Current1D->Image[0]->Width;	\
      theight = 1.0;							\
   }									\
   if (flat_shade) {							\
      r = VB->Color[pv][0];						\
      g = VB->Color[pv][1];						\
      b = VB->Color[pv][2];						\
      a = VB->Color[pv][3];						\
   }

#define INNER_LOOP( LEFT, RIGHT, Y )					\
	{								\
	   GLint i, n = RIGHT-LEFT;					\
	   GLdepth zspan[MAX_WIDTH];					\
	   GLubyte red[MAX_WIDTH], green[MAX_WIDTH];			\
	   GLubyte blue[MAX_WIDTH], alpha[MAX_WIDTH];			\
           GLfloat s[MAX_WIDTH], t[MAX_WIDTH], u[MAX_WIDTH];		\
	   DEFARRAY(GLfloat,lambda,MAX_WIDTH);				\
	   if (n>0) {							\
	      if (flat_shade) {						\
		 for (i=0;i<n;i++) {					\
		    GLdouble wwvvInv = 1.0 / (ww*vv);			\
		    zspan[i] = FixedToDepth(ffz);			\
		    red[i]   = r;					\
		    green[i] = g;					\
		    blue[i]  = b;					\
		    alpha[i] = a;					\
		    s[i] = ss*wwvvInv;					\
		    t[i] = tt*wwvvInv;					\
		    u[i] = uu*wwvvInv;					\
		    lambda[i] = compute_lambda( s[i], t[i],		\
						dsdx, dsdy,		\
						dtdx, dtdy, ww,		\
						dwdx, dwdy,		\
						twidth, theight );	\
		    ffz += fdzdx;					\
		    ss += dsdx;						\
		    tt += dtdx;						\
		    uu += dudx;						\
		    vv += dvdx;						\
		    ww += dwdx;						\
		 }							\
              }								\
              else {							\
		 for (i=0;i<n;i++) {					\
		    GLdouble wwvvInv = 1.0 / (ww*vv);			\
		    zspan[i] = FixedToDepth(ffz);			\
		    red[i]   = FixedToInt(ffr);				\
		    green[i] = FixedToInt(ffg);				\
		    blue[i]  = FixedToInt(ffb);				\
		    alpha[i] = FixedToInt(ffa);				\
		    s[i] = ss*wwvvInv;					\
		    t[i] = tt*wwvvInv;					\
		    u[i] = uu*wwvvInv;					\
		    lambda[i] = compute_lambda( s[i], t[i],		\
						dsdx, dsdy,		\
						dtdx, dtdy, ww,		\
						dwdx, dwdy,		\
						twidth, theight );	\
		    ffz += fdzdx;					\
		    ffr += fdrdx;					\
		    ffg += fdgdx;					\
		    ffb += fdbdx;					\
		    ffa += fdadx;					\
		    ss += dsdx;						\
		    tt += dtdx;						\
		    uu += dudx;						\
		    vv += dvdx;						\
		    ww += dwdx;						\
		 }							\
              }								\
	      gl_write_texture_span( ctx, n, LEFT, Y, zspan,		\
                                     s, t, u, lambda,	 		\
	                             red, green, blue, alpha,		\
				     GL_POLYGON );			\
	   }								\
	   UNDEFARRAY(lambda);						\
	}

#include "tritemp.h"
}



/*
 * Null rasterizer for measuring transformation speed.
 */
static void null_triangle( GLcontext *ctx, GLuint v0, GLuint v1,
                           GLuint v2, GLuint pv )
{
}



/*
 * Determine which triangle rendering function to use given the current
 * rendering context.
 */
void gl_set_triangle_function( GLcontext *ctx )
{
   GLboolean rgbmode = ctx->Visual->RGBAflag;

   if (ctx->RenderMode==GL_RENDER) {
      if (ctx->NoRaster) {
         ctx->Driver.TriangleFunc = null_triangle;
         return;
      }
      if (ctx->Driver.TriangleFunc) {
         /* Device driver will draw triangles. */
      }
      else if (ctx->Texture.Enabled
               && ctx->Texture.Current
               && ctx->Texture.Current->Complete) {
         if (   (ctx->Texture.Enabled==TEXTURE_2D)
             && ctx->Texture.Current2D->MinFilter==GL_NEAREST
             && ctx->Texture.Current2D->MagFilter==GL_NEAREST
             && ctx->Texture.Current2D->WrapS==GL_REPEAT
             && ctx->Texture.Current2D->WrapT==GL_REPEAT
             && ctx->Texture.Current2D->Image[0]->Format==GL_RGB
             && ctx->Texture.Current2D->Image[0]->Border==0
             && (ctx->Texture.EnvMode==GL_DECAL
                 || ctx->Texture.EnvMode==GL_REPLACE)
             && ctx->Hint.PerspectiveCorrection==GL_FASTEST
             && ctx->TextureMatrixType==MATRIX_IDENTITY
             && ((ctx->RasterMask==DEPTH_BIT
                  && ctx->Depth.Func==GL_LESS
                  && ctx->Depth.Mask==GL_TRUE)
                 || ctx->RasterMask==0)
             && ctx->Polygon.StippleFlag==GL_FALSE
             && ctx->Visual->EightBitColor) {
            if (ctx->RasterMask==DEPTH_BIT) {
               ctx->Driver.TriangleFunc = simple_z_textured_triangle;
            }
            else {
               ctx->Driver.TriangleFunc = simple_textured_triangle;
            }
         }
         else {
            GLboolean needLambda = GL_TRUE;
            /* if mag filter == min filter we're not mipmapping */
            if (ctx->Texture.Enabled & TEXTURE_3D) {
               if (ctx->Texture.Current3D->MinFilter==
                   ctx->Texture.Current3D->MagFilter) {
                  needLambda = GL_FALSE;
               }
	    }
            else if (ctx->Texture.Enabled & TEXTURE_2D) {
               if (ctx->Texture.Current2D->MinFilter==
                   ctx->Texture.Current2D->MagFilter) {
                  needLambda = GL_FALSE;
               }
            }
            else if (ctx->Texture.Enabled & TEXTURE_1D) {
               if (ctx->Texture.Current1D->MinFilter==
                   ctx->Texture.Current1D->MagFilter) {
                  needLambda = GL_FALSE;
               }
            }
            ctx->Driver.TriangleFunc = needLambda ? lambda_textured_triangle
                                                  : general_textured_triangle;
         }
      }
      else {
	 if (ctx->Light.ShadeModel==GL_SMOOTH) {
	    /* smooth shaded, no texturing, stippled or some raster ops */
	    ctx->Driver.TriangleFunc = rgbmode ? smooth_rgba_triangle
                                               : smooth_ci_triangle;
	 }
	 else {
	    /* flat shaded, no texturing, stippled or some raster ops */
	    ctx->Driver.TriangleFunc = rgbmode ? flat_rgba_triangle
                                               : flat_ci_triangle;
	 }
      }
   }
   else if (ctx->RenderMode==GL_FEEDBACK) {
      ctx->Driver.TriangleFunc = feedback_triangle;
   }
   else {
      /* GL_SELECT mode */
      ctx->Driver.TriangleFunc = select_triangle;
   }
}

/* $Id: vbrender.c,v 1.16 1997/11/20 00:00:47 brianp Exp $ */

/*
 * Mesa 3-D graphics library
 * Version:  2.5
 * Copyright (C) 1995-1997  Brian Paul
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


/*
 * $Log: vbrender.c,v $
 * Revision 1.16  1997/11/20 00:00:47  brianp
 * only call Driver.RasterSetup() once in render_clipped_polygon()
 *
 * Revision 1.15  1997/09/18 01:32:47  brianp
 * fixed divide by zero problem for "weird" projection matrices
 *
 * Revision 1.14  1997/08/13 01:31:41  brianp
 * cleaned up code involving LightTwoSide
 *
 * Revision 1.13  1997/07/24 01:25:27  brianp
 * changed precompiled header symbol from PCH to PC_HEADER
 *
 * Revision 1.12  1997/07/11 02:19:52  brianp
 * flat-shaded quads in a strip were miscolored if clipped (Randy Frank)
 *
 * Revision 1.11  1997/05/28 03:26:49  brianp
 * added precompiled header (PCH) support
 *
 * Revision 1.10  1997/05/16 02:09:26  brianp
 * clipped GL_TRIANGLE_STRIP triangles sometimes got wrong provoking vertex
 *
 * Revision 1.9  1997/04/30 02:20:00  brianp
 * fixed a line clipping bug in GL_LINE_LOOPs
 *
 * Revision 1.8  1997/04/29 01:31:07  brianp
 * added RasterSetup() function to device driver
 *
 * Revision 1.7  1997/04/24 00:30:17  brianp
 * optimized glTexCoord2() code
 *
 * Revision 1.6  1997/04/20 19:47:06  brianp
 * fixed an error message, added a comment
 *
 * Revision 1.5  1997/04/20 15:59:30  brianp
 * removed VERTEX2_BIT stuff
 *
 * Revision 1.4  1997/04/20 15:27:34  brianp
 * removed odd_flag from all polygon rendering functions
 *
 * Revision 1.3  1997/04/12 12:26:06  brianp
 * now directly call ctx->Driver.Points/Line/Triangle/QuadFunc
 *
 * Revision 1.2  1997/04/07 03:01:11  brianp
 * optimized vertex[234] code
 *
 * Revision 1.1  1997/04/02 03:14:14  brianp
 * Initial revision
 *
 */


/*
 * Render points, lines, and polygons.  The only entry point to this
 * file is the gl_render_vb() function.  This function is called after
 * the vertex buffer has filled up or glEnd() has been called.
 *
 * This file basically only makes calls to the clipping functions and
 * the point, line and triangle rasterizers via the function pointers.
 *    context->Driver.PointsFunc()
 *    context->Driver.LineFunc()
 *    context->Driver.TriangleFunc()
 */


#ifdef PC_HEADER
#include "all.h"
#else
#include "clip.h"
#include "context.h"
#include "macros.h"
#include "matrix.h"
#include "pb.h"
#include "types.h"
#include "vb.h"
#include "vbrender.h"
#include "xform.h"
#endif


/*
 * This file implements rendering of points, lines and polygons defined by
 * vertices in the vertex buffer.
 */



/*
 * Render a line segment from VB[v1] to VB[v2] when either one or both
 * endpoints must be clipped.
 */
static void render_clipped_line( GLcontext *ctx, GLuint v1, GLuint v2 )
{
   GLfloat ndc_x, ndc_y, ndc_z;
   GLuint provoking_vertex;
   struct vertex_buffer *VB = ctx->VB;

   /* which vertex dictates the color when flat shading: */
   provoking_vertex = v2;

   /*
    * Clipping may introduce new vertices.  New vertices will be stored
    * in the vertex buffer arrays starting with location VB->Free.  After
    * we've rendered the line, these extra vertices can be overwritten.
    */
   VB->Free = VB_MAX;

   /* Clip against user clipping planes */
   if (ctx->Transform.AnyClip) {
      GLuint orig_v1 = v1, orig_v2 = v2;
      if (gl_userclip_line( ctx, &v1, &v2 )==0)
	return;
      /* Apply projection matrix:  clip = Proj * eye */
      if (v1!=orig_v1) {
         TRANSFORM_POINT( VB->Clip[v1], ctx->ProjectionMatrix, VB->Eye[v1] );
      }
      if (v2!=orig_v2) {
         TRANSFORM_POINT( VB->Clip[v2], ctx->ProjectionMatrix, VB->Eye[v2] );
      }
   }

   /* Clip against view volume */
   if (gl_viewclip_line( ctx, &v1, &v2 )==0)
      return;

   /* Transform from clip coords to ndc:  ndc = clip / W */
   if (VB->Clip[v1][3] != 0.0F) {
      GLfloat wInv = 1.0F / VB->Clip[v1][3];
      ndc_x = VB->Clip[v1][0] * wInv;
      ndc_y = VB->Clip[v1][1] * wInv;
      ndc_z = VB->Clip[v1][2] * wInv;
   }
   else {
      /* Can't divide by zero, so... */
      ndc_x = ndc_y = ndc_z = 0.0F;
   }

   /* Map ndc coord to window coords. */
   VB->Win[v1][0] = ndc_x * ctx->Viewport.Sx + ctx->Viewport.Tx;
   VB->Win[v1][1] = ndc_y * ctx->Viewport.Sy + ctx->Viewport.Ty;
   VB->Win[v1][2] = ndc_z * ctx->Viewport.Sz + ctx->Viewport.Tz;

   /* Transform from clip coords to ndc:  ndc = clip / W */
   if (VB->Clip[v2][3] != 0.0F) {
      GLfloat wInv = 1.0F / VB->Clip[v2][3];
      ndc_x = VB->Clip[v2][0] * wInv;
      ndc_y = VB->Clip[v2][1] * wInv;
      ndc_z = VB->Clip[v2][2] * wInv;
   }
   else {
      /* Can't divide by zero, so... */
      ndc_x = ndc_y = ndc_z = 0.0F;
   }

   /* Map ndc coord to window coords. */
   VB->Win[v2][0] = ndc_x * ctx->Viewport.Sx + ctx->Viewport.Tx;
   VB->Win[v2][1] = ndc_y * ctx->Viewport.Sy + ctx->Viewport.Ty;
   VB->Win[v2][2] = ndc_z * ctx->Viewport.Sz + ctx->Viewport.Tz;

   if (ctx->Driver.RasterSetup) {
      /* Device driver rasterization setup */
      (*ctx->Driver.RasterSetup)( ctx, v1, v1+1 );
      (*ctx->Driver.RasterSetup)( ctx, v2, v2+1 );
   }

   START_PROFILE
   (*ctx->Driver.LineFunc)( ctx, v1, v2, provoking_vertex );
   END_PROFILE( ctx->LineTime, ctx->LineCount, 1 )
}



/*
 * Compute Z offsets for a polygon with plane defined by (A,B,C,D)
 * D is not needed.
 */
static void offset_polygon( GLcontext *ctx, GLfloat a, GLfloat b, GLfloat c )
{
   GLfloat ac, bc, m;
   GLfloat offset;

   if (c<0.001F && c>-0.001F) {
      /* to prevent underflow problems */
      offset = 0.0F;
   }
   else {
      ac = a / c;
      bc = b / c;
      if (ac<0.0F)  ac = -ac;
      if (bc<0.0F)  bc = -bc;
      m = MAX2( ac, bc );
      /* m = sqrt( ac*ac + bc*bc ); */

      offset = m * ctx->Polygon.OffsetFactor + ctx->Polygon.OffsetUnits;
   }

   ctx->PointZoffset   = ctx->Polygon.OffsetPoint ? offset : 0.0F;
   ctx->LineZoffset    = ctx->Polygon.OffsetLine  ? offset : 0.0F;
   ctx->PolygonZoffset = ctx->Polygon.OffsetFill  ? offset : 0.0F;
}



/*
 * When glPolygonMode() is used to specify that the front/back rendering
 * mode for polygons is not GL_FILL we end up calling this function.
 */
static void unfilled_polygon( GLcontext *ctx,
                              GLuint n, GLuint vlist[],
                              GLuint pv, GLuint facing )
{
   GLenum mode = facing ? ctx->Polygon.BackMode : ctx->Polygon.FrontMode;
   struct vertex_buffer *VB = ctx->VB;

   if (mode==GL_POINT) {
      GLint i, j;
      GLboolean edge;

      if (   ctx->Primitive==GL_TRIANGLES
          || ctx->Primitive==GL_QUADS
          || ctx->Primitive==GL_POLYGON) {
         edge = GL_FALSE;
      }
      else {
         edge = GL_TRUE;
      }

      for (i=0;i<n;i++) {
         j = vlist[i];
         if (edge || VB->Edgeflag[j]) {
            (*ctx->Driver.PointsFunc)( ctx, j, j );
         }
      }
   }
   else if (mode==GL_LINE) {
      GLuint i, j0, j1;
      GLboolean edge;

      ctx->StippleCounter = 0;

      if (   ctx->Primitive==GL_TRIANGLES
          || ctx->Primitive==GL_QUADS
          || ctx->Primitive==GL_POLYGON) {
         edge = GL_FALSE;
      }
      else {
         edge = GL_TRUE;
      }

      /* draw the edges */
      for (i=0;i<n;i++) {
         j0 = (i==0) ? vlist[n-1] : vlist[i-1];
         j1 = vlist[i];
         if (edge || VB->Edgeflag[j0]) {
            START_PROFILE
            (*ctx->Driver.LineFunc)( ctx, j0, j1, pv );
            END_PROFILE( ctx->LineTime, ctx->LineCount, 1 )
         }
      }
   }
   else {
      /* Fill the polygon */
      GLuint j0, i;
      j0 = vlist[0];
      for (i=2;i<n;i++) {
         START_PROFILE
         (*ctx->Driver.TriangleFunc)( ctx, j0, vlist[i-1], vlist[i], pv );
         END_PROFILE( ctx->PolygonTime, ctx->PolygonCount, 1 )
      }
   }
}


/*
 * Compute signed area of the n-sided polgyon specified by vertices vb->Win[]
 * and vertex list vlist[].
 * A clockwise polygon will return a negative area.
 * A counter-clockwise polygon will return a positive area.
 */
static GLfloat polygon_area( const struct vertex_buffer *vb,
                             GLuint n, const GLuint vlist[] )
{
   GLfloat area = 0.0F;
   GLint i;
   for (i=0;i<n;i++) {
      /* area = sum of trapezoids */
      GLuint j0 = vlist[i];
      GLuint j1 = vlist[(i+1)%n];
      GLfloat x0 = vb->Win[j0][0];
      GLfloat y0 = vb->Win[j0][1];
      GLfloat x1 = vb->Win[j1][0];
      GLfloat y1 = vb->Win[j1][1];
      GLfloat trapArea = (x0-x1)*(y0+y1);  /* Note: no divide by two here! */
      area += trapArea;
   }
   return area * 0.5F;     /* divide by two now! */
}


/*
 * Render a polygon in which doesn't have to be clipped.
 * Input:  n - number of vertices
 *         vlist - list of vertices in the polygon.
 */
static void render_polygon( GLcontext *ctx, GLuint n, GLuint vlist[] )
{
   struct vertex_buffer *VB = ctx->VB;
   GLuint pv;

   /* which vertex dictates the color when flat shading: */
   pv = (ctx->Primitive==GL_POLYGON) ? vlist[0] : vlist[n-1];

   /* Compute orientation of polygon, do cull test, offset, etc */
   {
      GLuint facing;   /* 0=front, 1=back */
      GLfloat area = polygon_area( VB, n, vlist );

      if (area==0.0F) {
         /* polygon has zero area, don't draw it */
         return;
      }

      facing = (area<0.0F) ^ (ctx->Polygon.FrontFace==GL_CW);

      if ((facing+1) & ctx->Polygon.CullBits) {
         return;   /* culled */
      }

      if (ctx->Polygon.OffsetAny) {
         /* compute plane equation of polygon, apply offset */
         GLuint j0 = vlist[0];
         GLuint j1 = vlist[1];
         GLuint j2 = vlist[2];
         GLuint j3 = vlist[ (n==3) ? 0 : 3 ];
         GLfloat ex = VB->Win[j1][0] - VB->Win[j3][0];
         GLfloat ey = VB->Win[j1][1] - VB->Win[j3][1];
         GLfloat ez = VB->Win[j1][2] - VB->Win[j3][2];
         GLfloat fx = VB->Win[j2][0] - VB->Win[j0][0];
         GLfloat fy = VB->Win[j2][1] - VB->Win[j0][1];
         GLfloat fz = VB->Win[j2][2] - VB->Win[j0][2];
         GLfloat a = ey*fz-ez*fy;
         GLfloat b = ez*fx-ex*fz;
         GLfloat c = ex*fy-ey*fx;
         offset_polygon( ctx, a, b, c );
      }

      if (ctx->LightTwoSide) {
         if (facing==1) {
            /* use back color or index */
            VB->Color = VB->Bcolor;
            VB->Index = VB->Bindex;
         }
         else {
            /* use front color or index */
            VB->Color = VB->Fcolor;
            VB->Index = VB->Findex;
         }
      }

      /* Render the polygon! */
      if (ctx->Polygon.Unfilled) {
         unfilled_polygon( ctx, n, vlist, pv, facing );
      }
      else {
         /* Draw filled polygon as a triangle fan */
         GLint i;
         GLuint j0 = vlist[0];
         for (i=2;i<n;i++) {
            START_PROFILE
            (*ctx->Driver.TriangleFunc)( ctx, j0, vlist[i-1], vlist[i], pv );
            END_PROFILE( ctx->PolygonTime, ctx->PolygonCount, 1 )
         }
      }
   }
}



/*
 * Render a polygon in which at least one vertex has to be clipped.
 * Input:  n - number of vertices
 *         vlist - list of vertices in the polygon.
 *                 CCW order = front facing.
 */
static void render_clipped_polygon( GLcontext *ctx, GLuint n, GLuint vlist[] )
{
   GLuint pv;
   struct vertex_buffer *VB = ctx->VB;
   GLfloat (*win)[3] = VB->Win;

   /* which vertex dictates the color when flat shading: */
   pv = (ctx->Primitive==GL_POLYGON) ? vlist[0] : vlist[n-1];

   /*
    * Clipping may introduce new vertices.  New vertices will be stored
    * in the vertex buffer arrays starting with location VB->Free.  After
    * we've rendered the polygon, these extra vertices can be overwritten.
    */
   VB->Free = VB_MAX;

   /* Clip against user clipping planes in eye coord space. */
   if (ctx->Transform.AnyClip) {
      GLfloat *proj = ctx->ProjectionMatrix;
      GLuint i;
      n = gl_userclip_polygon( ctx, n, vlist );
      if (n<3)
         return;
      /* Transform vertices from eye to clip coordinates:  clip = Proj * eye */
      for (i=0;i<n;i++) {
         GLuint j = vlist[i];
         TRANSFORM_POINT( VB->Clip[j], proj, VB->Eye[j] );
      }
   }

   /* Clip against view volume in clip coord space */
   n = gl_viewclip_polygon( ctx, n, vlist );
   if (n<3)
      return;

   /* Transform vertices from clip to ndc to window coords.        */
   /* ndc = clip / W    window = viewport_mapping(ndc)             */
   /* Note that window Z values are scaled to the range of integer */
   /* depth buffer values.                                         */
   {
      GLfloat sx = ctx->Viewport.Sx;
      GLfloat tx = ctx->Viewport.Tx;
      GLfloat sy = ctx->Viewport.Sy;
      GLfloat ty = ctx->Viewport.Ty;
      GLfloat sz = ctx->Viewport.Sz;
      GLfloat tz = ctx->Viewport.Tz;
      GLuint i;
      for (i=0;i<n;i++) {
         GLuint j = vlist[i];
         if (VB->Clip[j][3] != 0.0F) {
            GLfloat wInv = 1.0F / VB->Clip[j][3];
            win[j][0] = VB->Clip[j][0] * wInv * sx + tx;
            win[j][1] = VB->Clip[j][1] * wInv * sy + ty;
            win[j][2] = VB->Clip[j][2] * wInv * sz + tz;
         }
         else {
            /* Can't divide by zero, so... */
            win[j][0] = win[j][1] = win[j][2] = 0.0F;
         }
      }
      if (ctx->Driver.RasterSetup && (VB->Free > VB_MAX)) {
         /* Device driver raster setup for newly introduced vertices */
         (*ctx->Driver.RasterSetup)(ctx, VB_MAX, VB->Free);
      }
   }

   /* Compute orientation of polygon, do cull test, offset, etc */
   {
      GLuint facing;   /* 0=front, 1=back */
      GLfloat area = polygon_area( VB, n, vlist );

      if (area==0.0F) {
         /* polygon has zero area, don't draw it */
         return;
      }

      facing = (area<0.0F) ^ (ctx->Polygon.FrontFace==GL_CW);

      if ((facing+1) & ctx->Polygon.CullBits) {
         return;   /* culled */
      }

      if (ctx->Polygon.OffsetAny) {
         /* compute plane equation of polygon, apply offset */
         GLuint j0 = vlist[0];
         GLuint j1 = vlist[1];
         GLuint j2 = vlist[2];
         GLuint j3 = vlist[ (n==3) ? 0 : 3 ];
         GLfloat ex = win[j1][0] - win[j3][0];
         GLfloat ey = win[j1][1] - win[j3][1];
         GLfloat ez = win[j1][2] - win[j3][2];
         GLfloat fx = win[j2][0] - win[j0][0];
         GLfloat fy = win[j2][1] - win[j0][1];
         GLfloat fz = win[j2][2] - win[j0][2];
         GLfloat a = ey*fz-ez*fy;
         GLfloat b = ez*fx-ex*fz;
         GLfloat c = ex*fy-ey*fx;
         offset_polygon( ctx, a, b, c );
      }

      if (ctx->LightTwoSide) {
         if (facing==1) {
            /* use back color or index */
            VB->Color = VB->Bcolor;
            VB->Index = VB->Bindex;
         }
         else {
            /* use front color or index */
            VB->Color = VB->Fcolor;
            VB->Index = VB->Findex;
         }
      }

      /* Render the polygon! */
      if (ctx->Polygon.Unfilled) {
         unfilled_polygon( ctx, n, vlist, pv, facing );
      }
      else {
         /* Draw filled polygon as a triangle fan */
         GLint i;
         GLuint j0 = vlist[0];
         for (i=2;i<n;i++) {
            START_PROFILE
            (*ctx->Driver.TriangleFunc)( ctx, j0, vlist[i-1], vlist[i], pv );
            END_PROFILE( ctx->PolygonTime, ctx->PolygonCount, 1 )
         }
      }
   }
}



/*
 * Render an un-clipped triangle.
 * v0, v1, v2 - vertex indexes.  CCW order = front facing
 * pv - provoking vertex
 */
static void render_triangle( GLcontext *ctx,
                             GLuint v0, GLuint v1, GLuint v2, GLuint pv )
{
   struct vertex_buffer *VB = ctx->VB;
   GLfloat ex, ey, fx, fy, c;
   GLuint facing;  /* 0=front, 1=back */
   GLfloat (*win)[3] = VB->Win;

   /* Compute orientation of triangle */
   ex = win[v1][0] - win[v0][0];
   ey = win[v1][1] - win[v0][1];
   fx = win[v2][0] - win[v0][0];
   fy = win[v2][1] - win[v0][1];
   c = ex*fy-ey*fx;

   if (c==0.0F) {
      /* polygon is perpindicular to view plane, don't draw it */
      return;
   }

   facing = (c<0.0F) ^ (ctx->Polygon.FrontFace==GL_CW);

   if ((facing+1) & ctx->Polygon.CullBits) {
      return;   /* culled */
   }

   if (ctx->Polygon.OffsetAny) {
      /* finish computing plane equation of polygon, compute offset */
      GLfloat fz = win[v2][2] - win[v0][2];
      GLfloat ez = win[v1][2] - win[v0][2];
      GLfloat a = ey*fz-ez*fy;
      GLfloat b = ez*fx-ex*fz;
      offset_polygon( ctx, a, b, c );
   }

   if (ctx->LightTwoSide) {
      if (facing==1) {
         /* use back color or index */
         VB->Color = VB->Bcolor;
         VB->Index = VB->Bindex;
      }
      else {
         /* use front color or index */
         VB->Color = VB->Fcolor;
         VB->Index = VB->Findex;
      }
   }

   if (ctx->Polygon.Unfilled) {
      GLuint vlist[3];
      vlist[0] = v0;
      vlist[1] = v1;
      vlist[2] = v2;
      unfilled_polygon( ctx, 3, vlist, pv, facing );
   }
   else {
      START_PROFILE
      (*ctx->Driver.TriangleFunc)( ctx, v0, v1, v2, pv );
      END_PROFILE( ctx->PolygonTime, ctx->PolygonCount, 1 )
   }
}



/*
 * Render an un-clipped quadrilateral.
 * v0, v1, v2, v3 : CCW order = front facing
 * pv - provoking vertex
 */
static void render_quad( GLcontext *ctx, GLuint v0, GLuint v1,
                         GLuint v2, GLuint v3, GLuint pv )
{
   struct vertex_buffer *VB = ctx->VB;
   GLfloat ex, ey, fx, fy, c;
   GLuint facing;  /* 0=front, 1=back */
   GLfloat (*win)[3] = VB->Win;

   /* Compute polygon orientation */
   ex = win[v2][0] - win[v0][0];
   ey = win[v2][1] - win[v0][1];
   fx = win[v3][0] - win[v1][0];
   fy = win[v3][1] - win[v1][1];
   c = ex*fy-ey*fx;

   if (c==0.0F) {
      /* polygon is perpindicular to view plane, don't draw it */
      return;
   }

   facing = (c<0.0F) ^ (ctx->Polygon.FrontFace==GL_CW);

   if ((facing+1) & ctx->Polygon.CullBits) {
      return;   /* culled */
   }

   if (ctx->Polygon.OffsetAny) {
      /* finish computing plane equation of polygon, compute offset */
      GLfloat ez = win[v2][2] - win[v0][2];
      GLfloat fz = win[v3][2] - win[v1][2];
      GLfloat a = ey*fz-ez*fy;
      GLfloat b = ez*fx-ex*fz;
      offset_polygon( ctx, a, b, c );
   }

   if (ctx->LightTwoSide) {
      if (facing==1) {
         /* use back color or index */
         VB->Color = VB->Bcolor;
         VB->Index = VB->Bindex;
      }
      else {
         /* use front color or index */
         VB->Color = VB->Fcolor;
         VB->Index = VB->Findex;
      }
   }

   /* Render the quad! */
   if (ctx->Polygon.Unfilled) {
      GLuint vlist[4];
      vlist[0] = v0;
      vlist[1] = v1;
      vlist[2] = v2;
      vlist[3] = v3;
      unfilled_polygon( ctx, 4, vlist, pv, facing );
   }
   else {
      START_PROFILE
      (*ctx->Driver.QuadFunc)( ctx, v0, v1, v2, v3, pv );
      END_PROFILE( ctx->PolygonTime, ctx->PolygonCount, 2 )
   }
}



/*
 * When the vertex buffer is full, we transform/render it.  Sometimes we
 * have to copy the last vertex (or two) to the front of the vertex list
 * to "continue" the primitive.  For example:  line or triangle strips.
 * This function is a helper for that.
 */
static void copy_vertex( struct vertex_buffer *vb, GLuint dst, GLuint src )
{
   COPY_4V( vb->Clip[dst], vb->Clip[src] );
   COPY_4V( vb->Eye[dst], vb->Eye[src] );
   COPY_3V( vb->Win[dst], vb->Win[src] );
   COPY_4V( vb->Fcolor[dst], vb->Fcolor[src] );
   COPY_4V( vb->Bcolor[dst], vb->Bcolor[src] );
   COPY_4V( vb->TexCoord[dst], vb->TexCoord[src] );
   vb->Findex[dst] = vb->Findex[src];
   vb->Bindex[dst] = vb->Bindex[src];
   vb->Edgeflag[dst] = vb->Edgeflag[src];
   vb->ClipMask[dst] = vb->ClipMask[src];
}

/*Parallel_gtt_loop*/
void* parallel_gtt_f(void* arg){
	parallel_general_textured_triangle( global_ctx, global_it_count-2, global_it_count-1,global_it_count, global_it_count);
	parallel_gtt.commit();
}

/*Parallel_gtt_first_chunk*/
void* parallel_gtt_first_chunk (void* arg){
	it_finished=false;
//	START_PROFILE
	first_chunk_general_textured_triangle( global_ctx, global_it_count-2, global_it_count-1,global_it_count, global_it_count);
	parallel_gtt.commit();
}





/*
 * Either the vertex buffer is full (VB->Count==VB_MAX) or glEnd() has been
 * called.  Render the primitives defined by the vertices and reset the
 * buffer.
 *
 * This function won't be called if the device driver implements a
 * RenderVB() function.  If the device driver renders the vertex buffer
 * then the driver must also call gl_reset_vb()!
 *
 * Input:  allDone - GL_TRUE = caller is glEnd()
 *                   GL_FALSE = calling because buffer is full.
 */
void gl_render_vb( GLcontext *ctx, GLboolean allDone )
{
   struct vertex_buffer *VB = ctx->VB;
   GLuint vlist[VB_SIZE];

   switch (ctx->Primitive) {
      case GL_POINTS:
         START_PROFILE
         (*ctx->Driver.PointsFunc)( ctx, 0, VB->Count-1 );
         END_PROFILE( ctx->PointTime, ctx->PointCount, VB->Count )
	 break;

      case GL_LINES:
         if (VB->ClipOrMask) {
            GLuint i;
            for (i=1;i<VB->Count;i+=2) {
               if (VB->ClipMask[i-1] | VB->ClipMask[i]) {
                  render_clipped_line( ctx, i-1, i );
               }
               else {
                  START_PROFILE
                  (*ctx->Driver.LineFunc)( ctx, i-1, i, i );
                  END_PROFILE( ctx->LineTime, ctx->LineCount, 1 )
               }
               ctx->StippleCounter = 0;
            }
         }
         else {
            GLuint i;
            for (i=1;i<VB->Count;i+=2) {
               START_PROFILE
               (*ctx->Driver.LineFunc)( ctx, i-1, i, i );
               END_PROFILE( ctx->LineTime, ctx->LineCount, 1 )
               ctx->StippleCounter = 0;
            }
         }
	 break;

      case GL_LINE_STRIP:
         if (VB->ClipOrMask) {
            GLuint i;
	    for (i=1;i<VB->Count;i++) {
               if (VB->ClipMask[i-1] | VB->ClipMask[i]) {
                  render_clipped_line( ctx, i-1, i );
               }
               else {
                  START_PROFILE
                  (*ctx->Driver.LineFunc)( ctx, i-1, i, i );
                  END_PROFILE( ctx->LineTime, ctx->LineCount, 1 )
               }
	    }
         }
         else {
            /* no clipping needed */
            GLuint i;
	    for (i=1;i<VB->Count;i++) {
               START_PROFILE
               (*ctx->Driver.LineFunc)( ctx, i-1, i, i );
               END_PROFILE( ctx->LineTime, ctx->LineCount, 1 )
            }
         }
         break;

      case GL_LINE_LOOP:
         {
            GLuint i;
            if (VB->Start==0) {
               i = 1;  /* start at 0th vertex */
            }
            else {
               i = 2;  /* skip first vertex, we're saving it until glEnd */
            }
            while (i<VB->Count) {
               if (VB->ClipMask[i-1] | VB->ClipMask[i]) {
                  render_clipped_line( ctx, i-1, i );
               }
               else {
                  START_PROFILE
                  (*ctx->Driver.LineFunc)( ctx, i-1, i, i );
                  END_PROFILE( ctx->LineTime, ctx->LineCount, 1 )
               }
               i++;
            }
         }
         break;

      case GL_TRIANGLES:
         if (VB->ClipOrMask) {
            GLuint i;
            for (i=2;i<VB->Count;i+=3) {
               if (VB->ClipMask[i-2] | VB->ClipMask[i-1] | VB->ClipMask[i]) {
                  vlist[0] = i-2;
                  vlist[1] = i-1;
                  vlist[2] = i-0;
                  render_clipped_polygon( ctx, 3, vlist );
               }
               else {
                  if (ctx->DirectTriangles) {
                     START_PROFILE
                     (*ctx->Driver.TriangleFunc)( ctx, i-2, i-1, i, i );
                     END_PROFILE( ctx->PolygonTime, ctx->PolygonCount, 1 )
                  }
                  else {
                     render_triangle( ctx, i-2, i-1, i, i );
                  }
               }
            }
         }
         else {
            /* no clipping needed */
            GLuint i;
            if (ctx->DirectTriangles) {
               for (i=2;i<VB->Count;i+=3) {
                  START_PROFILE
                  (*ctx->Driver.TriangleFunc)( ctx, i-2, i-1, i, i );
                  END_PROFILE( ctx->PolygonTime, ctx->PolygonCount, 1 )
               }
            }
            else {
               for (i=2;i<VB->Count;i+=3) {
                  render_triangle( ctx, i-2, i-1, i, i );
               }
            }
         }
	 break;

      case GL_TRIANGLE_STRIP:
         if (VB->ClipOrMask) {
            GLuint i;
            for (i=2;i<VB->Count;i++) {
               if (VB->ClipMask[i-2] | VB->ClipMask[i-1] | VB->ClipMask[i]) {
                  if (i&1) {
                     /* reverse vertex order */
                     vlist[0] = i-1;
                     vlist[1] = i-2;
                     vlist[2] = i-0;
                     render_clipped_polygon( ctx, 3, vlist );
                  }
                  else {
                     vlist[0] = i-2;
                     vlist[1] = i-1;
                     vlist[2] = i-0;
                     render_clipped_polygon( ctx, 3, vlist );
                  }
               }
               else {
                  if (ctx->DirectTriangles) {
                     START_PROFILE
                     (*ctx->Driver.TriangleFunc)( ctx, i-2, i-1, i, i );
                     END_PROFILE( ctx->PolygonTime, ctx->PolygonCount, 1 )
                  }
                  else {
                     if (i&1)
                        render_triangle( ctx, i, i-1, i-2, i );
                     else
                        render_triangle( ctx, i-2, i-1, i, i );
                  }
               }
            }
         }
         else {
            /* no vertices were clipped...*/
            GLuint i;
            if (ctx->DirectTriangles) {
		  global_ctx=ctx;
		  global_it_count=2;
		  global_max_it=VB->Count;
		  script_vector input_functions;
		  input_functions.push_back(parallel_gtt_first_chunk);
		  vector <void*> input_vars;
  		  input_vars.push_back((void*)0); //Input for first iteration
		  parallel_gtt.run(input_functions, input_vars);
		  parallel_gtt.commit();
		  parallel_gtt.get_results();
		

		//SON MAS O MENOS 298 repeticiones... La idea sería solo invocar a la primera iteración aquí, quizás mandandole el limite, de modo que cada iteración reciba su i y el limite
          //     for (i=2;i<VB->Count;i++) {
//                  START_PROFILE
	//	    general_textured_triangle (ctx, i-2, i-1, i, i ); //PROBANDO...
  //                END_PROFILE( ctx->PolygonTime, ctx->PolygonCount, 1 )
		 
            //   }
            }
            else {
               for (i=2;i<VB->Count;i++) {
                  if (i&1)
                     render_triangle( ctx, i, i-1, i-2, i );
                  else
                     render_triangle( ctx, i-2, i-1, i, i );
               }
            }
         }
	 break;

      case GL_TRIANGLE_FAN:
         if (VB->ClipOrMask) {
            GLuint i;
            for (i=2;i<VB->Count;i++) {
               if (VB->ClipMask[0] | VB->ClipMask[i-1] | VB->ClipMask[i]) {
                  vlist[0] = 0;
                  vlist[1] = i-1;
                  vlist[2] = i;
                  render_clipped_polygon( ctx, 3, vlist );
               }
               else {
                  if (ctx->DirectTriangles) {
                     START_PROFILE
                     (*ctx->Driver.TriangleFunc)( ctx, 0, i-1, i, i );
                     END_PROFILE( ctx->PolygonTime, ctx->PolygonCount, 1 )
                  }
                  else {
                     render_triangle( ctx, 0, i-1, i, i );
                  }
               }
            }
         }
         else {
            /* no clipping needed */
            GLuint i;
            if (ctx->DirectTriangles) {
               for (i=2;i<VB->Count;i++) {
                  START_PROFILE
                  (*ctx->Driver.TriangleFunc)( ctx, 0, i-1, i, i );
                  END_PROFILE( ctx->PolygonTime, ctx->PolygonCount, 1 )
               }
            }
            else {
               for (i=2;i<VB->Count;i++) {
                  render_triangle( ctx, 0, i-1, i, i );
               }
            }
         }
	 break;

      case GL_QUADS:
         if (VB->ClipOrMask) {
            GLuint i;
            for (i=3;i<VB->Count;i+=4) {
               if (  VB->ClipMask[i-3] | VB->ClipMask[i-2]
                   | VB->ClipMask[i-1] | VB->ClipMask[i]) {
                  vlist[0] = i-3;
                  vlist[1] = i-2;
                  vlist[2] = i-1;
                  vlist[3] = i-0;
                  render_clipped_polygon( ctx, 4, vlist );
               }
               else {
                  if (ctx->DirectTriangles) {
                     START_PROFILE
                     (*ctx->Driver.QuadFunc)( ctx, i-3, i-2, i-1, i, i );
                     END_PROFILE( ctx->PolygonTime, ctx->PolygonCount, 2 )
                  }
                  else {
                     render_quad( ctx, i-3, i-2, i-1, i, i );
                  }
               }
            }
         }
         else {
            /* no vertices were clipped */
            GLuint i;
            if (ctx->DirectTriangles) {
               for (i=3;i<VB->Count;i+=4) {
                  START_PROFILE
                  (*ctx->Driver.QuadFunc)( ctx, i-3, i-2, i-1, i, i );
                  END_PROFILE( ctx->PolygonTime, ctx->PolygonCount, 2 )
               }
            }
            else {
               for (i=3;i<VB->Count;i+=4) {
                  render_quad( ctx, i-3, i-2, i-1, i, i );
               }
            }
         }
	 break;

      case GL_QUAD_STRIP:
         if (VB->ClipOrMask) {
            GLuint i;
            for (i=3;i<VB->Count;i+=2) {
               if (  VB->ClipMask[i-2] | VB->ClipMask[i-3]
                   | VB->ClipMask[i-1] | VB->ClipMask[i]) {
                  vlist[0] = i-1;
                  vlist[1] = i-3;
                  vlist[2] = i-2;
                  vlist[3] = i-0;
                  render_clipped_polygon( ctx, 4, vlist );
               }
               else {
                  if (ctx->DirectTriangles) {
                     START_PROFILE
                     (*ctx->Driver.QuadFunc)( ctx, i-3, i-2, i, i-1, i );
                     END_PROFILE( ctx->PolygonTime, ctx->PolygonCount, 2 )
                  }
                  else {
                     render_quad( ctx, i-3, i-2, i, i-1, i );
                  }
               }
            }
         }
         else {
            /* no clipping needed */
            GLuint i;
            if (ctx->DirectTriangles) {
               for (i=3;i<VB->Count;i+=2) {
                  START_PROFILE
                  (*ctx->Driver.QuadFunc)( ctx, i-3, i-2, i, i-1, i );
                  END_PROFILE( ctx->PolygonTime, ctx->PolygonCount, 2 )
               }
            }
            else {
               for (i=3;i<VB->Count;i+=2) {
                  render_quad( ctx, i-3, i-2, i, i-1, i );
               }
            }
         }
	 break;

      case GL_POLYGON:
         if (VB->Count>2) {
            GLuint i;
            for (i=0;i<VB->Count;i++) {
               vlist[i] = i;
            }
            if (VB->ClipOrMask) {
               render_clipped_polygon( ctx, VB->Count, vlist );
            }
            else {
               render_polygon( ctx, VB->Count, vlist );
            }
         }
	 break;

      default:
         /* should never get here */
         gl_problem( ctx, "invalid mode in gl_render_vb" );
   }

   gl_reset_vb( ctx, allDone );
}


#define CLIP_ALL_BITS    0x3f


/*
 * After we've rendered the primitives in the vertex buffer we call
 * this function to reset the vertex buffer.  That is, we prepare it
 * for the next batch of vertices.
 * Input:  ctx - the context
 *         allDone - GL_TRUE = glEnd() was called
 *                   GL_FALSE = buffer was filled, more vertices to come
 */
void gl_reset_vb( GLcontext *ctx, GLboolean allDone )
{
   struct vertex_buffer *VB = ctx->VB;

   if (ctx->Primitive==GL_LINE_LOOP && allDone) {
      /* special case */
      if (VB->ClipMask[VB->Count-1] | VB->ClipMask[0]) {
         render_clipped_line( ctx, VB->Count-1, 0 );
      }
      else {
         START_PROFILE
            (*ctx->Driver.LineFunc)( ctx, VB->Count-1, 0, 0 );
         END_PROFILE( ctx->LineTime, ctx->LineCount, 1 )
      }
   }

   if (VB->ClipOrMask) {
      /* reset clip masks to zero */
      MEMSET( VB->ClipMask + VB->Start, 0,
              (VB->Count - VB->Start) * sizeof(VB->ClipMask[0]) );
   }

   if (!VB->MonoMaterial) {
      /* reset material masks to zero */
      MEMSET( VB->MaterialMask + VB->Start, 0,
              (VB->Count - VB->Start) * sizeof(VB->MaterialMask[0]) );
   }

   if (VB->VertexSizeMask!=VERTEX3_BIT) {
      /* reset object W coords to one */
      GLint i, n;
      GLfloat (*obj)[4] = VB->Obj + VB->Start;
      n = VB->Count - VB->Start;
      for (i=0; i<n; i++) {
         obj[i][3] = 1.0F;
      }
   }

   if (allDone) {
      VB->MonoColor = GL_TRUE;
      VB->VertexSizeMask = VERTEX3_BIT;
      if (VB->TexCoordSize!=2) {
         GLint i, n = VB->Count;
         for (i=0;i<n;i++) {
            VB->TexCoord[i][2] = 0.0F;
            VB->TexCoord[i][3] = 1.0F;
         }
      }
      if (ctx->Current.TexCoord[2]==0.0F && ctx->Current.TexCoord[3]==1.0F) {
         VB->TexCoordSize = 2;
      }
      else {
         VB->TexCoordSize = 4;
      }
   }

   switch (ctx->Primitive) {
      case GL_POINTS:
         ASSERT(VB->Start==0);
	 VB->Count = 0;
         VB->ClipOrMask = 0;
         VB->ClipAndMask = CLIP_ALL_BITS;
         VB->MonoMaterial = GL_TRUE;
         VB->MonoNormal = GL_TRUE;
	 break;

      case GL_LINES:
         ASSERT(VB->Start==0);
	 VB->Count = 0;
         VB->ClipOrMask = 0;
         VB->ClipAndMask = CLIP_ALL_BITS;
         VB->MonoMaterial = GL_TRUE;
         VB->MonoNormal = GL_TRUE;
	 break;

      case GL_LINE_STRIP:
         if (allDone) {
            VB->Count = 0;
            VB->ClipOrMask = 0;
            VB->ClipAndMask = CLIP_ALL_BITS;
            VB->MonoMaterial = GL_TRUE;
            VB->MonoNormal = GL_TRUE;
	 }
         else {
            copy_vertex( VB, 0, VB->Count-1 );  /* copy last vertex to front */
            VB->Count = 1;
            VB->ClipOrMask = VB->ClipMask[0];
            VB->ClipAndMask = VB->ClipMask[0];
            VB->MonoMaterial = VB->MaterialMask[0] ? GL_FALSE : GL_TRUE;
         }
         break;

      case GL_LINE_LOOP:
	 if (allDone) {
            VB->Count = 0;
            VB->ClipOrMask = 0;
            VB->ClipAndMask = CLIP_ALL_BITS;
            VB->MonoMaterial = GL_TRUE;
            VB->MonoNormal = GL_TRUE;
	 }
	 else {
	    ASSERT(VB->Count==VB_MAX);
	    /* recycle the vertex list */
            copy_vertex( VB, 1, VB_MAX-1 );
	    VB->Count = 2;
            VB->ClipOrMask = VB->ClipMask[0] | VB->ClipMask[1];
            VB->ClipAndMask = VB->ClipMask[0] & VB->ClipMask[1];
            VB->MonoMaterial = !(VB->MaterialMask[0] | VB->MaterialMask[1]);
	 }
         break;

      case GL_TRIANGLES:
         ASSERT(VB->Start==0);
	 VB->Count = 0;
         VB->ClipOrMask = 0;
         VB->ClipAndMask = CLIP_ALL_BITS;
         VB->MonoMaterial = GL_TRUE;
         VB->MonoNormal = GL_TRUE;
	 break;

      case GL_TRIANGLE_STRIP:
         if (allDone) {
            VB->Count = 0;
            VB->ClipOrMask = 0;
            VB->ClipAndMask = CLIP_ALL_BITS;
            VB->MonoMaterial = GL_TRUE;
            VB->MonoNormal = GL_TRUE;
         }
         else {
            /* get ready for more vertices in this triangle strip */
            copy_vertex( VB, 0, VB_MAX-2 );
            copy_vertex( VB, 1, VB_MAX-1 );
            VB->Count = 2;
            VB->ClipOrMask = VB->ClipMask[0] | VB->ClipMask[1];
            VB->ClipAndMask = VB->ClipMask[0] & VB->ClipMask[1];
            VB->MonoMaterial = !(VB->MaterialMask[0] | VB->MaterialMask[1]);
         }
	 break;

      case GL_TRIANGLE_FAN:
         if (allDone) {
            VB->Count = 0;
            VB->ClipOrMask = 0;
            VB->ClipAndMask = CLIP_ALL_BITS;
            VB->MonoMaterial = GL_TRUE;
            VB->MonoNormal = GL_TRUE;
         }
         else {
            /* get ready for more vertices in this triangle fan */
            copy_vertex( VB, 1, VB_MAX-1 );
            VB->Count = 2;
            VB->ClipOrMask = VB->ClipMask[0] | VB->ClipMask[1];
            VB->ClipAndMask = VB->ClipMask[0] & VB->ClipMask[1];
            VB->MonoMaterial = !(VB->MaterialMask[0] | VB->MaterialMask[1]);
	 }
	 break;

      case GL_QUADS:
         ASSERT(VB->Start==0);
	 VB->Count = 0;
         VB->ClipOrMask = 0;
         VB->ClipAndMask = CLIP_ALL_BITS;
         VB->MonoMaterial = GL_TRUE;
         VB->MonoNormal = GL_TRUE;
	 break;

      case GL_QUAD_STRIP:
         if (allDone) {
            VB->Count = 0;
            VB->ClipOrMask = 0;
            VB->ClipAndMask = CLIP_ALL_BITS;
            VB->MonoMaterial = GL_TRUE;
            VB->MonoNormal = GL_TRUE;
         }
         else {
            /* get ready for more vertices in this quad strip */
            copy_vertex( VB, 0, VB_MAX-2 );
            copy_vertex( VB, 1, VB_MAX-1 );
            VB->Count = 2;
            VB->ClipOrMask = VB->ClipMask[0] | VB->ClipMask[1];
            VB->ClipAndMask = VB->ClipMask[0] & VB->ClipMask[1];
            VB->MonoMaterial = !(VB->MaterialMask[0] | VB->MaterialMask[1]);
         }
	 break;

      case GL_POLYGON:
         if (allDone) {
            VB->Count = 0;
            VB->ClipOrMask = 0;
            VB->ClipAndMask = CLIP_ALL_BITS;
            VB->MonoMaterial = GL_TRUE;
            VB->MonoNormal = GL_TRUE;
         }
         else {
            /* get ready for more vertices just like a triangle fan */
            copy_vertex( VB, 1, VB_MAX-1 );
            VB->Count = 2;
            VB->ClipOrMask = VB->ClipMask[0] | VB->ClipMask[1];
            VB->ClipAndMask = VB->ClipMask[0] & VB->ClipMask[1];
            VB->MonoMaterial = !(VB->MaterialMask[0] | VB->MaterialMask[1]);
	 }
	 break;

      default:
         /* should never get here */
         gl_problem( ctx, "invalid mode in gl_reset_vb" );
   }

   /* In any case, Start = first vertex which hasn't been transformed yet */
   VB->Start = VB->Count;
}


