// cudabasetypes.h -- basic types used on both CUDA and PC side
//
// F. Seide, V-hansu

#pragma once

#include <cuda_runtime_api.h>       // __host__, __device__
#include <cuda.h>
#ifdef __CUDA_ARCH__         // we are compiling under CUDA
#define ON_CUDA 1
#else
#define ON_CUDA 0           // TODO: this does not work for some combination--fix this
#ifndef __device__
#define __device__
#endif
#ifndef __host__
#define __host__
#endif
#endif

#include <assert.h>

namespace msra { namespace cuda {

typedef size_t cuda_size_t; // TODO: verify if this is consistent across CPU/CUDA, or use uint32 or so

// we wrap CUDA pointers so that we don't accidentally use them in CPU code
template<typename T> class cuda_ptr
{
    T * p;  // CUDA pointers are the same as host (e.g. Win32 is restricted to 32-bit CUDA pointers)
public:
    void swap (cuda_ptr & other) { T * tmp = p; p = other.p; other.p = tmp; }
//#if ON_CUDA    // CUDA side: allow access as if it is a pointer
    //operator T* () { return p; }
    //T & operator* () { return *p; }
    //T * operator-> () { return p; }
    //T ** operator& () { return &p; }
    __device__ T &       operator[] (size_t index)       { return p[index]; }
    __device__ const T & operator[] (size_t index) const { return p[index]; }
    __device__ __host__ cuda_ptr operator+ (size_t index) const { return cuda_ptr (p + index); }
    __device__ __host__ cuda_ptr operator- (size_t index) const { return cuda_ptr (p - index); }
    // 'const' versions
//#else       // PC-side: only allow allocation
    cuda_ptr (T * pp) : p (pp) {}
    //void reset (T * pp) { p = pp; }
    T * get() const { return p; }
//#endif
};

// reference to a vector (without allocation) that lives in CUDA RAM
// This can be directly passed by value to CUDA functions.
template<typename T> class vectorref
{
    cuda_ptr<T> p;      // pointer in CUDA space of this device
    cuda_size_t n;      // number of elements
public:
    __device__ __host__ size_t size() const throw() { return n; }
//#if ON_CUDA    // CUDA side: allow access as if it is a pointer
    __device__ T &       operator[] (size_t i)       { return p[i]; }
    __device__ const T & operator[] (size_t i) const { return p[i]; }
//#else
    cuda_ptr<T> get() const throw() { return p; }
    cuda_ptr<T> reset (cuda_ptr<T> pp, size_t nn) throw() { p.swap (pp); n = nn; return pp; }
    vectorref (cuda_ptr<T> pp, size_t nn) : p (pp), n (nn) { }
    vectorref() : p (0), n (0) { }
//#endif
};

// reference to a matrix
template<typename T> class matrixref
{
protected:
    cuda_ptr<T> p;      // pointer in CUDA space of this device
    size_t numrows;     // rows()
    size_t numcols;     // cols()
    size_t colstride;   // height of column = rows() rounded to multiples of 4
    __device__ __host__ size_t locate (size_t i, size_t j) const {
        /*if (j >= numrows || i >= numcols)
            *((int*)-1)=0;*/
        return j * colstride + i; 
    }   // matrix in column-wise storage
    matrixref() : p (0), numrows (0), numcols (0), colstride (0) {}
public:
    matrixref(T* p, size_t numRows, size_t numCols, size_t colStride)
        : p(p), numrows(numRows), numcols(numCols), colstride(colStride)
    {
    }
    cuda_ptr<T> get() const throw() { return p; }
    __device__ __host__ size_t rows() const throw() { return numrows; }
    __device__ __host__ size_t cols() const throw() { return numcols; }
    __device__ __host__ void reshape(const size_t newrows, const size_t newcols) { assert (rows() * cols() == newrows * newcols); numrows=newrows; numcols = newcols;};
    __device__ __host__ size_t getcolstride() const throw() { return colstride; }
//#if ON_CUDA    // CUDA side: allow access as if it is a pointer
    __device__ T &       operator() (size_t i, size_t j)       { return p[locate(i,j)]; }
    __device__ const T & operator() (size_t i, size_t j) const { return p[locate(i,j)]; }
//#endif
};

// reference to a CUDA array for use with textures
// It is set up and destroyed elsewhere, and used through the textureref class.
template<typename T> class cudaarrayref
{
protected:
    cudaArray * a;          // pointer to cudaArray object
    size_t numrows;         // rows()
    size_t numcols;         // cols()
    cudaarrayref() : a (NULL), numrows (0), numcols (0) {}
public:
    __device__ __host__ size_t rows() const throw() { return numrows; }
    __device__ __host__ size_t cols() const throw() { return numcols; }
    cudaArray * get() const { return a; }
};

// using a cudaarrayref
// Pattern:
//  - do not declare the texture as an argument to the kernel, instead:
//  - at file scope:
//    texture<float, 2, cudaReadModeElementType> texref;
//  - right before kernel launch:
//    passtextureref texref (texref, cudaarrayref);    // use the same name as that global texref one, so it will match the name inside the kernel
class passtextureref
{
    textureReference & texref;  // associated texture reference if any
public:
    template<typename R,class T>
    passtextureref (R texref, cudaarrayref<T> cudaarrayref) : texref (texref)
    {
        texref.addressMode[0] = cudaAddressModeWrap;
        texref.addressMode[1] = cudaAddressModeWrap;
        texref.filterMode     = cudaFilterModePoint;
        texref.normalized     = false;
        cudaError_t rc = cudaBindTextureToArray (texref, cudaarrayref.get());
        if (rc != cudaSuccess)
        {
            char buf[1000];
            sprintf_s (buf, "passtextureref: %s (cuda error %d)", cudaGetErrorString (rc), rc);
            throw std::runtime_error (buf);
        }
    }
    ~passtextureref()
    {
        cudaUnbindTexture (&texref);
    }
};

};};