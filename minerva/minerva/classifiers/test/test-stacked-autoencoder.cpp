/*! \file   test-stacked-autoencoder.cpp
	\author Gregory Diamos <solusstultus@gmail.com>
	\date   Tuesday November 19, 2013
	\brief  A unit test for training a 6-layer stacked autoencoder.
*/

// Minerva Includes
#include <minerva/video/interface/Image.h>
#include <minerva/video/interface/ImageVector.h>
#include <minerva/neuralnetwork/interface/NeuralNetwork.h>

#include <minerva/matrix/interface/Matrix.h>

#include <minerva/visualization/interface/NeuronVisualizer.h>

#include <minerva/util/interface/debug.h>
#include <minerva/util/interface/paths.h>
#include <minerva/util/interface/ArgumentParser.h>

// Type definitions
typedef minerva::video::Image Image;
typedef minerva::neuralnetwork::NeuralNetwork NeuralNetwork;
typedef minerva::neuralnetwork::Layer Layer;
typedef minerva::video::ImageVector ImageVector;
typedef minerva::matrix::Matrix Matrix;
typedef minerva::visualization::NeuronVisualizer NeuronVisualizer;

static NeuralNetwork createNeuralNetwork(size_t xPixels, size_t yPixels,
	size_t colors, std::default_random_engine& engine)
{
	// Layer 1: (1024 16 x 16 ) sparse blocks   O(1024 * 256^3) O(1024 * 1e7) O(1e10)  O(256^2*1024) O(1e7)
	// Layer 2: (256  16 x 16 ) sparse blocks   O(1e9)                                 O(1e7)
	// Layer 3: (64   16 x 16 ) sparse blocks   O(1e8)                                 O(1e6)
	// Layer 4: (32   16 x 16 ) sparse blocks   O(1e8)                                 O(1e6)
	// Layer 5: (1    300)      fully connected O(1e8)                                 O(1e4)
	// Layer 6: (1    100)      fully connected O(1e8)                                 O(1e4)

	size_t reductionFactor = 4;

	assert(xPixels % reductionFactor == 0);
	assert(yPixels % reductionFactor == 0);
	
	NeuralNetwork network;

	// convolutional layer
	network.addLayer(Layer(colors * yPixels, xPixels, xPixels));

	// pooling layer
	network.addLayer(Layer(network.back().blocks(), network.back().getBlockingFactor(),
		network.back().getBlockingFactor() / reductionFactor));
	
	// convolutional layer
	network.addLayer(Layer(network.back().blocks() / reductionFactor, network.back().getBlockingFactor(), network.back().getBlockingFactor()));

	// pooling layer
	network.addLayer(Layer(network.back().blocks(), network.back().getBlockingFactor(),
		network.back().getBlockingFactor() / reductionFactor));

	// fully connected hidden layer
	network.addLayer(Layer(1, network.getOutputCount(), network.getOutputCount()));
	
	// final prediction layer
	network.addLayer(Layer(1, network.getOutputCount(), 1));

	network.initializeRandomly(engine);

	return network;
}

static Image generateRandomImage(size_t xPixels, size_t yPixels, size_t colors,
	std::default_random_engine& engine)
{
	Image image(xPixels, yPixels, colors, 1);
	
	std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);

	for(size_t y = 0; y != yPixels; ++y)
	{
		for(size_t x = 0; x != xPixels; ++x)
		{
			for(int c = 0; c != colors; ++c)
			{
				float value = distribution(engine);

				image.setStandardizedComponentAt(x, y, c, value);
			}
		}
	}
	
	return image;
}

static Image addRandomNoiseToImage(const Image& image, float noiseMagnitude,
	std::default_random_engine& engine)
{
	Image copy = image;

	size_t xPixels = image.x();
	size_t yPixels = image.y();
	size_t colors  = image.colorComponents();

	std::uniform_real_distribution<float> distribution(-noiseMagnitude, noiseMagnitude);
				
	for(size_t y = 0; y != yPixels; ++y)
	{
		for(size_t x = 0; x != xPixels; ++x)
		{
			for(int c = 0; c != colors; ++c)
			{
				float value = distribution(engine) + image.getStandardizedComponentAt(x, y, c);

				if(value < -1.0f)
				{
					value = -1.0f;
				}

				if(value > 1.0f)
				{
					value = 1.0f;
				}

				copy.setStandardizedComponentAt(x, y, c, value);
			}
		}
	}
	
	return copy;
}

