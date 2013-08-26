/* Author: Sudnya Padalikar
 * Date  : 08/09/2013
 * The implementation of the class to learn from raw video & features to classifiers 
 */

#include <minerva/classifiers/interface/Learner.h>

namespace minerva
{
namespace classifiers
{

void Learner::learnAndTrain(const ImageVector& images)
{
    loadFeatureSelector();
    loadClassifier();
    trainClassifier(images);
    writeClassifier();
}

unsigned Learner::getInputFeatureCount()
{
    loadFeatureSelector();
    
    return m_featureSelectorNetwork.getInputCount();
}

void Learner::loadFeatureSelector()
{
    /* read from the feature file into memory/variable */
    m_featureSelectorNetwork = m_classificationModel.getNeuralNetwork("FeatureSelector");
}

void Learner::loadClassifier()
{
    m_classifierNetwork = m_classificationModel.getNeuralNetwork("Classifier");
}

void Learner::trainClassifier(const ImageVector& images)
{
    /* using the feature NN & training images emit a NN for classifiers */
    auto matrix           = images.convertToMatrix(m_featureSelectorNetwork.getInputCount());
    auto featureMatrix    = m_featureSelectorNetwork.runInputs(matrix);

    // TODO: optimize weights using back propagation as tool
    m_classifierNetwork.backPropagate(featureMatrix, images.getReference());
}
 
void Learner::writeClassifier()
{
    /* write out m_classifiers to disk */
    m_classificationModel.setNeuralNetwork("Classifier", m_classifierNetwork);
}

} //end classifiers
} //end minerva

