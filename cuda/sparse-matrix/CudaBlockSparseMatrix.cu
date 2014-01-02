/*! \file   CudaBlockSparseMatrix.cu
	\author Gregory Diamos
	\date   Tuesday December 31, 2013
	\brief  The source file for the cuda block sparse matrix class.
*/


extern "C" __global__ void multiplyFloat(float* result, float* input, float value, uint64_t size)
{
	uint64_t step  = blockDim.x * gridDim.x;
	uint64_t start = blockIdx.x * blockDim.x + threadIdx.x;

	for(uint64_t i = start; i < size; i += step)
	{
		result[i] = input[i] * value;
	}
}

extern "C" __global__ void elementMultiply(float* result, float* left, float* right, float value, uint64_t size)
{
	uint64_t step  = blockDim.x * gridDim.x;
	uint64_t start = blockIdx.x * blockDim.x + threadIdx.x;

	for(uint64_t i = start; i < size; i += step)
	{
		result[i] = left[i] * right[i];
	}
}

extern "C" __global__ void add(float* result, float* left, float* right, float value, uint64_t size)
{
	uint64_t step  = blockDim.x * gridDim.x;
	uint64_t start = blockIdx.x * blockDim.x + threadIdx.x;

	for(uint64_t i = start; i < size; i += step)
	{
		result[i] = left[i] + right[i];
	}
}

extern "C" __global__ void addBroadcastRowRowSparse(float* result, float* left, float* right,
	uint64_t blocks, uint64_t rows, uint64_t columns)
{
	uint64_t step  = blockDim.x * gridDim.x;
	uint64_t start = blockIdx.x * blockDim.x + threadIdx.x;
	uint64_t size  = blocks * rows * columns;
	
	uint64_t blockSize = rows * columns;

	for(uint64_t i = start; i < size; i += step)
	{
		// TODO: try to optimize these out
		uint64_t indexInBlock = i % blockSize;
		uint64_t columnIndex  = indexInBlock % columns;
		
		float leftValue  = left[i];
		float rightValue = right[columnIndex];
		
		result[i] = leftValue + rightValue;
	}
}

extern "C" __global__ void addBroadcastRowColumnSparse(float* result, float* left, float* right,
	uint64_t blocks, uint64_t rows, uint64_t columns)
{
	uint64_t step  = blockDim.x * gridDim.x;
	uint64_t start = blockIdx.x * blockDim.x + threadIdx.x;
	uint64_t size  = blocks * rows * columns;

	uint64_t totalColumns = blocks * columns;

	for(uint64_t i = start; i < size; i += step)
	{
		// TODO: try to optimize these out
		uint64_t columnIndex = i % totalColumns;
		
		float leftValue  = left[i];
		float rightValue = right[columnIndex];
		
		result[i] = leftValue + rightValue;
	}
}

extern "C" __global__ void addFloat(float* result, float* input, float value, uint64_t size)
{
	uint64_t step  = blockDim.x * gridDim.x;
	uint64_t start = blockIdx.x * blockDim.x + threadIdx.x;

	for(uint64_t i = start; i < size; i += step)
	{
		result[i] = input[i] + value;
	}
}

extern "C" __global__ void subtract(float* result, float* left, float* right, float value, uint64_t size)
{
	uint64_t step  = blockDim.x * gridDim.x;
	uint64_t start = blockIdx.x * blockDim.x + threadIdx.x;

	for(uint64_t i = start; i < size; i += step)
	{
		result[i] = left[i] - right[i];
	}
}

extern "C" __global__ void subtractFloat(float* result, float* input, float value, uint64_t size)
{
	uint64_t step  = blockDim.x * gridDim.x;
	uint64_t start = blockIdx.x * blockDim.x + threadIdx.x;

	for(uint64_t i = start; i < size; i += step)
	{
		result[i] = input[i] - value;
	}
}

__device__ float computeKlDivergence(float value, float sparsity)
{
	const float epsilon = 1.0e-5;
	
	if(value > (1.0f - epsilon)) value = 1.0f - epsilon;
	if(value < epsilon         ) value = epsilon;

	float result = sparsity * __logf(sparsity / value) +
		(1.0f - sparsity) * __logf((1.0f - sparsity) / (1.0f - value));
	
	//assert(!std::isnan(result));

	return result;
}

__device__ float computeKlDivergenceDerivative(float value, float sparsity)
{
	const float epsilon = 1.0e-5;
	
	if(value > (1.0f - epsilon)) value = 1.0f - epsilon;
	if(value < epsilon         ) value = epsilon;

	float result = (-sparsity / value + (1.0f - sparsity)/(1.0f - value));

	//assert(!std::isnan(result));

	return result;
}

