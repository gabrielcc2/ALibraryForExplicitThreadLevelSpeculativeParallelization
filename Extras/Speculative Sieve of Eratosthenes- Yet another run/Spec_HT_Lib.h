//
//  Spec_HT_Lib
//  	Explicit Models to use Helper Threads as a mean to extract speculative
//      parallelism
//
//  File    : Spec_HT_Lib.h
//  Author  : Gabriel Campero
//  	    : Universidad de Los Andes, Ing. de Sist. Merida- Venezuela.
//  Purpose : Collection of classes offering explicit uses of speculative HT (helper-.
//            threading) for the extraction of parallelism.
//
//  Classes included: ht_speculator.
//
// Limitations: 
//
// * Sequential consistency is mostly guaranteed, save for exception 
// behaviour.
//
#include "ht_speculator.h"
