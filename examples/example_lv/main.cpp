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

/*
 * main.cpp
 *
 *  Created on: Sep 14, 2016
 *      Author: srossi
 */
// Basic include files needed for the mesh functionality.
#include "Elasticity/MixedElasticity.hpp"
#include "PoissonSolver/Poisson.hpp"

#include "libmesh/mesh_base.h"
#include "libmesh/linear_implicit_system.h"
#include "libmesh/transient_system.h"

#include "libmesh/wrapped_functor.h"
#include "libmesh/mesh.h"
#include "libmesh/mesh_generation.h"
#include "Util/SpiritFunction.hpp"

#include "libmesh/numeric_vector.h"
#include "libmesh/elem.h"
#include "Util/CTestUtil.hpp"
#include "libmesh/dof_map.h"
#include <iomanip>
#include "Util/CTestUtil.hpp"
#include "libmesh/mesh_modification.h"

enum Formulation { Primal,  Mixed, Incompressible };
enum CL { Linear,  NH };

void moveMesh(BeatIt::Elasticity* elas);
double checkConfiguration(BeatIt::Elasticity* elas);
void generate_fibers(  libMesh::EquationSystems& es , GetPot& data_fibers );

int main (int argc, char ** argv)
{
    // Bring in everything from the libMesh namespace

    using namespace libMesh;
    // Initialize libraries, like in example 2.
    LibMeshInit init (argc, argv, MPI_COMM_WORLD);


    // Use the MeshTools::Generation mesh generator to create a uniform
    // 3D grid
    // We build a linear tetrahedral mesh (TET4) on  [0,2]x[0,0.7]x[0,0.3]
    // the number of elements on each side is read from the input file
    GetPot commandLine ( argc, argv );
    std::string datafile_name = commandLine.follow ( "data.beat", 2, "-i", "--input" );
    GetPot data(datafile_name);
    // allow us to use higher-order approximation.
    // Create a mesh, with dimension to be overridden later, on the
    // default MPI communicator.
    libMesh::Mesh mesh(init.comm());

    std::string meshfile =  data("mesh", "UNKNOWN");
    mesh.read(&meshfile[0]);
    double scalex =  data("scalex", 1.0);
    double scaley =  data("scaley", 1.0);
    double scalez =  data("scalez", 1.0);
    MeshTools::Modification::scale(mesh, scalex, scaley, scalez);
    libMesh::EquationSystems es(mesh);

    Formulation f;
    std::string formulation = data("elasticity/formulation", "NO_FORMULATION");
    std::cout << "Creating " << formulation << std::endl;

    CL cl;
    std::string cl_name = data("elasticity/materials", "NO_MATERIAL");
    std::cout << "Material " << cl_name << std::endl;

    BeatIt::Elasticity* elas = nullptr;
    if(formulation == "primal")
    {
    	//throw std::runtime_error("Cannot use primal formulation");
        elas = new BeatIt::Elasticity(es, "Elasticity");
    }
    else
    {
        elas = new BeatIt::MixedElasticity(es, "Elasticity");
    }
    std::cout << "Setting up ... " << std::endl;
    elas->setup(data,"elasticity");
        // compute fiber using Poisson interpolation
    generate_fibers(es, data);

    std::cout << "Initializing output ... " << std::endl;
    elas->init_exo_output("solution.exo");

    int save_iter = 1;
    double ramp_dt = data("ramp/dt", 0.1);
    double ramp_end_time = data("ramp/end_time", 0.9);
    double time = 0.0;
    bool computeReferenceConfiguration = data("do_bd", true);
    double error = 0.0;
	int bd_iter = 0;
	elas->save_exo("solution.exo", save_iter,0);
	//elas->save("solution.gmv", save_iter);
    save_iter++;
    while(time < ramp_end_time)
    {
        std::cout << "Solving ... " << std::endl;
        time+=ramp_dt;
        elas->setTime(time);
        std::cout << "Time: " << time << std::endl;
		elas->newton();
		elas->evaluate_nodal_I4f();
		if(computeReferenceConfiguration)
		{
			bd_iter = 0;
			std::cout << "Backward Displacement iteration: " << bd_iter << std::endl;
			error = checkConfiguration(elas);

			double tol = 1e-8;
			while(error > tol)
			{
				bd_iter++;
				moveMesh(elas);

				elas->newton();
				elas->evaluate_nodal_I4f();
				std::cout << "Backward Displacement iteration: " << bd_iter << std::endl;
				error = checkConfiguration(elas);
			}
		    //generate_fibers(es, data);
		}
		//elas->save("solution.gmv", save_iter);
		elas->save_exo("solution.exo", save_iter, time);
 	   save_iter++;

    }

    delete elas;
    std::cout << std::setprecision(20) << "error: " << error << std::endl;
    std::cout << "backward displacement iterations: " << bd_iter << std::endl;

    return 0;
}

