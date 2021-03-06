/*! \file   NaiveBlockSparseMatrix.h
	\author Gregory Diamos
	\date   Sunday December 29, 2013
	\brief  The header file for the NaiveBlockSparseMatrix class.
*/

#pragma once

// Minerva Includes
#include <minerva/matrix/interface/BlockSparseMatrixImplementation.h>

namespace minerva
{

namespace matrix
{

/*! \brief A straightforward implementation of block sparse matrix on top of matrix */
class NaiveBlockSparseMatrix : public BlockSparseMatrixImplementation
{
public:
	explicit NaiveBlockSparseMatrix(size_t blocks, size_t rows,
		size_t columns, bool rowSparse);
	explicit NaiveBlockSparseMatrix(bool rowSparse);

public: 
	virtual Value* multiply(const Value* m) const;
	virtual Value* convolutionalMultiply(const Value* m, size_t step) const;
	virtual Value* reverseConvolutionalMultiply(const Value* m) const;
	virtual Value* multiply(float f) const;
	virtual Value* elementMultiply(const Value* m) const;

	virtual Value* add(const Value* m) const;
	virtual Value* addBroadcastRow(const Value* m) const;
	virtual Value* convolutionalAddBroadcastRow(const Value* m) const;
	virtual Value* add(float f) const;

	virtual Value* subtract(const Value* m) const;
	virtual Value* subtract(float f) const;

	virtual Value* log() const;
	virtual Value* negate() const;
	virtual Value* sigmoid() const;
	virtual Value* sigmoidDerivative() const;
	
	virtual Value* klDivergence(float sparsity) const;
	virtual Value* klDivergenceDerivative(float sparsity) const;

public:
	virtual Value* transpose() const;

public:
	virtual void negateSelf();
	virtual void logSelf();
    virtual void sigmoidSelf();
    virtual void sigmoidDerivativeSelf();
    
	virtual void minSelf(float value);
	virtual void maxSelf(float value);

	virtual void assignSelf(float value);

	virtual void transposeSelf();

	virtual void assignUniformRandomValues(std::default_random_engine& engine,
		float min, float max);

public:
	virtual Value* greaterThanOrEqual(float f) const;
	virtual Value* equals(const Value* m) const;

public:
    virtual float reduceSum() const;
	virtual Value* reduceSumAlongColumns() const;
	virtual Value* reduceSumAlongRows() const;
	virtual Value* reduceTileSumAlongRows(size_t tilesPerRow, size_t blocks) const;

public:
	virtual Value* clone() const;

public:
	static bool isSupported();

};

}

}