static ImageVector generateBatch(const Image& image, float noiseMagnitude,
	size_t batchSize, std::default_random_engine& engine)
{
	ImageVector images;

	std::bernoulli_distribution distribution(0.5f);

	for(size_t i = 0; i != batchSize; ++i)
	{
		bool generateRandom = distribution(engine);

		if(generateRandom)
		{
			images.push_back(generateRandomImage(
				image.y(), image.x(), image.colorComponents(), engine));
		}
		else
		{
			images.push_back(addRandomNoiseToImage(image,
				noiseMagnitude, engine));
		}
	}
	
	return images;
}

static Matrix generateReference(const ImageVector& images)
{
	Matrix reference(images.size(), 1);
	
	for(size_t i = 0; i < images.size(); ++i)
	{
		reference(i, 0) = images[i].label() == "reference" ? 1.0f : 0.0f;
	
		//std::cout << "reference: " << reference(i, 0) << "\n";
	}
	
	return reference;
}

static void trainUsingGreedyStacking(NeuralNetwork& neuralNetwork, const Matrix& input, const Matrix& reference)
{
	NeuralNetwork copy;

	auto inputReference = input.add(1.0f).multiply(0.5f);
	auto layerInput = input;

	unsigned int counter = 0;

	for(auto layer = neuralNetwork.begin(); layer != neuralNetwork.end() && counter < 4; ++layer, ++counter)
	{
		copy.addLayer(*layer);
		
		copy.mirror();

		copy.train(layerInput, inputReference);
		
		copy.cutOffSecondHalf();
	
		layerInput = copy.runInputs(layerInput);
		inputReference = layerInput;

		*layer = copy.back();

		copy.clear();
	}

	neuralNetwork.train(input, reference);
}

static void trainNetwork(NeuralNetwork& neuralNetwork, const Image& image,
	float noiseMagnitude, size_t iterations, size_t batchSize,
	std::default_random_engine& engine)
{
	minerva::util::log("TestStackedAutoencoder") << "Training the network.\n";
	for(size_t i = 0; i != iterations; ++i)
	{
		minerva::util::log("TestStackedAutoencoder") << " Iteration " << i << " out of "
			<< iterations << "\n";
		ImageVector batch = generateBatch(image, noiseMagnitude,
			batchSize, engine);
		
		Matrix input = batch.convertToStandardizedMatrix(
			neuralNetwork.getInputCount());
		
		Matrix reference = generateReference(batch);
		
		minerva::util::log("TestStackedAutoencoder") << "  Input:     " << input.toString();
		minerva::util::log("TestStackedAutoencoder") << "  Reference: " << reference.toString();

		trainUsingGreedyStacking(neuralNetwork, input, reference);	
	}
}

static float testNetwork(NeuralNetwork& neuralNetwork, const Image& image,
	float noiseMagnitude, size_t iterations, size_t batchSize,
	std::default_random_engine& engine)
{
	float accuracy = 0.0f;

	iterations = std::max(iterations, 1UL);

	minerva::util::log("TestStackedAutoencoder") << "Testing the accuracy of the trained network.\n";

	for(size_t i = 0; i != iterations; ++i)
	{
		minerva::util::log("TestStackedAutoencoder") << " Iteration " << i << " out of "
			<< iterations << "\n";
		
		ImageVector batch = generateBatch(image, noiseMagnitude,
			batchSize, engine);
		
		Matrix input = batch.convertToStandardizedMatrix(
			neuralNetwork.getInputCount());
		
		Matrix reference = generateReference(batch);
		
		minerva::util::log("TestStackedAutoencoder") << "  Input:     " << input.toString();
		minerva::util::log("TestStackedAutoencoder") << "  Reference: " << reference.toString();
		
		accuracy += neuralNetwork.computeAccuracy(input, reference);
	}
	
	return accuracy * 100.0f / iterations;
}

static std::string rename(const std::string& name)
{
	return minerva::util::stripExtension(name) + "-reference" +
		minerva::util::getExtension(name);
}

static float visualizeNetwork(NeuralNetwork& neuralNetwork, const Image& referenceImage,
	const std::string& outputPath)
{
	// save the downsampled reference
	Image reference = referenceImage;
	
	reference.setPath(rename(outputPath));
	reference.save();

	NeuronVisualizer visualizer(&neuralNetwork);
	
	Image image = referenceImage;
	
	image.setPath(outputPath);
	
	visualizer.visualizeNeuron(image, 0);

	minerva::util::log("TestStackedAutoencoder") << "Reference response: "
		<< neuralNetwork.runInputs(referenceImage.convertToStandardizedMatrix(
			neuralNetwork.getInputCount())).toString();
	minerva::util::log("TestStackedAutoencoder") << "Visualized response: "
		<< neuralNetwork.runInputs(image.convertToStandardizedMatrix(
			neuralNetwork.getInputCount())).toString();
	
	image.save();

	return 0.0f;
}