void moveMesh(BeatIt::Elasticity* elas)
{
	std::cout << "Moving the mesh" << std::endl;
	const libMesh::MeshBase & mesh = elas->M_equationSystems.get_mesh();
	const unsigned int dim = mesh.mesh_dimension();

	auto it =  elas->M_equationSystems.get_mesh().local_nodes_begin();
	auto it_end =  elas->M_equationSystems.get_mesh().local_nodes_end();

	typedef libMesh::TransientLinearImplicitSystem LinearSystem;
	auto& system = elas->M_equationSystems.get_system < LinearSystem
			> ("Elasticity");
	auto sys_num = system.number();
	const libMesh::DofMap & dof_map = system.get_dof_map();

	for (; it != it_end; it++)
	{
		auto * node = *it;
		auto n_vars = node->n_vars(sys_num);
		for (auto v = 0; v != n_vars; ++v)
		{
			double value = 0.0;
			if (v < dim) // 3 may be the pressure field
			{
				value = (*node)(v);

				// the number of components will always be 1 in this test
				// because the components of the displacement are defined as
				// separate scalar variables
				auto nc = node->n_comp(sys_num, v);
				auto dof_id = node->dof_number(sys_num, v, 0);
				auto disp = (*system.solution)(dof_id);
				auto x = system.get_vector("X")(dof_id);
				double X = x - disp;
				(*node)(v) = X;

//				std::cout << "position after: " << value << std::endl;
//				for (auto c = 0; c != nc; ++c)
//				{
//					//system.get_vector("X").set(dof_id, value);
//				}
			}
		}
	}
}


double checkConfiguration(BeatIt::Elasticity* elas)
{
	std::cout << "====================================" << std::endl;
	std::cout << "Checking error in the configuration" << std::endl;
	const libMesh::MeshBase & mesh = elas->M_equationSystems.get_mesh();
	const unsigned int dim = mesh.mesh_dimension();

	auto it =  elas->M_equationSystems.get_mesh().local_nodes_begin();
	auto it_end =  elas->M_equationSystems.get_mesh().local_nodes_end();

	typedef libMesh::TransientLinearImplicitSystem LinearSystem;
	auto& system = elas->M_equationSystems.get_system < LinearSystem
			> ("Elasticity");
	auto sys_num = system.number();
	const libMesh::DofMap & dof_map = system.get_dof_map();

	double error_l_inf = -1;

	for (; it != it_end; it++)
	{
		auto * node = *it;
		auto n_vars = node->n_vars(sys_num);
		for (auto v = 0; v != n_vars; ++v)
		{
			double value = 0.0;
			if (v < dim) // 3 may be the pressure field
			{
				// the number of components will always be 1 in this test
				// because the components of the displacement are defined as
				// separate scalar variables
				auto nc = node->n_comp(sys_num, v);
				auto dof_id = node->dof_number(sys_num, v, 0);
				auto disp = (*system.solution)(dof_id);
				auto x = system.get_vector("X")(dof_id);
				auto X = (*node)(v);
				error_l_inf = std::max(error_l_inf, std::abs(x - X - disp ) );
//				std::cout << "position after: " << value << std::endl;
//				for (auto c = 0; c != nc; ++c)
//				{
//					//system.get_vector("X").set(dof_id, value);
//				}
			}
		}
	}
	std::cout << "errol_l_inf: " << error_l_inf << std::endl;
	std::cout << "====================================" << std::endl;

	return error_l_inf;
}