extern "C" __global__ void klDivergence(float* result, float* input, float value, uint64_t size)
{
	uint64_t step  = blockDim.x * gridDim.x;
	uint64_t start = blockIdx.x * blockDim.x + threadIdx.x;

	for(uint64_t i = start; i < size; i += step)
	{
		result[i] = computeKlDivergence(input[i], value);
	}
}

extern "C" __global__ void klDivergenceDerivative(float* result, float* input, float value, uint64_t size)
{
	uint64_t step  = blockDim.x * gridDim.x;
	uint64_t start = blockIdx.x * blockDim.x + threadIdx.x;

	for(uint64_t i = start; i < size; i += step)
	{
		result[i] = computeKlDivergenceDerivative(input[i], value);
	}
}

extern "C" __global__ void negate(float* result, uint64_t size)
{
	uint64_t step  = blockDim.x * gridDim.x;
	uint64_t start = blockIdx.x * blockDim.x + threadIdx.x;

	for(uint64_t i = start; i < size; i += step)
	{
		result[i] = -result[i];
	}
}

extern "C" __global__ void logArray(float* result, uint64_t size)
{
	uint64_t step  = blockDim.x * gridDim.x;
	uint64_t start = blockIdx.x * blockDim.x + threadIdx.x;

	for(uint64_t i = start; i < size; i += step)
	{
		result[i] = __logf(result[i]);
	}
}

__device__ float computeSigmoid(float v)
{
	if(v < -50.0f) return 0.0f;
    if(v > 50.0f)  return 1.0f;
    
    return 1.0f / (1.0f + __expf(-v)); 
}

extern "C" __global__ void sigmoid(float* result, uint64_t size)
{
	uint64_t step  = blockDim.x * gridDim.x;
	uint64_t start = blockIdx.x * blockDim.x + threadIdx.x;

	for(uint64_t i = start; i < size; i += step)
	{
		result[i] = computeSigmoid(result[i]);
	}
}

__device__ float computeSigmoidDerivative(float v)
{
	return v * (1.0f - v);
}

extern "C" __global__ void sigmoidDerivative(float* result, uint64_t size)
{
	uint64_t step  = blockDim.x * gridDim.x;
	uint64_t start = blockIdx.x * blockDim.x + threadIdx.x;

	for(uint64_t i = start; i < size; i += step)
	{
		result[i] = computeSigmoidDerivative(result[i]);
	}
}

extern "C" __global__ void scaleRandom(float* result, float min, float max, uint64_t size)
{
	uint64_t step  = blockDim.x * gridDim.x;
	uint64_t start = blockIdx.x * blockDim.x + threadIdx.x;
	
	float scale = max - min;
	
	float mean = (scale / 2.0f);
	
	for(uint64_t i = start; i < size; i += step)
	{
		float value = result[i];
		
		// center around 0.0f (-0.5f, 0.5f)
		value -= 0.5f;
		
		// expand the range
		value *= scale;
		
		// center around the new mean
		value += mean;

		result[i] = value;
	}
}

extern "C" __global__ void greaterThanOrEqual(float* result, float* input, float value, uint64_t size)
{
	uint64_t step  = blockDim.x * gridDim.x;
	uint64_t start = blockIdx.x * blockDim.x + threadIdx.x;

	for(uint64_t i = start; i < size; i += step)
	{
		result[i] = input[i] >= value ? 1.0f : 0.0f;
	}
}

extern "C" __global__ void equals(float* result, float* left, float* right, uint64_t size)
{
	uint64_t step  = blockDim.x * gridDim.x;
	uint64_t start = blockIdx.x * blockDim.x + threadIdx.x;

	for(uint64_t i = start; i < size; i += step)
	{
		result[i] = left[i] == right[i] ? 1.0f : 0.0f;
	}
}

const uint32_t maxCtaSize = 1024;

__device__ float reduceCta(float value)
{
	__shared__ float buffer[maxCtaSize];
	
	buffer[threadIdx.x] = value;
	
	for(uint32_t offset = 1; offset < blockDim.x; offset *= 2)
	{
		__syncthreads();
		
		uint32_t neighbor = threadIdx.x + offset;
		
		if(threadIdx.x % (offset * 2) != 0) continue;
		if(neighbor >= blockDim.x)          continue;
		
		buffer[threadIdx.x] += buffer[neighbor];
	}

	__syncthreads();

	return buffer[0];
}

