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
 * IsotropicMaterial.hpp
 *
 *  Created on: Oct 21, 2016
 *      Author: srossi
 */

#ifndef SRC_ELASTICITY_MATERIALS_BenNeohookean_HPP_
#define SRC_ELASTICITY_MATERIALS_BenNeohookean_HPP_

#include "Elasticity/Materials/Material.hpp"

namespace BeatIt {

class BenNeohookean : public virtual Material
{
public:
    BenNeohookean();
	virtual ~BenNeohookean();


    void setup(GetPot& data, std::string section);
	/// This method is used for primal elasticity
	void evaluateVolumetricStress();
	void evaluateDeviatoricStress();
	void evaluateStress( ElasticSolverType solverType);
	/// This method is used only for mixed elasticy
	void evaluateDeviatoricJacobian(  const libMesh::TensorValue <double>&  dU, double q = 0.0) ;
	void evaluateVolumetricJacobian( const libMesh::TensorValue <double>& dU, double q = 0.0);

	/// This method is used for primal elasticity
	void evaluateJacobian(  const libMesh::TensorValue <double>&  dU, double q = 0.0);

	double evaluatePressure();
    double evaluatePressureResidual();
    double dpdF(const libMesh::TensorValue <double>&  dF);

    double d2U( double J = 1.0);
    double d3U( double J = 1.0);





};



Material* createBenNeohookean();

namespace
{
	static bool register_BenNeohookean = BeatIt::Material::MaterialFactory::Register("ben", &createBenNeohookean);
}


} /* namespace BeatIt */

#endif /* SRC_ELASTICITY_MATERIALS_ISOTROPICMATERIAL_HPP_ */