void generate_fibers(  libMesh::EquationSystems& es , GetPot& data_fibers)
{
		std::string pois1 = "poisson1";
		BeatIt::Poisson poisson1(es, pois1);
		poisson1.setup(data_fibers, "poisson1");

	     const libMesh::MeshBase & mesh = poisson1.M_equationSystems.get_mesh();

		std::cout << " Done!" << std::endl;
		std::cout << "Calling assemble system: ..." << std::flush;
		poisson1.assemble_system();
		std::cout << " Done!" << std::endl;
		std::cout << "Calling solve system: ..." << std::flush;
		poisson1.solve_system();
		std::cout << " Done!" << std::endl;
		std::cout << "Calling gradient: ..." << std::flush;
		poisson1.compute_elemental_solution_gradient();
		std::cout << " Done!" << std::endl;
		auto& grad1 = poisson1.M_equationSystems.get_system
				< libMesh::ExplicitSystem > (pois1 + "_gradient").solution;
		auto& p0_system = poisson1.M_equationSystems.get_system
				< libMesh::ExplicitSystem > (pois1 + "_P0");
		p0_system.update();
		const auto& sol_ptr = p0_system.current_local_solution;

		auto first = grad1->first_local_index();
		auto last = grad1->last_local_index();

		//const auto& sol_ptr = p.get_P0_solution();
		double cx = data_fibers("poisson/centerline_x", 0.0);
		double cy = data_fibers("poisson/centerline_y", 0.0);
		double cz = data_fibers("poisson/centerline_z", 1.0);
        double epi_angle = data_fibers ("poisson/epi_angle", -60.0);
        double endo_angle = data_fibers ("poisson/endo_angle", 60.0);


		auto normalize = [](double& x, double& y, double& z,
				double X, double Y, double Z)
		{
			double norm = std::sqrt( x * x + y * y + z * z);
			if(norm >= 1e-12 )
			{
				x /= norm;
				y /= norm;
				z /= norm;
			}
			else
			{
				x = X;
				y = Y;
				z = Z;
			}
		};

    double norm = 0.0;
    double fx = 0.0;
    double fy = 0.0;
    double fz = 0.0;
    double cdot = 0.0;
    double sx = 0.0;
    double sy = 0.0;
    double sz = 0.0;
    double xfx = 0.0;
    double xfy = 0.0;
    double xfz = 0.0;
    double potential = 0.0;
    double W11 = 0.0;
    double W12 = 0.0;
    double W13 = 0.0;
    double W21 = 0.0;
    double W22 = 0.0;
    double W23 = 0.0;
    double W31 = 0.0;
    double W32 = 0.0;
    double W33 = 0.0;
    //
    double R11 = 0.0;
    double R12 = 0.0;
    double R13 = 0.0;
    double R21 = 0.0;
    double R22 = 0.0;
    double R23 = 0.0;
    double R31 = 0.0;
    double R32 = 0.0;
    double R33 = 0.0;
    double sa  = 0.0;
    double sa2 = 0.0;
    double teta1 = 0.0;
    double teta2 = 0.0;
    double teta  = 0.0;
    double m = 0.0;
    double q = 0.0;
    double f0x = 0.0;
    double f0y = 0.0;
    double f0z = 0.0;

    typedef libMesh::ExplicitSystem                     ParameterSystem;
    ParameterSystem& fiber_system    = es.get_system<ParameterSystem>("fibers");
    ParameterSystem& sheets_system       = es.get_system<ParameterSystem>("sheets");
    ParameterSystem& xfiber_system       = es.get_system<ParameterSystem>("xfibers");

	*sheets_system.solution = *poisson1.get_gradient();

	const libMesh::DofMap & f_dof_map = fiber_system.get_dof_map();
	const libMesh::DofMap & p0_dof_map= p0_system.get_dof_map();

	std::vector < libMesh::dof_id_type > f_dof_indices;
	std::vector < libMesh::dof_id_type > p0_dof_indices;

	std::vector<double> phi;
	std::vector<double> fibers;
	std::vector<double> sheets;
	std::vector<double> xfibers;
	    auto j = first;

//    for(auto i = first; i < last; )
	     libMesh::MeshBase::const_element_iterator el =
	             mesh.active_local_elements_begin();
	     const libMesh::MeshBase::const_element_iterator end_el =
	             mesh.active_local_elements_end();

	 for (; el != end_el; ++el)
    {
         const libMesh::Elem * elem = *el;
         f_dof_map.dof_indices(elem, f_dof_indices);
         p0_dof_map.dof_indices(elem, p0_dof_indices);

         p0_system.current_local_solution->get(p0_dof_indices, phi);
         fiber_system.solution->get(f_dof_indices, fibers);
         sheets_system.solution->get(f_dof_indices, sheets);
         xfiber_system.solution->get(f_dof_indices, xfibers);
        potential = phi[0];
        j++;

        sx = sheets[0];
        sy = sheets[1];
        sz = sheets[2];
        normalize(sx, sy, sz, 0.0, 1.0, 0.0);

        cdot = cx * sx + cy * sy + cz * sz;

        xfx = cx - cdot * sx;
        xfy = cy - cdot * sy;
        xfz = cz - cdot * sz;
        normalize(xfx, xfy, xfz, 0.0, 0.0, 1.0);

        fx = sy * xfz - sz * xfy;
        fy = sz * xfx - sx * xfz;
        fz = sx * xfy - sy * xfx;
        normalize(fx, fy, fz, 1.0, 0.0, 0.0);

        teta1 = M_PI * epi_angle / 180.0;
        teta2 = M_PI * endo_angle / 180.0;
        m = (teta1 - teta2 );
        q = teta2;
        teta =  m * potential + q;


        //*************************************************************//
        // The fiber field F is a rotation of the flat fiber field f
        // F = R f
        // where R is the rotation matrix.
        // To compute R we need the sin(teta) and
        // the sin(teta)^2 and the cross-product matrix W (check
        // rodrigues formula on wikipedia :) )
        //*************************************************************//
        sa = std::sin (teta);
        sa2 = 2.0 * std::sin (0.5 * teta) * std::sin (0.5 * teta);

        W11 = 0.0; W12 = -sz; W13 =  sy;
        W21 =  sz; W22 = 0.0; W23 = -sx;
        W31 = -sy; W32 =  sx; W33 = 0.0;
        //
        R11 = 1.0 + sa * W11 + sa2 * ( sx * sx - 1.0 );
        R12 = 0.0 + sa * W12 + sa2 * ( sx * sy );
        R13 = 0.0 + sa * W13 + sa2 * ( sx * sz );
        R21 = 0.0 + sa * W21 + sa2 * ( sy * sx );
        R22 = 1.0 + sa * W22 + sa2 * ( sy * sy - 1.0 );
        R23 = 0.0 + sa * W23 + sa2 * ( sy * sz );
        R31 = 0.0 + sa * W31 + sa2 * ( sz * sx );
        R32 = 0.0 + sa * W32 + sa2 * ( sz * sy );
        R33 = 1.0 + sa * W33 + sa2 * ( sz * sz - 1.0 );

        f0x =   R11 * fx + R12 * fy + R13 * fz;
        f0y =   R21 * fx + R22 * fy + R23 * fz;
        f0z =   R31 * fx + R32 * fy + R33 * fz;
        normalize(f0x, f0y, f0z, 1.0, 0.0, 0.0);


        xfx = f0y * sz - f0z * sy;
        xfy = f0z * sx - f0x * sz;
        xfz = f0x * sy - f0y * sx;
        normalize(xfx, xfy, xfz, 0.0, 0.0, 1.0);

        sheets[0] = sx;
        sheets[1] = sy;
        sheets[2] = sz;
        sheets_system.solution->insert(sheets, f_dof_indices);
//        sheets_system.solution->set(i+1, sy);
//        sheets_system.solution->set(i+2, sz);
        xfibers[0] = xfx;
        xfibers[1] = xfy;
        xfibers[2] = xfz;
        xfiber_system.solution->insert(xfibers, f_dof_indices);
//        xfiber_system.solution->set(i+1, xfy);
//        xfiber_system.solution->set(i+2, xfz);

        fibers[0] = f0x;
        fibers[1] = f0y;
        fibers[2] = f0z;
        fiber_system. solution->insert(fibers, f_dof_indices);
//        fiber_system. solution->set(i+1, f0y);
//        fiber_system. solution->set(i+2, f0z);

        //i += 3;
    }
	poisson1.deleteSystems();

}

