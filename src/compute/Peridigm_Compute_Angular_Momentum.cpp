/*! \file Peridigm_Compute_Angular_Momentum.cpp */

//@HEADER
// ************************************************************************
//
//                             Peridigm
//                 Copyright (2011) Sandia Corporation
//
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions?
// David J. Littlewood   djlittl@sandia.gov
// John A. Mitchell      jamitch@sandia.gov
// Michael L. Parks      parks@sandia.gov
// Stewart A. Silling    sasilli@sandia.gov
//
// ************************************************************************
//@HEADER

#include <vector>

#include "Peridigm_Compute_Angular_Momentum.hpp"
#include "../core/Peridigm.hpp"

//! Standard constructor.
PeridigmNS::Compute_Angular_Momentum::Compute_Angular_Momentum(PeridigmNS::Peridigm *peridigm_ ){peridigm = peridigm_;}

//! Destructor.
PeridigmNS::Compute_Angular_Momentum::~Compute_Angular_Momentum(){}


//! Returns the fieldspecs computed by this class
std::vector<Field_NS::FieldSpec> PeridigmNS::Compute_Angular_Momentum::getFieldSpecs() const 
{
  std::vector<Field_NS::FieldSpec> myFieldSpecs;

  // This is a hack.
  // Ideally, we'd specify some global variable as the output variable, but Peridigm is not
  // currently capable of outputting a global variable.
  // So, just associate this compute class with the general displacment field, that way this
  // compute class will be called if "Displacement" is requested in the input deck.
  //myFieldSpecs.push_back(Field_NS::DISPL3D);

  return myFieldSpecs;
}

//! Perform computation
int PeridigmNS::Compute_Angular_Momentum::compute( Teuchos::RCP< std::vector<PeridigmNS::Block> > blocks  ) const
{
  return 0;
}        

//! Fill the angular momentum vector
int PeridigmNS::Compute_Angular_Momentum::computeAngularMomentum( Teuchos::RCP< std::vector<PeridigmNS::Block> > blocks, bool storeLocal  ) const
{
  int retval;

  double globalAM = 0.0;
  Teuchos::RCP<Epetra_Vector> velocity,  arm, volume, angular_momentum;
  std::vector<PeridigmNS::Block>::iterator blockIt;
  for(blockIt = blocks->begin() ; blockIt != blocks->end() ; blockIt++)
  {
    Teuchos::RCP<PeridigmNS::DataManager> dataManager = blockIt->getDataManager();
    Teuchos::RCP<PeridigmNS::NeighborhoodData> neighborhoodData = blockIt->getNeighborhoodData();
    const int numOwnedPoints = neighborhoodData->NumOwnedPoints();
    const int* ownedIDs = neighborhoodData->OwnedIDs();

    velocity         = dataManager->getData(Field_NS::VELOC3D, Field_ENUM::STEP_NP1);
    arm              = dataManager->getData(Field_NS::CURCOORD3D, Field_ENUM::STEP_NP1);
    volume           = dataManager->getData(Field_NS::VOLUME, Field_ENUM::STEP_NONE);
    angular_momentum = dataManager->getData(Field_NS::ANGULAR_MOMENTUM3D, Field_ENUM::STEP_NP1);

    // Sanity check
    if ( (velocity->Map().NumMyElements() != volume->Map().NumMyElements()) ||  (arm->Map().NumMyElements() != volume->Map().NumMyElements()) )
    {
      retval = 1;
      return(retval);
    }
 	
    // Collect values
    double *volume_values = volume->Values();
    double *velocity_values = velocity->Values();
    double *arm_values  = arm->Values();
    double *angular_momentum_values = angular_momentum->Values();

    // Initialize angular momentum values
    double angular_momentum_x,  angular_momentum_y, angular_momentum_z;
    angular_momentum_x = angular_momentum_y = angular_momentum_z = 0.0;
	
    double density = blockIt->getMaterialModel()->Density();
  	
    // volume is a scalar and force a vector, so maps are different; must do multiplication on per-element basis
    int numElements = numOwnedPoints;
    double vol, mass;
    for (int i=0;i<numElements;i++) 
    {
      int ID = ownedIDs[i];
      vol = volume_values[ID];
      mass = vol*density;
      double v1 = velocity_values[3*ID];
      double v2 = velocity_values[3*ID+1];
      double v3 = velocity_values[3*ID+2];
      double r1 = arm_values[3*ID];
      double r2 = arm_values[3*ID+1];
      double r3 = arm_values[3*ID+2];
      angular_momentum_x = angular_momentum_x + mass*(v2*r3 - v3*r2);
      angular_momentum_y = angular_momentum_y + mass*(v3*r1 - v1*r3); 
      angular_momentum_z = angular_momentum_z + mass*(v1*r2 - v2*r1);
      if (storeLocal)
      {
        angular_momentum_values[3*ID] = mass*(v2*r3 - v3*r2);
        angular_momentum_values[3*ID+1] = mass*(v3*r1 - v1*r3);
        angular_momentum_values[3*ID+2] = mass*(v1*r2 - v2*r1);     
      }
    }
    
    if (!storeLocal)
    {
      // Update info across processors
      std::vector<double> localAngularMomentum(3), globalAngularMomentum(3);
      localAngularMomentum[0] = angular_momentum_x;
      localAngularMomentum[1] = angular_momentum_y;
      localAngularMomentum[2] = angular_momentum_z;
	
      peridigm->getEpetraComm()->SumAll(&localAngularMomentum[0], &globalAngularMomentum[0], 3);
		
      globalAM += sqrt(globalAngularMomentum[0]*globalAngularMomentum[0] + globalAngularMomentum[1]*globalAngularMomentum[1] + globalAngularMomentum[2]*globalAngularMomentum[2]);
    }
  }

/*	
        if (peridigm->getEpetraComm()->MyPID() == 0) 
	{
		std::cout << "Hello!" << std::endl;
		std::cout << std::endl;
		std::cout << "Total Angular Momentum =  " << "("  << globalAngularMomentum[0]
							  << ", " << globalAngularMomentum[1]
							  << ", " << globalAngularMomentum[2] << ")" << std::endl;
	}
*/

  // Store global angular momentum in block (block globals are static, so only need to assign data to first block)
  if (!storeLocal)  
    blocks->begin()->getScalarData(Field_NS::GLOBAL_ANGULAR_MOMENTUM) = globalAM;

  return(0);

}
