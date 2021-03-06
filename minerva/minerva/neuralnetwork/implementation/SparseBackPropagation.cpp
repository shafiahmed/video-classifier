/*! \file   SparseBackPropagation.cpp
	\author Gregory Diamos
	\date   Sunday December 22, 2013
	\brief  The source file for the SparseBackPropagation class.
*/

// Minerva Includes
#include <minerva/neuralnetwork/interface/SparseBackPropagation.h>
#include <minerva/neuralnetwork/interface/NeuralNetwork.h>

#include <minerva/matrix/interface/Matrix.h>
#include <minerva/matrix/interface/BlockSparseMatrix.h>
#include <minerva/matrix/interface/BlockSparseMatrixVector.h>

#include <minerva/util/interface/debug.h>
#include <minerva/util/interface/Knobs.h>

// Standard Library Includes
#include <algorithm>

namespace minerva
{

namespace neuralnetwork
{

typedef matrix::Matrix Matrix;
typedef matrix::BlockSparseMatrix BlockSparseMatrix;
typedef Matrix::FloatVector FloatVector;
typedef SparseBackPropagation::BlockSparseMatrixVector BlockSparseMatrixVector;

static BlockSparseMatrixVector computeCostDerivative(const NeuralNetwork& network, const BlockSparseMatrix& input,
	const BlockSparseMatrix& referenceOutput, float lambda, float sparsity, float sparsityWeight);
static BlockSparseMatrix computeInputDerivative(const NeuralNetwork& network, const BlockSparseMatrix& input,
	const BlockSparseMatrix& referenceOutput, float lambda, float sparsity, float sparsityWeight);
static float computeCostForNetwork(const NeuralNetwork& network, const BlockSparseMatrix& input,
	const BlockSparseMatrix& referenceOutput, float lambda, float sparsity, float sparsityWeight);
static float getActivationSparsityCost(const NeuralNetwork& network, const BlockSparseMatrix& input,
	float sparsity, float sparsityWeight);

SparseBackPropagation::SparseBackPropagation(NeuralNetwork* ann,
	BlockSparseMatrix* input,
	BlockSparseMatrix* ref)
: BackPropagation(ann, input, ref), _lambda(0.0f), _sparsity(0.0f), _sparsityWeight(0.0f)
{
	_lambda         = util::KnobDatabase::getKnobValue("NeuralNetwork::Lambda",         0.050f);
	_sparsity       = util::KnobDatabase::getKnobValue("NeuralNetwork::Sparsity",       0.005f);
	_sparsityWeight = util::KnobDatabase::getKnobValue("NeuralNetwork::SparsityWeight", 0.600f);
}

BlockSparseMatrixVector SparseBackPropagation::getCostDerivative(const NeuralNetwork& neuralNetwork,
	const BlockSparseMatrix& input, const BlockSparseMatrix& reference) const
{
	return neuralnetwork::computeCostDerivative(neuralNetwork, input, reference, _lambda, _sparsity, _sparsityWeight);
}

BlockSparseMatrix SparseBackPropagation::getInputDerivative(const NeuralNetwork& network,
	const BlockSparseMatrix& input, const BlockSparseMatrix& reference) const
{
	return neuralnetwork::computeInputDerivative(network, input, reference, _lambda, _sparsity, _sparsityWeight);
}

float SparseBackPropagation::getCost(const NeuralNetwork& network,
	const BlockSparseMatrix& input, const BlockSparseMatrix& reference) const
{
	return neuralnetwork::computeCostForNetwork(network, input, reference, _lambda, _sparsity, _sparsityWeight);
}

float SparseBackPropagation::getInputCost(const NeuralNetwork& network,
	const BlockSparseMatrix& input, const BlockSparseMatrix& reference) const
{
	return neuralnetwork::computeCostForNetwork(network, input, reference, 0.0f, 0.0f, 0.0f);
}

static float computeCostForNetwork(const NeuralNetwork& network, const BlockSparseMatrix& input,
	const BlockSparseMatrix& referenceOutput, float lambda, float sparsity, float sparsityWeight)
{
	//J(theta) = -1/m (sum over i, sum over k y(i,k) * log (h(x)) + (1-y(i,k))*log(1-h(x)) +
	//		   regularization term lambda/2m sum over l,i,j (theta[i,j])^2
	// J = (1/m) .* sum(sum(-yOneHot .* log(hx) - (1 - yOneHot) .* log(1 - hx)));

	#if USE_LOGISTIC_COST_FUNCTION
	const float epsilon = 1.0e-40f;

	unsigned m = input.rows();

	auto hx = network.runInputs(input);
	
	auto logHx = hx.add(epsilon).log();
	auto yTemp = referenceOutput.elementMultiply(logHx);

	auto oneMinusY = referenceOutput.negate().add(1.0f);
	auto oneMinusHx = hx.negate().add(1.0f);
	auto logOneMinusHx = oneMinusHx.add(epsilon).log(); // add an epsilon to avoid log(0)
	auto yMinusOneTemp = oneMinusY.elementMultiply(logOneMinusHx);

	auto sum = yTemp.add(yMinusOneTemp);

	float costSum = sum.reduceSum() * -0.5f / m;
	#else

	unsigned m = input.rows();

	auto hx = network.runInputs(input);

	auto errors = hx.subtract(referenceOutput);
	auto squaredErrors = errors.elementMultiply(errors);

	float sumOfSquaredErrors = squaredErrors.reduceSum();
	
	float costSum = sumOfSquaredErrors * 1.0f / (2.0f * m);

	#endif

	if(lambda > 0.0f)
	{
		float regularizationCost = 0.0f;

		for(auto& layer : network)
		{
			regularizationCost += ((layer.getWeightsWithoutBias().elementMultiply(
				layer.getWeightsWithoutBias())).reduceSum());
		}
		
		regularizationCost *= lambda / 2.0f;
		
		costSum += regularizationCost;
	}

	if(sparsityWeight > 0.0f)
	{
		costSum += getActivationSparsityCost(network, input, sparsity, sparsityWeight);
	}
	
	return costSum;
}

static BlockSparseMatrixVector getActivations(const NeuralNetwork& network, const BlockSparseMatrix& input) 
{
	BlockSparseMatrixVector activations;

	activations.reserve(network.size() + 1);

	auto temp = input;

	activations.push_back(temp);
	//util::log("SparseBackPropagation") << " added activation of size ( " << activations.back().rows()
	// << " ) rows and ( " << activations.back().columns() << " )\n" ;

	for (auto i = network.begin(); i != network.end(); ++i)
	{
		network.formatInputForLayer(*i, activations.back());
	
		activations.push_back((*i).runInputs(activations.back()));
		util::log("SparseBackPropagation::Detail") << " added activation of size ( " << activations.back().rows()
		<< " ) rows and ( " << activations.back().columns() << " )\n" ;
	}

	//util::log("SparseBackPropagation") << " intermediate stage ( " << activations[activations.size() / 2].toString() << "\n";
	//util::log("SparseBackPropagation") << " final output ( " << activations.back().toString() << "\n";

	return activations;
}

static BlockSparseMatrixVector getDeltas(const NeuralNetwork& network, const BlockSparseMatrixVector& activations, const BlockSparseMatrix& reference,
	float sparsity, float sparsityWeight)
{
	BlockSparseMatrixVector deltas;
	
	deltas.reserve(activations.size() - 1);
	
	auto i = activations.rbegin();
	auto delta = (*i).subtract(reference).elementMultiply(i->sigmoidDerivative());
	++i;

	while (i != activations.rend())
	{
		deltas.push_back(std::move(delta));
		
		unsigned int layerNumber = std::distance(activations.begin(), --(i.base()));
		//util::log ("SparseBackPropagation") << " Layer number: " << layerNumber << "\n";
		auto& layer = network[layerNumber];
		auto& activation = *i;		

		network.formatOutputForLayer(layer, deltas.back());

		auto activationDerivativeOfCurrentLayer = activation.sigmoidDerivative();
		auto deltaPropagatedReverse = layer.runReverse(deltas.back());
		
		// add in the sparsity term
		size_t samples = activation.rows();
		
		auto klDivergenceDerivative = activation.reduceSumAlongRows().multiply(1.0f/samples).klDivergenceDerivative(sparsity);

		auto sparsityTerm = klDivergenceDerivative.multiply(sparsityWeight);
	   
		delta = deltaPropagatedReverse.addBroadcastRow(sparsityTerm).elementMultiply(activationDerivativeOfCurrentLayer);

		++i; 
	}

	std::reverse(deltas.begin(), deltas.end());
	
	if(util::isLogEnabled("SparseBackPropagation::Detail"))
	{
		for(auto& delta : deltas)
		{
			util::log("SparseBackPropagation::Detail") << " added delta of size " << delta.shapeString() << "\n" ;
			//util::log("SparseBackPropagation") << " delta contains " << delta.toString() << "\n";
		}
	}

	return deltas;
}

static void coalesceNeuronOutputs(BlockSparseMatrix& derivative, const BlockSparseMatrix& skeleton)
{
	if(derivative.rowsPerBlock() == skeleton.columnsPerBlock() && derivative.blocks() == skeleton.blocks()) return;
	
	// Must be evenly divisible
	assert(derivative.rows() % skeleton.columns() == 0);
	assert(derivative.columns() == skeleton.rowsPerBlock());
	
	// Add the rows together in a block-cyclic fasion
	derivative = derivative.reduceTileSumAlongRows(skeleton.columnsPerBlock(),
		skeleton.blocks());
}

static BlockSparseMatrixVector computeCostDerivative(const NeuralNetwork& network, const BlockSparseMatrix& input,
	const BlockSparseMatrix& referenceOutput, float lambda, float sparsity, float sparsityWeight)
{
	//get activations in a vector
	auto activations = getActivations(network, input);
	//get deltas in a vector
	auto deltas = getDeltas(network, activations, referenceOutput, sparsity, sparsityWeight);
	
	BlockSparseMatrixVector partialDerivative;
	
	partialDerivative.reserve(2 * deltas.size());
	
	unsigned int samples = input.rows();

	//derivative of layer = activation[i] * delta[i+1] - for some layer i
	auto layer = network.begin();
	for (auto i = deltas.begin(), j = activations.begin(); i != deltas.end() && j != activations.end(); ++i, ++j, ++layer)
	{
		auto& activation = *j;
		auto& delta      = *i;

		auto  transposedDelta = delta.transpose();

		transposedDelta.setRowSparse();

		// there will be one less delta than activation
		auto unnormalizedPartialDerivative = (transposedDelta.reverseConvolutionalMultiply(activation));
		auto normalizedPartialDerivative = unnormalizedPartialDerivative.multiply(1.0f/samples);
		
		// add in the regularization term
		auto weights = layer->getWeightsWithoutBias();
		
		auto lambdaTerm = weights.multiply(lambda);
		
		// Account for cases where the same neuron produced multiple outputs
		//  or not enough inputs existed
		coalesceNeuronOutputs(normalizedPartialDerivative, lambdaTerm);
		
		auto regularizedPartialDerivative = lambdaTerm.add(normalizedPartialDerivative.transpose());
		
		partialDerivative.push_back(std::move(regularizedPartialDerivative));
	
		util::log("SparseBackPropagation") << " computed derivative for layer "
			<< std::distance(deltas.begin(), i)
			<< " (" << partialDerivative.back().rows()
			<< " rows, " << partialDerivative.back().columns() << " columns).\n";
		util::log("SparseBackPropagation") << " PD contains " << partialDerivative.back().toString() << "\n";
		
		// Compute partial derivatives with respect to the bias
		auto normalizedBiasPartialDerivative = transposedDelta.reduceSumAlongColumns().multiply(1.0f/samples);
		
		coalesceNeuronOutputs(normalizedBiasPartialDerivative, layer->getBias());
		
		partialDerivative.push_back(std::move(normalizedBiasPartialDerivative.transpose()));
	
	}//this loop ends after all activations are done. and we don't need the last delta (ref-output anyway)
	
	return partialDerivative;
}

static BlockSparseMatrix getInputDelta(const NeuralNetwork& network, const BlockSparseMatrixVector& activations,
	const BlockSparseMatrix& reference, float sparsity, float sparsityWeight)
{
	auto i = activations.rbegin();
	auto delta = (*i).subtract(reference).elementMultiply(i->sigmoidDerivative());
	++i;

	while (i + 1 != activations.rend())
	{
		unsigned int layerNumber = std::distance(activations.begin(), --(i.base()));
		auto& layer = network[layerNumber];
		auto& activation = *i;		
		
		network.formatOutputForLayer(layer, delta);

		auto activationDerivativeOfCurrentLayer = activation.sigmoidDerivative();
		auto deltaPropagatedReverse = layer.runReverse(delta);
		
		delta = deltaPropagatedReverse.elementMultiply(activationDerivativeOfCurrentLayer);

		util::log ("SparseBackPropagation") << " Computing input delta for layer number: " << layerNumber << "\n";

		++i; 
	}

	// Handle the first layer differently because the input does not have sigmoid applied
	unsigned int layerNumber = 0;
	auto& layer = network[layerNumber];

	network.formatOutputForLayer(layer, delta);

	auto deltaPropagatedReverse = layer.runReverse(delta);

	util::log ("SparseBackPropagation") << " Computing input delta for layer number: " << layerNumber << "\n";
	
	return deltaPropagatedReverse;	
}

static float getActivationSparsityCost(const NeuralNetwork& network, const BlockSparseMatrix& input,
	float sparsity, float sparsityWeight)
{
	float cost = 0.0f;
	
	auto temp = input;

	for(auto i = network.begin(); i != --network.end(); ++i)
	{
		auto& layer = *i;

		// The average activation of each neuron over all samples
		network.formatInputForLayer(layer, temp);
		
		temp = layer.runInputs(temp);
		
		size_t samples = temp.rows();
	
		auto averageActivations = temp.reduceSumAlongRows().multiply(1.0f/samples);

		// Get the KL divergence of each activation
		auto klDivergence = averageActivations.klDivergence(sparsity);
		
		if(util::isLogEnabled("SparseBackPropagation::Detail"))
		{
			util::log("SparseBackPropagation::Detail") << " activations of size " << temp.shapeString() << "\n" ;
			util::log("SparseBackPropagation::Detail") << " average activations of size " << averageActivations.shapeString() << "\n" ;
			util::log("SparseBackPropagation::Detail") << " kl divergence of size " << klDivergence.shapeString() << "\n" ;
			util::log("SparseBackPropagation::Detail") << " activations " << temp.debugString() << "\n" ;
			util::log("SparseBackPropagation::Detail") << " average activations " << averageActivations.debugString() << "\n" ;
			util::log("SparseBackPropagation::Detail") << " kl divergence " << klDivergence.toString() << "\n";
		}
	
		// Add it into the cost
		cost += sparsityWeight * klDivergence.reduceSum();
	}
	
	return cost;
}

static BlockSparseMatrix computeInputDerivative(const NeuralNetwork& network,
	const BlockSparseMatrix& input,
	const BlockSparseMatrix& referenceOutput,
	float lambda, float sparsity, float sparsityWeight)
{
	//get activations in a vector
	auto activations = getActivations(network, input);
	//get deltas in a vector
	auto delta = getInputDelta(network, activations, referenceOutput, sparsity, sparsityWeight);
	
	util::log("SparseBackPropagation") << "Input delta: " << delta.toString();
	unsigned int samples = input.rows();

	auto partialDerivative = delta;

	auto normalizedPartialDerivative = partialDerivative.multiply(1.0f/samples);

	util::log("SparseBackPropagation") << "Input derivative: " << normalizedPartialDerivative.toString();

	return normalizedPartialDerivative;
}
	
}

}

