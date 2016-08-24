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
#include "Electrophysiology/Monodomain/Monodomain.hpp"
#include "Electrophysiology/Monodomain/MonodomainUtil.hpp"

#include "libmesh/linear_implicit_system.h"
#include "libmesh/transient_system.h"

#include "libmesh/wrapped_functor.h"
#include "libmesh/mesh.h"
#include "libmesh/mesh_generation.h"
#include "Util/SpiritFunction.hpp"

#include "libmesh/numeric_vector.h"


#include "libmesh/error_vector.h"
#include "libmesh/kelly_error_estimator.h"
#include "libmesh/mesh_refinement.h"
#include "libmesh/fourth_error_estimators.h"


int main (int argc, char ** argv)
{
    // Bring in everything from the libMesh namespace

    using namespace libMesh;
      // Initialize libraries, like in example 2.
      LibMeshInit init (argc, argv, MPI_COMM_WORLD);

      // Create a mesh, with dimension to be overridden later, distributed
      // across the default MPI communicator.
      Mesh mesh(init.comm());

      // Use the MeshTools::Generation mesh generator to create a uniform
      // 3D grid
      // We build a linear tetrahedral mesh (TET4) on  [0,2]x[0,0.7]x[0,0.3]
      // the number of elements on each side is read from the input file
      GetPot data("data.pot");
      // allow us to use higher-order approximation.
      int numElementsX = data("mesh/elX", 15);
      int numElementsY = data("mesh/elY",   5);
      int numElementsZ = data("mesh/elZ",   4);
      MeshTools::Generation::build_cube (mesh,
    		  	  	  	  	  	  	  	  numElementsX, numElementsY, numElementsZ,
                                         0., 2.,
                                         0., 0.7,
                                         0., 0.3,
                                         TET4);
      libMesh:: MeshRefinement mesh_refinement(mesh);
      mesh_refinement.refine_fraction()  =  data("mesh/refine_fraction", 0.7);
      mesh_refinement.coarsen_fraction() =data("mesh/coarsen_fraction", 0.3);
      int max_num_mesh_ref = data("mesh/max_num_mesh_ref", 0);
      int AMRstep = data("mesh/step", 10);
     std::string error_estimator  =  data("mesh/error_estimator","kelly");
      mesh_refinement.max_h_level()      = max_num_mesh_ref;
      const unsigned int max_r_steps = max_num_mesh_ref;
      bool usingAMR = false;
      if(max_num_mesh_ref > 0) usingAMR = true;
      //IonicModel* M_ionicModelPtr =  BeatIt::IonicModel::IonicModelFactory::Create("NashPanfilov");

      // Create an equation systems object.

//      equation_systems.add_system<libMesh::TransientLinearImplicitSystem>("Test");
//      equation_systems.get_system("Test").add_variable("Pippo",libMesh::FIRST);

      // Constructor
      BeatIt::Monodomain monodomain(mesh);
      // Setup the equation systems
      monodomain.setup(data, "monodomain");
      monodomain.init(0.0);

      BeatIt::TimeData datatime;
      datatime.setup(data, "monodomain/");
      datatime.print();

      int save_iter = 0;
      monodomain.init_exo_output();
      save_iter++;
//      monodomain.save(save_iter++);
      monodomain.assemble_matrices();

      libMesh::PerfLog perf_log ("Solving");

      for( ; datatime.M_iter < datatime.M_maxIter && datatime.M_time < datatime.M_endTime ; )
      {

		  datatime.advance();
		  monodomain.advance();
//
		  if( 0 == (datatime.M_iter-1)%AMRstep && true == usingAMR)
		  {
//
			  for (unsigned int r_step=0; r_step <= max_r_steps; r_step++)
			  {
//			  perf_log.push("update pacing");
			  monodomain.update_pacing(datatime.M_time-0.75*datatime.M_dt);
//			  perf_log.pop("update pacing");
//			  perf_log.push("solving reactions");
//			  monodomain.solve_reaction_step(datatime.M_dt/2, datatime.M_time);
			  monodomain.solve_reaction_step(datatime.M_dt/2, datatime.M_time,0);
//			  perf_log.pop("solving reactions");
//			  perf_log.push("solving diffusion");
			  monodomain.solve_diffusion_step(datatime.M_dt, datatime.M_time);
//			  perf_log.pop("solving diffusion");
//			  perf_log.push("update pacing");
			  monodomain.update_pacing(datatime.M_time-0.25*datatime.M_dt);
//			  perf_log.pop("update pacing");
//			  perf_log.push("solving reactions");
			  monodomain.solve_reaction_step(datatime.M_dt/2, datatime.M_time, 1);
//			  perf_log.pop("solving reactions");
//			  perf_log.push("update activation times");
			  monodomain.update_activation_time(datatime.M_time);
//			  perf_log.pop("update activation times");

			  				  if (r_step != max_r_steps )
			  				  {
			  					  monodomain.amr(mesh_refinement, error_estimator);
			  					  monodomain.assemble_matrices();
			  					  monodomain.reinit_linear_solver();
			  				  }


			  }

		  }
		  else
		  {
			  perf_log.push("update pacing");
			  monodomain.update_pacing(datatime.M_time-0.75*datatime.M_dt);
			  perf_log.pop("update pacing");
			  perf_log.push("solving reactions");
			  monodomain.solve_reaction_step(datatime.M_dt/2, datatime.M_time,0);
			  perf_log.pop("solving reactions");
			  perf_log.push("solving diffusion");
			  monodomain.solve_diffusion_step(datatime.M_dt, datatime.M_time);
			  perf_log.pop("solving diffusion");
			  perf_log.push("update pacing");
			  monodomain.update_pacing(datatime.M_time-0.25*datatime.M_dt);
			  perf_log.pop("update pacing");
			  perf_log.push("solving reactions");
			  monodomain.solve_reaction_step(datatime.M_dt/2, datatime.M_time, 1);
			  perf_log.pop("solving reactions");
			  perf_log.push("update activation times");
			  monodomain.update_activation_time(datatime.M_time);
			  perf_log.pop("update activation times");
		  }


          if( 0 == datatime.M_iter%datatime.M_saveIter )
          {
              std::cout << "* Test Monodomain: Time: " << datatime.M_time << std::endl;
//              monodomain.save(save_iter++);
              perf_log.push("saving");

              if(usingAMR == true)
			  {
            	  monodomain.save(save_iter);
            	  monodomain.save_exo(save_iter++, datatime.M_time);
			  }
              else monodomain.save_exo(save_iter++, datatime.M_time);
              perf_log.pop("saving");
          }
      }

      monodomain.save_parameters();

      return 0;

}