static void runTest(const std::string& imagePath, float noiseMagnitude,
	size_t iterations, size_t batchSize, bool seedWithTime,
	size_t xPixels, size_t yPixels,
	size_t colors, const std::string& outputPath)
{
	std::default_random_engine randomNumberGenerator;

	if(seedWithTime)
	{
		randomNumberGenerator.seed(std::time(nullptr));
	}

	// create network
	/// one convolutional layer
	/// one output layer

	auto neuralNetwork = createNeuralNetwork(xPixels, yPixels, colors,
		randomNumberGenerator);

	// load image
	// create random image
	Image image(imagePath, "reference");
	image.load();

	image = image.downsample(xPixels, yPixels, colors);

	// iterate
	/// select default or random image
	/// add noise to image
	/// train
	trainNetwork(neuralNetwork, image, noiseMagnitude, iterations,
		batchSize, randomNumberGenerator);

	// test the network's predition ability
	float accuracy = testNetwork(neuralNetwork, image, noiseMagnitude, iterations,
		batchSize, randomNumberGenerator);

	std::cout << "Test accuracy was " << accuracy << "%\n";

	if(accuracy < 95.0)
	{
		std::cout << "Test Failed! Accuracy is too low.\n";
	}
	
	// visualize the output
	float visualizationAccuracy = visualizeNetwork(neuralNetwork, image,
		outputPath);
	
	std::cout << "Visualization accuracy was " << visualizationAccuracy << "%\n";
}

static void enableSpecificLogs(const std::string& modules)
{
	auto individualModules = minerva::util::split(modules, ",");
	
	for(auto& module : individualModules)
	{
		minerva::util::enableLog(module);
	}
}

int main(int argc, char** argv)
{
    minerva::util::ArgumentParser parser(argc, argv);
    
    bool verbose = false;
    bool seed = false;
    std::string loggingEnabledModules;
	size_t xPixels = 0;
	size_t yPixels = 0;
	size_t colors  = 0;
	size_t iterations = 0;
	size_t batchSize = 0;
	float noiseMagnitude = 0.0f;
	
	std::string image;
	std::string outputPath;

    parser.description("A test for minerva stacked neural network training.");

    parser.parse("-i", "--image", image, "images/cat.jpg",
        "The input image to train on, and perform visualization on.");
    parser.parse("-o", "--output-path", outputPath, "visualization/stacked-cat.jpg",
        "The output path to generate visualization results.");
    parser.parse("", "--iterations", iterations, 3,
        "The number of iterations to train the network for.");
    parser.parse("-b", "--batch-size", batchSize, 100,
        "The number of images to use for each iteration.");
    parser.parse("-n", "--noise-magnitude", noiseMagnitude, 0.01f,
        "The magnitude of noise to add to the image (0.0f - 1.0f).");
    parser.parse("-L", "--log-module", loggingEnabledModules, "",
		"Print out log messages during execution for specified modules "
		"(comma-separated list of modules, e.g. NeuralNetwork, Layer, ...).");
    parser.parse("-s", "--seed", seed, false, "Seed with time.");
    parser.parse("-x", "--x-pixels", xPixels, 64,
        "The number of X pixels to consider from the input image.");
	parser.parse("-y", "--y-pixels", yPixels, 64,
		"The number of Y pixels to consider from the input image");
	parser.parse("-c", "--colors", colors, 3,
		"The number of color components (e.g. RGB) to consider from the input image");
    parser.parse("-v", "--verbose", verbose, false,
        "Print out log messages during execution");

	parser.parse();

    if(verbose)
	{
		minerva::util::enableAllLogs();
	}
	else
	{
		enableSpecificLogs(loggingEnabledModules);
	}
    
    minerva::util::log("TestStackedAutoencoder") << "Test begins\n";
    
    try
    {
        runTest(image, noiseMagnitude, iterations,
			batchSize, seed, xPixels, yPixels, colors, outputPath);
    }
    catch(const std::exception& e)
    {
        std::cout << "Minerva Stacked Autoencoder Test Failed:\n";
        std::cout << "Message: " << e.what() << "\n\n";
    }

    return 0;
}



