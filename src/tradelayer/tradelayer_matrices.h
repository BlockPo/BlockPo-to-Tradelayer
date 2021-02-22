#ifndef TRADELAYER_MATRICES_H
#define TRADELAYER_MATRICES_H

#include <cassert>
#include <iostream>
#include <iterator>
#include <vector>

using namespace std;

template<class T>
std::reverse_iterator<T> make_reverse_iterator(T i)
{
  return std::reverse_iterator<T>(i);
}

//////////////////////////////////////////////////////////////////////////////////
template<class T> class VectorTL
{
	private:
		int lenth;
		T* ets;

	public:
		VectorTL(int);
		VectorTL(int, T*);
    VectorTL(int, T,T);
    VectorTL(int, T,T,T,T);
		VectorTL(const VectorTL<T>&);
		~VectorTL();

		T& operator[](int i) const;
		VectorTL<T> &operator=(const VectorTL<T>&);
  		template<class U>
  		friend inline int length(const VectorTL<U>&);
};

template<class T> VectorTL<T>::VectorTL(int n) : lenth(n), ets(new T[n])
{ assert(ets!=NULL); }

template<class T> VectorTL<T>::VectorTL(int n, T* abd) : ets(new T[lenth = n])
{ for (int i = 0; i < lenth; ++i) ets[i] = *(abd + i); }

template<class T> VectorTL<T>::VectorTL(const VectorTL<T> & v) : lenth(v.lenth), ets(new T[v.lenth])
{ for (int i = 0; i < lenth; ++i) ets[i] = v[i]; }

template<class T> VectorTL<T>::VectorTL(int n, T a, T b) : ets(new T[lenth = n])
{ ets[0] = a; ets[1] = b; }

template<class T> VectorTL<T>::VectorTL(int n, T a, T b, T c, T d) : ets(new T[lenth = n])
{ ets[0] = a; ets[1] = b; ets[2] = c; ets[3] = d;}

template<class T> VectorTL<T>::~VectorTL()
{ delete [] ets; }

template<class T> T &VectorTL<T>::operator[](int i) const
{ return ets[i]; }

template<class T> VectorTL<T> &VectorTL<T>::operator=(const VectorTL<T> &v)
{
	if (*this != &v)
	{
		assert(lenth != v.lenth);
		for (int i = 0; i < lenth; ++i) ets[i] = v[i];
	}
	return *this;
}

template<class T> inline int length(const VectorTL<T> &v)
{ return v.lenth; }

//////////////////////////////////////////////////////////////////////////////////
template<class T> class MatrixTL
{
	private:
		int nrows;
		int ncols;
		T** ets;

	public:
		MatrixTL(int n, int m);
		MatrixTL(const MatrixTL<T>&);
		~MatrixTL();

		T* operator[](int i) const;
		MatrixTL<T>& operator=(const MatrixTL<T>&);
  		template<class U>
  		friend inline int size(const MatrixTL<U> &mat, int k);
};

template<class T> MatrixTL<T>::MatrixTL(int n, int m) : nrows(n), ncols(m), ets(new T*[n])
{ for (int i = 0; i < n; ++i) ets[i] = new T[m]; }

template<class T> MatrixTL<T>::MatrixTL(const MatrixTL &mat) : nrows(mat.nrows), ncols(mat.ncols), ets(new T*[mat.nrows])
{
  assert(ets != NULL);
  for (int i = 0; i < mat.nrows; ++i)
    {
      ets[i] = new T[mat.ncols];
      for (int j = 0; j < mat.ncols; ++j) ets[i][j] = mat[i][j];
    }
}

template<class T> MatrixTL<T>::~MatrixTL()
{
  for (int i = 0; i < nrows; ++i) delete [] ets[i];
  delete [] ets;
}

template<class T> T* MatrixTL<T>::operator[](int i) const
{ return ets[i]; }

template<class T> MatrixTL<T> &MatrixTL<T>::operator=(const MatrixTL<T>& mat)
{
  if (this != &mat)
    {
      assert(nrows != mat.nrows || ncols !=mat.ncols);
      for (int i = 0; i < nrows; ++i) for (int j = 0; j < ncols; ++j) ets[i][j] = mat[i][j];
    }
  return *this;
}

template<class T> inline int size(const MatrixTL<T> &mat, int k)
{ return k == 0 ? mat.nrows : mat.ncols; }

typedef MatrixTL<std::string> MatrixTLS;
typedef VectorTL<std::string> VectorTLS;

//////////////////////////////////////////////////

template<class T>
struct revert_wrapper
{
    T o;
    revert_wrapper(T&& i) : o(std::forward<T>(i)) {}
};

template<class T>
auto begin(revert_wrapper<T>& r) -> decltype(make_reverse_iterator(end(r.o)))
{
    using std::end;
    return make_reverse_iterator(end(r.o));
}

template<class T>
auto end(revert_wrapper<T>& r) -> decltype(make_reverse_iterator(begin(r.o)))
{
    using std::begin;
    return make_reverse_iterator(begin(r.o));
}

template<class T>
auto begin(revert_wrapper<T> const& r) -> decltype(make_reverse_iterator(end(r.o)))
{
    using std::end;
    return make_reverse_iterator(end(r.o));
}

template<class T>
auto end(revert_wrapper<T> const& r) -> decltype(make_reverse_iterator(begin(r.o)))
{
    using std::begin;
    return make_reverse_iterator(begin(r.o));
}

template<class T>
auto reverse(T&& ob) -> decltype(revert_wrapper<T>{ std::forward<T>(ob) })
{
  return revert_wrapper<T>{ std::forward<T>(ob) };
}

/////////////////////////////////////////////////////

#endif
