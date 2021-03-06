/*
  Copyright 2012 SINTEF ICT, Applied Mathematics.

  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "config.h"

#include <opm/core/grid/GridManager.hpp>
#include <opm/core/grid.h>
#include <opm/core/grid/cart_grid.h>
#include <opm/core/grid/cornerpoint_grid.h>
#include <opm/core/utility/ErrorMacros.hpp>

#include <array>
#include <algorithm>
#include <numeric>

namespace Opm
{
    /// Construct a 3d corner-point grid from a deck.
    GridManager::GridManager(Opm::EclipseGridConstPtr eclipseGrid) {
        struct grdecl g;
        std::vector<int> actnum;
        std::vector<double> coord;
        std::vector<double> zcorn;
        std::vector<double> mapaxes;
        
        g.dims[0] = eclipseGrid->getNX();
        g.dims[1] = eclipseGrid->getNY();
        g.dims[2] = eclipseGrid->getNZ();

        eclipseGrid->exportMAPAXES( mapaxes );
        eclipseGrid->exportCOORD( coord );
        eclipseGrid->exportZCORN( zcorn );
        eclipseGrid->exportACTNUM( actnum );   

        g.coord = coord.data();
        g.zcorn = zcorn.data();
        g.actnum = actnum.data();
        g.mapaxes = mapaxes.data();

        ug_ = create_grid_cornerpoint(&g , 0.0);
        if (!ug_) {
            OPM_THROW(std::runtime_error, "Failed to construct grid.");
        }
    }


    GridManager::GridManager(Opm::DeckConstPtr deck)
    {
        // We accept two different ways to specify the grid.
        //    1. Corner point format.
        //       Requires ZCORN, COORDS, DIMENS or SPECGRID, optionally
        //       ACTNUM, optionally MAPAXES.
        //       For this format, we will verify that DXV, DYV, DZV,
        //       DEPTHZ and TOPS are not present.
        //    2. Tensor grid format.
        //       Requires DXV, DYV, DZV, optionally DEPTHZ or TOPS.
        //       For this format, we will verify that ZCORN, COORDS
        //       and ACTNUM are not present.
        //       Note that for TOPS, we only allow a uniform vector of values.

        if (deck->hasKeyword("ZCORN") && deck->hasKeyword("COORD")) {
            initFromDeckCornerpoint(deck);
        } else if (deck->hasKeyword("DXV") && deck->hasKeyword("DYV") && deck->hasKeyword("DZV")) {
            initFromDeckTensorgrid(deck);
        } else {
            OPM_THROW(std::runtime_error, "Could not initialize grid from deck. "
                      "Need either ZCORN + COORD or DXV + DYV + DZV keywords.");
        }
    }



    /// Construct a 2d cartesian grid with cells of unit size.
    GridManager::GridManager(int nx, int ny)
    {
        ug_ = create_grid_cart2d(nx, ny, 1.0, 1.0);
        if (!ug_) {
            OPM_THROW(std::runtime_error, "Failed to construct grid.");
        }
    }

    GridManager::GridManager(int nx, int ny,double dx, double dy)
    {
        ug_ = create_grid_cart2d(nx, ny, dx, dy);
        if (!ug_) {
            OPM_THROW(std::runtime_error, "Failed to construct grid.");
        }
    }


    /// Construct a 3d cartesian grid with cells of unit size.
    GridManager::GridManager(int nx, int ny, int nz)
    {
        ug_ = create_grid_cart3d(nx, ny, nz);
        if (!ug_) {
            OPM_THROW(std::runtime_error, "Failed to construct grid.");
        }
    }




    /// Construct a 3d cartesian grid with cells of size [dx, dy, dz].
    GridManager::GridManager(int nx, int ny, int nz,
                             double dx, double dy, double dz)
    {
        ug_ = create_grid_hexa3d(nx, ny, nz, dx, dy, dz);
        if (!ug_) {
            OPM_THROW(std::runtime_error, "Failed to construct grid.");
        }
    }




    /// Construct a grid from an input file.
    /// The file format used is currently undocumented,
    /// and is therefore only suited for internal use.
    GridManager::GridManager(const std::string& input_filename)
    {
        ug_ = read_grid(input_filename.c_str());
        if (!ug_) {
            OPM_THROW(std::runtime_error, "Failed to read grid from file " << input_filename);
        }
    }

    /// Destructor.
    GridManager::~GridManager()
    {
        destroy_grid(ug_);
    }




    /// Access the managed UnstructuredGrid.
    /// The method is named similarly to c_str() in std::string,
    /// to make it clear that we are returning a C-compatible struct.
    const UnstructuredGrid* GridManager::c_grid() const
    {
        return ug_;
    }

    // Construct corner-point grid from deck.
    void GridManager::initFromDeckCornerpoint(Opm::DeckConstPtr deck)
    {
        // Extract data from deck.
        // Collect in input struct for preprocessing.
        struct grdecl grdecl;
        createGrdecl(deck, grdecl);

        // Process grid.
        ug_ = create_grid_cornerpoint(&grdecl, 0.0);
        if (!ug_) {
            OPM_THROW(std::runtime_error, "Failed to construct grid.");
        }
    }

    void GridManager::createGrdecl(Opm::DeckConstPtr deck, struct grdecl &grdecl)
    {
        // Extract data from deck.
        const std::vector<double>& zcorn = deck->getKeyword("ZCORN")->getSIDoubleData();
        const std::vector<double>& coord = deck->getKeyword("COORD")->getSIDoubleData();
        const int* actnum = NULL;
        if (deck->hasKeyword("ACTNUM")) {
            actnum = &(deck->getKeyword("ACTNUM")->getIntData()[0]);
        }

        std::array<int, 3> dims;
        if (deck->hasKeyword("DIMENS")) {
            Opm::DeckKeywordConstPtr dimensKeyword = deck->getKeyword("DIMENS");
            dims[0] = dimensKeyword->getRecord(0)->getItem(0)->getInt(0);
            dims[1] = dimensKeyword->getRecord(0)->getItem(1)->getInt(0);
            dims[2] = dimensKeyword->getRecord(0)->getItem(2)->getInt(0);
        } else if (deck->hasKeyword("SPECGRID")) {
            Opm::DeckKeywordConstPtr specgridKeyword = deck->getKeyword("SPECGRID");
            dims[0] = specgridKeyword->getRecord(0)->getItem(0)->getInt(0);
            dims[1] = specgridKeyword->getRecord(0)->getItem(1)->getInt(0);
            dims[2] = specgridKeyword->getRecord(0)->getItem(2)->getInt(0);
        } else {
            OPM_THROW(std::runtime_error, "Deck must have either DIMENS or SPECGRID.");
        }

        // Collect in input struct for preprocessing.

        grdecl.zcorn = &zcorn[0];
        grdecl.coord = &coord[0];
        grdecl.actnum = actnum;
        grdecl.dims[0] = dims[0];
        grdecl.dims[1] = dims[1];
        grdecl.dims[2] = dims[2];

        if (deck->hasKeyword("MAPAXES")) {
            Opm::DeckKeywordConstPtr mapaxesKeyword = deck->getKeyword("MAPAXES");
            Opm::DeckRecordConstPtr mapaxesRecord = mapaxesKeyword->getRecord(0);

            // memleak alert: here we need to make sure that C code
            // can properly take ownership of the grdecl.mapaxes
            // object. if it is not freed, it will result in a
            // memleak...
            double *cWtfMapaxes = static_cast<double*>(malloc(sizeof(double)*mapaxesRecord->size()));
            for (unsigned i = 0; i < mapaxesRecord->size(); ++i)
                cWtfMapaxes[i] = mapaxesRecord->getItem(i)->getSIDouble(0);
            grdecl.mapaxes = cWtfMapaxes;
        } else
            grdecl.mapaxes = NULL;

    }

    namespace
    {
        std::vector<double> coordsFromDeltas(const std::vector<double>& deltas)
        {
            std::vector<double> coords(deltas.size() + 1);
            coords[0] = 0.0;
            std::partial_sum(deltas.begin(), deltas.end(), coords.begin() + 1);
            return coords;
        }
    } // anonymous namespace


    void GridManager::initFromDeckTensorgrid(Opm::DeckConstPtr deck)
    {
        // Extract logical cartesian size.
        std::array<int, 3> dims;
        if (deck->hasKeyword("DIMENS")) {
            Opm::DeckKeywordConstPtr dimensKeyword = deck->getKeyword("DIMENS");
            dims[0] = dimensKeyword->getRecord(0)->getItem(0)->getInt(0);
            dims[1] = dimensKeyword->getRecord(0)->getItem(1)->getInt(0);
            dims[2] = dimensKeyword->getRecord(0)->getItem(2)->getInt(0);
        } else if (deck->hasKeyword("SPECGRID")) {
            Opm::DeckKeywordConstPtr specgridKeyword = deck->getKeyword("SPECGRID");
            dims[0] = specgridKeyword->getRecord(0)->getItem(0)->getInt(0);
            dims[1] = specgridKeyword->getRecord(0)->getItem(1)->getInt(0);
            dims[2] = specgridKeyword->getRecord(0)->getItem(2)->getInt(0);
        } else {
            OPM_THROW(std::runtime_error, "Deck must have either DIMENS or SPECGRID.");
        }

        // Extract coordinates (or offsets from top, in case of z).
        const std::vector<double>& dxv = deck->getKeyword("DXV")->getSIDoubleData();
        const std::vector<double>& dyv = deck->getKeyword("DYV")->getSIDoubleData();
        const std::vector<double>& dzv = deck->getKeyword("DZV")->getSIDoubleData();
        std::vector<double> x = coordsFromDeltas(dxv);
        std::vector<double> y = coordsFromDeltas(dyv);
        std::vector<double> z = coordsFromDeltas(dzv);

        // Check that number of cells given are consistent with DIMENS/SPECGRID.
        if (dims[0] != int(dxv.size())) {
            OPM_THROW(std::runtime_error, "Number of DXV data points do not match DIMENS or SPECGRID.");
        }
        if (dims[1] != int(dyv.size())) {
            OPM_THROW(std::runtime_error, "Number of DYV data points do not match DIMENS or SPECGRID.");
        }
        if (dims[2] != int(dzv.size())) {
            OPM_THROW(std::runtime_error, "Number of DZV data points do not match DIMENS or SPECGRID.");
        }

        // Extract top corner depths, if available.
        const double* top_depths = 0;
        std::vector<double> top_depths_vec;
        if (deck->hasKeyword("DEPTHZ")) {
            const std::vector<double>& depthz = deck->getKeyword("DEPTHZ")->getSIDoubleData();
            if (depthz.size() != x.size()*y.size()) {
                OPM_THROW(std::runtime_error, "Incorrect size of DEPTHZ: " << depthz.size());
            }
            top_depths = &depthz[0];
        } else if (deck->hasKeyword("TOPS")) {
            // We only support constant values for TOPS.
            // It is not 100% clear how we best can deal with
            // varying TOPS (stair-stepping grid, or not).
            const std::vector<double>& tops = deck->getKeyword("TOPS")->getSIDoubleData();
            if (std::count(tops.begin(), tops.end(), tops[0]) != int(tops.size())) {
                OPM_THROW(std::runtime_error, "We do not support nonuniform TOPS, please use ZCORN/COORDS instead.");
            }
            top_depths_vec.resize(x.size()*y.size(), tops[0]);
            top_depths = &top_depths_vec[0];
        }

        // Construct grid.
        ug_ = create_grid_tensor3d(dxv.size(), dyv.size(), dzv.size(),
                                   &x[0], &y[0], &z[0], top_depths);
        if (!ug_) {
            OPM_THROW(std::runtime_error, "Failed to construct grid.");
        }
    }


} // namespace Opm
