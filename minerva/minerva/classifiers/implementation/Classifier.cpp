/* Author: Sudnya Padalikar
 * Date  : 08/10/2013
 * The implementation of the class to classify test images into gestures
*/

#include <minerva/classifiers/interface/Classifier.h>
#include <minerva/util/interface/Debug.h>

#include <algorithm>
#include <cassert>

namespace minerva
{
namespace classifiers
{

GestureVector Classifier::classify(const ImageVector& images)
{
    loadFeatureSelector();
    loadClassifier();
    Matrix m = detectGestures(images);
    GestureVector gestureName = pickMostLikelyGesture(m);
    return gestureName;
}

unsigned Classifier::getInputFeatureCount()
{
	loadFeatureSelector();
	
	return m_featureSelectorNetwork.getInputCount();
}

void Classifier::loadFeatureSelector()
{
    /* read from the feature file into memory/variable */
    m_featureSelectorNetwork = m_classificationModel.getNeuralNetwork("FeatureSelector");
}

void Classifier::loadClassifier()
{
    /* read from the classifier file into memory/variable */
    m_classifierNetwork = m_classificationModel.getNeuralNetwork("Classifier");
}

Classifier::Matrix Classifier::detectGestures(const ImageVector& images)
{
    assert(m_classifierNetwork.getInputCount() == m_featureSelectorNetwork.getOutputCount());
    
    /* run classification using features, classifier network to emit gesture */
    auto matrix = images.convertToMatrix(m_featureSelectorNetwork.getInputCount());

    assert(matrix.columns() == m_featureSelectorNetwork.getInputCount());

    auto featureMatrix = m_featureSelectorNetwork.runInputs(matrix);
    auto gestureMatrix = m_classifierNetwork.runInputs(featureMatrix);

    return gestureMatrix;    
}

GestureVector Classifier::pickMostLikelyGesture(const Matrix& likelyGestures)
{
    /* some algorithm to pick the best value out of the input vector of likely gestures */
    //until we come up with a sophisticated heuristic, just return the max
    /*auto maxIter = max_element(likelyGestures.begin(), likelyGestures.end());
    if (maxIter == likelyGestures.end)
        return "Could not determine gesture accurately\n";
    return *maxIter;*/
    GestureVector gestureList;
    unsigned int totalRows = likelyGestures.rows();

    util::log("Classifier") << "Finding gestures for each image\n";

    for (unsigned i = 0; i < totalRows; ++i)
    {
        auto gestureNeurons = likelyGestures.getRow(i);
        
        util::log("Classifier") << " for image " << i << "\n";
        
        for(auto neuronOutput = gestureNeurons.begin(); neuronOutput != gestureNeurons.end(); ++neuronOutput)
        {
            util::log("Classifier") << "   neuron " << std::distance(gestureNeurons.begin(), neuronOutput) << " : " << *neuronOutput << "\n";
        }

        auto maxNeuron = max_element(gestureNeurons.begin(), gestureNeurons.end());
        std::string name = m_classifierNetwork.getLabelForOutputNeuron(std::distance(gestureNeurons.begin(),maxNeuron));
        gestureList.push_back(name);
    }
    
    return gestureList;
}

}//end classifiers

}//end minerva
