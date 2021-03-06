#!/bin/bash

g++ -c -o accum.o              -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/accum.c -lpthread
g++ -c -o alpha.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/alpha.c -lpthread
g++ -c -o alphabuf.o             -O3  /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/alphabuf.c -lpthread
g++ -c -o api1.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/api1.c -lpthread
g++ -c -o api2.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/api2.c -lpthread
g++ -c -o attrib.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/attrib.c -lpthread
g++ -c -o bitmap.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/bitmap.c -lpthread
g++ -c -o blend.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/blend.c -lpthread
g++ -c -o clip.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/clip.c -lpthread
g++ -c -o colortab.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/colortab.c -lpthread
g++ -c -o context.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/context.c -lpthread
g++ -c -o copypix.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/copypix.c -lpthread
g++ -c -o dlist.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/dlist.c -lpthread
g++ -c -o drawpix.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/drawpix.c -lpthread
g++ -c -o enable.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/enable.c -lpthread
g++ -c -o eval.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/eval.c -lpthread
g++ -c -o feedback.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/feedback.c -lpthread
g++ -c -o fog.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/fog.c -lpthread
g++ -c -o get.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/get.c -lpthread
g++ -c -o hash.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/hash.c -lpthread
g++ -c -o image.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/image.c -lpthread
g++ -c -o light.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/light.c -lpthread
g++ -c -o lines.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/lines.c -lpthread
g++ -c -o logic.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/logic.c -lpthread
g++ -c -o masking.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/masking.c -lpthread
g++ -c -o matrix.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/matrix.c -lpthread
g++ -c -o misc.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/misc.c -lpthread
g++ -c -o mmath.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/mmath.c -lpthread
g++ -c -o osmesa.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/osmesa.c -lpthread
g++ -c -o pb.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/pb.c -lpthread
g++ -c -o pixel.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/pixel.c -lpthread
g++ -c -o pointers.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/pointers.c -lpthread
g++ -c -o points.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/points.c -lpthread
g++ -c -o polygon.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/polygon.c -lpthread
g++ -c -o quads.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/quads.c -lpthread
g++ -c -o rastpos.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/rastpos.c -lpthread
g++ -c -o readpix.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/readpix.c -lpthread
g++ -c -o rect.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/rect.c -lpthread
g++ -c -o scissor.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/scissor.c -lpthread
g++ -c -o shade.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/shade.c -lpthread
g++ -c -o stencil.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/stencil.c -lpthread
g++ -c -o teximage.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/teximage.c -lpthread
g++ -c -o texobj.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/texobj.c -lpthread
g++ -c -o texstate.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/texstate.c -lpthread
g++ -c -o texture.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/texture.c -lpthread
g++ -c -o varray.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/varray.c -lpthread
g++ -c -o vb.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/vb.c -lpthread
g++ -c -o vbfill.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/vbfill.c -lpthread
g++ -c -o vbrender.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/vbrender_bckup.c -lpthread
g++ -c -o vbxform.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/vbxform.c -lpthread
g++ -c -o winpos.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/winpos.c -lpthread
g++ -c -o xform.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/xform.c -lpthread
g++ -c -o mesa4.o               -O3 /home/gabriel/Escritorio/SPEC_CPU2000_CD_2/benchspec/CFP2000/177.mesa/src/mesa4.c -lpthread
g++        -O3 accum.o alpha.o alphabuf.o api1.o api2.o attrib.o bitmap.o blend.o clip.o colortab.o context.o copypix.o dlist.o drawpix.o enable.o eval.o feedback.o fog.o get.o hash.o image.o light.o lines.o logic.o masking.o matrix.o misc.o mmath.o osmesa.o pb.o pixel.o pointers.o points.o polygon.o quads.o rastpos.o readpix.o rect.o scissor.o shade.o stencil.o teximage.o texobj.o texstate.o texture.o varray.o vb.o vbfill.o vbrender.o vbxform.o winpos.o xform.o mesa4.o   -lm  -o mesa -lpthread