extern "C" __global__ void reduceSum(float* result, float* input, uint64_t size)
{
	uint64_t step  = blockDim.x * gridDim.x;
	uint64_t start = blockIdx.x * blockDim.x + threadIdx.x;

	float localValue = 0.0f;

	for(uint64_t i = start; i < size; i += step)
	{
		localValue += input[i];
	}
	
	// CTA reduction
	float reducedValue = reduceCta(localValue);
	
	// atomic add
	if(threadIdx.x == 0)
	{
		atomicAdd(result, localValue);
	}
}

__device__ uint64_t align(uint64_t address, uint64_t alignment)
{
	uint64_t remainder = address % alignment;
	return remainder == 0 ? address : address + alignment - remainder;
}

__device__ float ctaSegmentedReduce(float value, bool isStart)
{
	__shared__ float buffer   [maxCtaSize];
	__shared__ bool  isClaimed[maxCtaSize];
	
	buffer   [threadIdx.x] = value;
	isClaimed[threadIdx.x] = isStart;
	
	for(uint32_t offset = 1; offset < blockDim.x; offset *= 2)
	{
		__syncthreads();
		
		uint32_t neighbor = threadIdx.x + offset;
		
		if(threadIdx.x % (offset * 2) != 0) continue;
		if(neighbor >= blockDim.x)          continue;
		
		if(!isClaimed[neighbor])
		{
			isClaimed[threadIdx.x] = isClaimed[neighbor];
			buffer[threadIdx.x] += buffer[neighbor];
		}
	}
	
	__syncthreads();
	
	return buffer[threadIdx.x];
}

extern "C" __global__ void reduceSumAlongRowsRowSparse(float* result, float* input, uint64_t blocks, uint64_t rows, uint64_t columns)
{
	uint64_t size  = blocks * rows * columns;
	uint64_t step  = blockDim.x * gridDim.x;
	uint64_t start = blockIdx.x * blockDim.x + threadIdx.x;
	
	for(uint64_t i = start; i < size; i += step)
	{
		float value = input[i];
		
		bool isLeader = ((i % columns) == 0) || threadIdx.x == 0;
		
		float finalValue = ctaSegmentedReduce(value, isLeader);
		
		if(isLeader)
		{
			uint64_t row = i / columns;
			
			atomicAdd(result + row, finalValue);
		}
	}
}

extern "C" __global__ void reduceSumAlongRowsColumnSparse(float* result, float* input, uint64_t blocks, uint64_t rows, uint64_t columns)
{
	uint64_t step  = gridDim.x;
	uint64_t start = blockIdx.x;
	
	uint64_t size = blocks;

	for(uint64_t i = start; i < size; i += step)
	{
		float value = 0.0f;
		
		uint64_t ctaStart = threadIdx.x;
		uint64_t ctaStep  = blockDim.x;
		uint64_t ctaSize  = columns * blocks;
		
		for(uint64_t j = ctaStart; j < ctaSize; j += ctaStep)
		{	
			uint64_t blockSize = rows * columns;
			uint64_t blockId = j / columns;
			uint64_t rowOffset = i * columns;
			uint64_t columnOffset = j % columns;			

			uint64_t index = blockSize * blockId + rowOffset + columnOffset;
			
			value += input[index];
		}
		
		float finalValue = reduceCta(value);
		
		if(threadIdx.x == 0)
		{
			result[i] = finalValue;
		}
	}
}

extern "C" __global__ void reduceSumAlongColumnsRowSparse(float* result, float* input, uint64_t blocks, uint64_t rows, uint64_t columns)
{
	uint64_t step  = blockDim.x * gridDim.x;
	uint64_t start = blockIdx.x * blockDim.x + threadIdx.x;
	
	uint64_t totalSize = blocks * rows * columns;
	
	uint64_t chunkSize = align(step, columns);

	for(uint64_t i = start; i < chunkSize && i < totalSize; i += step)
	{
		float value = 0.0f;
		uint64_t column = i % columns;		

		for(uint64_t j = i; j < totalSize; j += chunkSize)
		{
			value += input[j];
		}
		
		atomicAdd(result + column, value);
	}
}

extern "C" __global__ void reduceSumAlongColumnsColumnSparse(float* result, float* input, uint64_t blocks, uint64_t rows, uint64_t columns)
{
	uint64_t step  = blockDim.x * gridDim.x;
	uint64_t start = blockIdx.x * blockDim.x + threadIdx.x;
	
	uint64_t totalSize    = blocks * rows * columns;
	uint64_t totalColumns = blocks * columns;

	for(uint64_t i = start; i < totalColumns; i += step)
	{
		uint64_t column = i;		

		float value = 0.0f;

		for(uint64_t j = column; j < totalSize; j += totalColumns)
		{
			value += input[j];
		}
		
		result[i] = value;
	}
}



