/*
 ============================================================================

 .______    _______     ___   .___________.    __  .___________.
 |   _  \  |   ____|   /   \  |           |   |  | |           |
 |  |_)  | |  |__     /  ^  \ `---|  |----`   |  | `---|  |----`
 |   _  <  |   __|   /  /_\  \    |  |        |  |     |  |
 |  |_)  | |  |____ /  _____  \   |  |        |  |     |  |
 |______/  |_______/__/     \__\  |__|        |__|     |__|

 BeatIt - code for cardiovascular simulations
 Copyright (C) 2016 Simone Rossi

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ============================================================================
 */

/**
 * \file main.cpp
 *
 * \class main
 *
 * \brief This class provides a simple factory implementation
 *
 * For details on how to use it check the test_factory in the testsuite folder
 *
 *
 * \author srossi
 *
 * \version 0.0
 *
 *
 * Contact: srossi@gmail.com
 *
 * Created on: Aug 11, 2016
 *
 */

// Basic include files needed for the mesh functionality.
#include "Electrophysiology/Bidomain/Bidomain.hpp"
#include "Electrophysiology/Monodomain/MonodomainUtil.hpp"

#include "libmesh/linear_implicit_system.h"
#include "libmesh/transient_system.h"

#include "libmesh/wrapped_functor.h"
#include "libmesh/mesh.h"
#include "libmesh/mesh_generation.h"
#include "libmesh/mesh_modification.h"
#include "Util/SpiritFunction.hpp"

#include "libmesh/numeric_vector.h"

#include "libmesh/error_vector.h"
#include "libmesh/kelly_error_estimator.h"
#include "libmesh/mesh_refinement.h"
#include "libmesh/fourth_error_estimators.h"

#include "Util/CTestUtil.hpp"
#include <iomanip>
//#include "libmesh/vtk_io.h"
#include "libmesh/exodusII_io.h"
#include "Util/Timer.hpp"

enum class TestCase
{
	NP,                      // Nash Panfilov model
	NP_AMR,           // Nash Panfilov model using AMR
	ORd,                   // ORd model no AMR
};

int main(int argc, char ** argv)
{
	// Bring in everything from the libMesh namespace

	using namespace libMesh;
	// Initialize libraries, like in example 2.
	LibMeshInit init(argc, argv, MPI_COMM_WORLD);

	// Create a mesh, with dimension to be overridden later, distributed
	// across the default MPI communicator.
	Mesh mesh(init.comm());

	// Use the MeshTools::Generation mesh generator to create a uniform
	// 3D grid
	// We build a linear tetrahedral mesh (TET4) on  [0,2]x[0,0.7]x[0,0.3]
	// the number of elements on each side is read from the input file
	GetPot commandLine(argc, argv);
	std::string datafile_name = commandLine.follow("data.beat", 2, "-i",
			"--input");
	GetPot data(datafile_name);

	BeatIt::TimeData datatime;
	datatime.setup(data, "bidomain");
	datatime.print();

	bool do_restart = data("bidomain/restart/restart", false);
	ExodusII_IO importer(mesh);
	if (do_restart)
	{
		std::string restart_file = data("bidomain/restart/restart_file",
				"NONE");
		if (restart_file != "NONE")
		{
			importer.read(restart_file);
			mesh.prepare_for_use();
		}
		else
		{
			do_restart = false;
		}

	}

	if (do_restart == false)
	{
		// allow us to use higher-order approximation.
		int numElementsX = data("mesh/elX", 15);
		int numElementsY = data("mesh/elY", 5);
		int numElementsZ = data("mesh/elZ", 4);
		double maxX = data("mesh/maxX", 2.0);
		double maxY = data("mesh/maxY", 0.7);
		double maxZ = data("mesh/maxZ", 0.3);
		double rotation = data("mesh/rotation", 0.0);
		double x_translation = data("mesh/x_translation", 0.0);
		double y_translation = data("mesh/y_translation", 0.0);
		double z_translation = data("mesh/z_translation", 0.0);

		std::map < std::string, ElemType > orderMap;
		orderMap["TRI3"] = TRI3;
		orderMap["QUAD4"] = QUAD4;
		orderMap["TRI6"] = TRI6;
		orderMap["QUAD9"] = QUAD9;
		std::string mesh_type = data("mesh/type", "TRI3");
		auto elType = orderMap.find(mesh_type)->second;

		MeshTools::Generation::build_square(mesh, numElementsX, numElementsY,
				0., maxX, 0.0, maxY, elType);
		MeshTools::Modification::rotate(mesh, rotation);
		MeshTools::Modification::translate(mesh, x_translation, y_translation,
				z_translation);
	}

	std::cout << "Mesh done!" << std::endl;

	std::string system_mass = data("bidomain/diffusion_mass", "mass");
	std::string iion_mass = data("bidomain/reaction_mass", "lumped_mass");
	bool useMidpointMethod = false;
	int step0 = 0;
	int step1 = 1;

	// Constructor
	std::cout << "Create bidomain ..." << std::endl;
	libMesh::EquationSystems es1(mesh);
	BeatIt::Bidomain bidomain(es1);
	bidomain.setup(data, "bidomain");
	bidomain.init(0.0);
	std::cout << "Assembling matrices" << std::endl;
    bidomain.assemble_matrices(datatime.M_dt);
      if(do_restart)
      {
  		int restart_step = data("bidomain/restart/step", 2);
  		bidomain.restart(importer, restart_step);
      }

      int save_iter = 1;
  	std::cout << "Init Output" << std::endl;
      bidomain.init_exo_output();
      bidomain.save_exo(save_iter++, datatime.M_time);


    	std::cout << "Time loop starts:" << std::endl;
      for( ; datatime.M_iter < datatime.M_maxIter && datatime.M_time < datatime.M_endTime ; )
      {
		  datatime.advance();
          std::cout << "Time:" << datatime.M_time << std::endl;
		  bidomain.advance();
		  bidomain.solve_reaction_step(datatime.M_dt, datatime.M_time,step0, useMidpointMethod, iion_mass);

		  bidomain.solve_diffusion_step(datatime.M_dt, datatime.M_time, useMidpointMethod, iion_mass);
		  bidomain.update_activation_time(datatime.M_time);

          if( 0 == datatime.M_iter%datatime.M_saveIter )
          {

	        bidomain.save_potential(save_iter++, datatime.M_time);

          }
          }

      bidomain.save_exo(2, datatime.M_time);

      typedef libMesh::TransientLinearImplicitSystem BidomainSystem;
  	BidomainSystem& bidomain_system = es1.get_system
  			< BidomainSystem > ("bidomain");
  	double sol_norm = bidomain_system.solution->l2_norm();
    std::cout << std::setprecision(25) << "pot norm = " << sol_norm << std::endl;
    const double reference_value = 53.26755679280705635392223;
    return BeatIt::CTest::check_test(sol_norm, reference_value, 1e-8);
}

