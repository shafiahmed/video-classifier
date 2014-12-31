/* Author: Sudnya Padalikar
 * Date  : 08/11/2013
 * The implementation of the layer class
 */

// Minerva Includes
#include <minerva/network/interface/Layer.h>

#include <minerva/network/interface/ActivationCostFunction.h>
#include <minerva/network/interface/ActivationFunction.h>
#include <minerva/network/interface/WeightCostFunction.h>

namespace minerva
{

namespace network
{

Layer::~Layer()
{

}
	
void Layer::setActivationFunction(ActivationFunction* f)
{
	_activationFunction.reset(f);
}

ActivationFunction* Layer::getActivationFunction()
{
	return _activationFunction.get();
}

void Layer::setActivationCostFunction(ActivationCostFunction* f)
{
	_activationCostFunction.reset(f);
}

ActivationCostFunction* Layer::getActivationCostFunction()
{
	return _activationCostFunction.get();
}

void Layer::setWeightCostFunction(WeightCostFunction* f)
{
	_weightCostFunction.reset(f);
}

WeightCostFunction* Layer::getWeightCostFunction()
{
	return _weightCostFunction.get();
}

}

}


