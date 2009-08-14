//===========================================================================
//
// File: mxgrdecl.c
//
// Created: Fri Jun 19 08:48:21 2009
//
// Author: Jostein R. Natvig <Jostein.R.Natvig@sintef.no>
//
// $Date$
//
// $Revision$
//
//===========================================================================

/*
Copyright 2009 SINTEF ICT, Applied Mathematics.
Copyright 2009 Statoil ASA.

This file is part of The Open Reservoir Simulator Project (OpenRS).

OpenRS is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or 
(at your option) any later version.

OpenRS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenRS.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include <mex.h>


#include "grdecl.h"



/* Get COORD, ZCORN, ACTNUM and DIMS from mxArray.       */
/*-------------------------------------------------------*/
void mx_init_grdecl(struct grdecl *g, const mxArray *s)
{
  int i,n;
  mxArray *field;
  int numel;

  field = mxGetField(s, 0, "cartDims");
  numel = mxGetNumberOfElements(field);
  double *tmp = mxGetPr(field);
  if (numel != 3){
    mexErrMsgTxt("cartDims field must be 3 numbers");
  }

  n = 1;
  for (i=0; i<3; ++i){
    g->dims[i] = tmp[i];
    n      *= tmp[i];
  }


  field = mxGetField(s, 0, "ACTNUM");
  numel = mxGetNumberOfElements(field);
  if (mxGetClassID(field) != mxINT32_CLASS ||
      numel != g->dims[0]*g->dims[1]*g->dims[2] ){
    mexErrMsgTxt("ACTNUM field must be nx*ny*nz numbers int32");
  }
  g->actnum = mxGetData(field);


  
  field = mxGetField(s, 0, "COORD");
  numel = mxGetNumberOfElements(field);
  if (mxGetClassID(field) != mxDOUBLE_CLASS || 
      numel != 6*(g->dims[0]+1)*(g->dims[1]+1)){
    mexErrMsgTxt("COORD field must have 6*(nx+1)*(ny+1) doubles.");
  }
  g->coord = mxGetPr(field);
  

  field = mxGetField(s, 0, "ZCORN");
  numel = mxGetNumberOfElements(field);
  if (mxGetClassID(field) != mxDOUBLE_CLASS || 
      numel != 8*g->dims[0]*g->dims[1]*g->dims[2]){
    mexErrMsgTxt("ZCORN field must have 8*nx*ny*nz doubles.");
  }
  g->zcorn = mxGetPr(field);
}