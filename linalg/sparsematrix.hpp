#ifndef FILE_NGS_SPARSEMATRIX
#define FILE_NGS_SPARSEMATRIX

/**************************************************************************/
/* File:   sparsematrix.hpp                                               */
/* Author: Joachim Schoeberl                                              */
/* Date:   01. Oct. 94, 15 Jan. 02                                        */
/**************************************************************************/

#include <limits>

namespace ngla
{

  template<class TM>
  class SparseMatrixTM ;

  template<class TM, 
	   class TV_ROW = typename mat_traits<TM>::TV_ROW, 
	   class TV_COL = typename mat_traits<TM>::TV_COL>
  class SparseMatrix;


  template<class TM, 
	   class TV = typename mat_traits<TM>::TV_ROW>
  class SparseMatrixSymmetric;



  class BaseJacobiPrecond;

  template<class TM, 
	   class TV_ROW = typename mat_traits<TM>::TV_ROW, 
	   class TV_COL = typename mat_traits<TM>::TV_COL>
  class JacobiPrecond;

  template<class TM, 
	   class TV = typename mat_traits<TM>::TV_ROW>
  class JacobiPrecondSymmetric;


  class BaseBlockJacobiPrecond;

  template<class TM, 
	   class TV_ROW = typename mat_traits<TM>::TV_ROW, 
	   class TV_COL = typename mat_traits<TM>::TV_COL>
  class BlockJacobiPrecond;

  template<class TM, 
	   class TV = typename mat_traits<TM>::TV_ROW>
  class BlockJacobiPrecondSymmetric;





  
#ifdef USE_PARDISO
  const INVERSETYPE default_inversetype = PARDISO;
#else
#ifdef USE_MUMPS
  const INVERSETYPE default_inversetype = MUMPS;
#else
#ifdef USE_UMFPACK
  const INVERSETYPE default_inversetype = UMFPACK;
#else
  const INVERSETYPE default_inversetype = SPARSECHOLESKY;
#endif
#endif
#endif


#ifdef USE_NUMA
template <typename T>
class NumaDistributedArray : public Array<T>
{
  T * numa_ptr;
  size_t numa_size;
public:
  NumaDistributedArray () { numa_size = 0; numa_ptr = nullptr; }
  NumaDistributedArray (size_t s)
    : Array<T> (s, (T*)numa_alloc_local(s*sizeof(T)))
  {
    numa_ptr = this->data;
    numa_size = s;

    /* int avail = */ numa_available();   // initialize libnuma
    int num_nodes = numa_num_configured_nodes();
    size_t pagesize = numa_pagesize();
    
    int npages = ceil ( double(s)*sizeof(T) / pagesize );

    // cout << "size = " << numa_size << endl;
    // cout << "npages = " << npages << endl;

    for (int i = 0; i < num_nodes; i++)
      {
        int beg = (i * npages) / num_nodes;
        int end = ( (i+1) * npages) / num_nodes;
        // cout << "node " << i << " : [" << beg << "-" << end << ")" << endl;
        numa_tonode_memory(numa_ptr+beg*pagesize/sizeof(T), (end-beg)*pagesize, i);
      }
  }

  ~NumaDistributedArray ()
  {
    numa_free (numa_ptr, numa_size*sizeof(T));
  }

  NumaDistributedArray & operator= (NumaDistributedArray && a2)
  {
    Array<T>::operator= ((Array<T>&&)a2);  
  /*
    ngstd::Swap (this->size, a2.size);
    ngstd::Swap (this->data, a2.data);
    ngstd::Swap (this->allocsize, a2.allocsize);
    ngstd::Swap (this->mem_to_delete, a2.mem_to_delete);
  */
    ngstd::Swap (numa_ptr, a2.numa_ptr);
    ngstd::Swap (numa_size, a2.numa_size);
    return *this;
  }

  void Swap (NumaDistributedArray & b)
  {
    Array<T>::Swap(b);    
    ngstd::Swap (numa_ptr, b.numa_ptr);
    ngstd::Swap (numa_size, b.numa_size);
  }

  void SetSize (size_t size)
  {
    cerr << "************************* NumaDistArray::SetSize not overloaded" << endl;
    Array<T>::SetSize(size);
  }
};
#else

  template <typename T>
  using NumaDistributedArray = Array<T>;
  
#endif



  /** 
      The graph of a sparse matrix.
  */
  class NGS_DLL_HEADER MatrixGraph
  {
  protected:
    /// number of rows
    int size;
    /// with of matrix
    int width;
    /// non-zero elements
    size_t nze; 

    /// column numbers
    // Array<int, size_t> colnr;
    NumaDistributedArray<int> colnr;

    /// pointer to first in row
    Array<size_t> firsti;
  
    /// row has same non-zero elements as previous row
    Array<int> same_nze;
    
    /// balancing for multi-threading
    Partitioning balance;

    /// owner of arrays ?
    bool owner;

  public:
    /// arbitrary number of els/row
    MatrixGraph (const Array<int> & elsperrow, int awidth);
    /// matrix of height as, uniform number of els/row
    MatrixGraph (int as, int max_elsperrow);    
    /// shadow matrix graph
    MatrixGraph (const MatrixGraph & graph, bool stealgraph);
    /// 
    MatrixGraph (int size, const Table<int> & rowelements, const Table<int> & colelements, bool symmetric);
    /// 
    // MatrixGraph (const Table<int> & dof2dof, bool symmetric);
    virtual ~MatrixGraph ();

    /// eliminate unused columne indices (was never implemented)
    void Compress();
  
    /// returns position of Element (i, j), exception for unused
    size_t GetPosition (int i, int j) const;
    
    /// returns position of Element (i, j), -1 for unused
    size_t GetPositionTest (int i, int j) const;

    /// find positions of n sorted elements, overwrite pos, exception for unused
    void GetPositionsSorted (int row, int n, int * pos) const;

    /// returns position of new element
    size_t CreatePosition (int i, int j);

    int Size() const { return size; }

    size_t NZE() const { return nze; }

    FlatArray<int> GetRowIndices(int i) const
      // { return FlatArray<int> (int(firsti[i+1]-firsti[i]), &colnr[firsti[i]]); }
      // { return FlatArray<int> (int(firsti[i+1]-firsti[i]), &colnr[firsti[i]]); }
    { return FlatArray<int> (int(firsti[i+1]-firsti[i]), colnr+firsti[i]); }

    size_t First (int i) const { return firsti[i]; }

    void FindSameNZE();
    void CalcBalancing ();


    ostream & Print (ostream & ost) const;

    virtual void MemoryUsage (Array<MemoryUsageStruct*> & mu) const;
  };


  





  /// A virtual base class for all sparse matrices
  class NGS_DLL_HEADER BaseSparseMatrix : virtual public BaseMatrix, 
					  public MatrixGraph
  {
  protected:
    /// sparse direct solver
    mutable INVERSETYPE inversetype = default_inversetype;    // C++11 :-) Windows VS2013
    bool spd = false;
    
  public:
    BaseSparseMatrix (int as, int max_elsperrow)
      : MatrixGraph (as, max_elsperrow)  
    { ; }
    
    BaseSparseMatrix (const Array<int> & elsperrow, int awidth)
      : MatrixGraph (elsperrow, awidth) 
    { ; }

    BaseSparseMatrix (int size, const Table<int> & rowelements, 
		      const Table<int> & colelements, bool symmetric)
      : MatrixGraph (size, rowelements, colelements, symmetric)
    { ; }

    BaseSparseMatrix (const MatrixGraph & agraph, bool stealgraph)
      : MatrixGraph (agraph, stealgraph)
    { ; }   

    BaseSparseMatrix (const BaseSparseMatrix & amat)
      : BaseMatrix(amat), MatrixGraph (amat, 0)
    { ; }   

    virtual ~BaseSparseMatrix ();

    BaseSparseMatrix & operator= (double s)
    {
      AsVector() = s;
      return *this;
    }

    BaseSparseMatrix & Add (double s, const BaseSparseMatrix & m2)
    {
      AsVector() += s * m2.AsVector();
      return *this;
    }

    virtual shared_ptr<BaseJacobiPrecond> CreateJacobiPrecond (shared_ptr<BitArray> inner = nullptr) const 
    {
      throw Exception ("BaseSparseMatrix::CreateJacobiPrecond");
    }

    virtual shared_ptr<BaseBlockJacobiPrecond>
      CreateBlockJacobiPrecond (shared_ptr<Table<int>> blocks,
                                const BaseVector * constraint = 0,
                                bool parallel  = 1,
                                shared_ptr<BitArray> freedofs = NULL) const
    { 
      throw Exception ("BaseSparseMatrix::CreateBlockJacobiPrecond");
    }


    virtual shared_ptr<BaseMatrix>
      InverseMatrix (shared_ptr<BitArray> subset = nullptr) const
    { 
      throw Exception ("BaseSparseMatrix::CreateInverse called");
    }

    virtual shared_ptr<BaseMatrix>
    InverseMatrix (const Array<int> * clusters) const
    { 
      throw Exception ("BaseSparseMatrix::CreateInverse called");
    }

    virtual BaseSparseMatrix * Restrict (const SparseMatrixTM<double> & prol,
					 BaseSparseMatrix* cmat = NULL ) const
    {
      throw Exception ("BaseSparseMatrix::Restrict");
    }

    virtual INVERSETYPE SetInverseType ( INVERSETYPE ainversetype ) const
    {

      INVERSETYPE old_invtype = inversetype;
      inversetype = ainversetype; 
      return old_invtype;
    }

    virtual INVERSETYPE SetInverseType ( string ainversetype ) const;

    virtual INVERSETYPE  GetInverseType () const
    { return inversetype; }

    void SetSPD (bool aspd = true) { spd = aspd; }
    bool IsSPD () const { return spd; }
  };

  /// A general, sparse matrix
  template<class TM>
  class  NGS_DLL_HEADER SparseMatrixTM : public BaseSparseMatrix, 
					 public S_BaseMatrix<typename mat_traits<TM>::TSCAL>
  {
  protected:
    // Array<TM, size_t> data;
    NumaDistributedArray<TM> data;
    VFlatVector<typename mat_traits<TM>::TSCAL> asvec;
    TM nul;

  public:
    typedef typename mat_traits<TM>::TSCAL TSCAL;

    SparseMatrixTM (int as, int max_elsperrow)
      : BaseSparseMatrix (as, max_elsperrow),
	data(nze), nul(TSCAL(0))
    {
      ; 
    }

    SparseMatrixTM (const Array<int> & elsperrow, int awidth)
      : BaseSparseMatrix (elsperrow, awidth), 
	data(nze), nul(TSCAL(0))
    {
      ; 
    }

    SparseMatrixTM (int size, const Table<int> & rowelements, 
		    const Table<int> & colelements, bool symmetric)
      : BaseSparseMatrix (size, rowelements, colelements, symmetric), 
	data(nze), nul(TSCAL(0))
    { 
      ; 
    }

    SparseMatrixTM (const MatrixGraph & agraph, bool stealgraph)
      : BaseSparseMatrix (agraph, stealgraph), 
	data(nze), nul(TSCAL(0))
    { 
      FindSameNZE();
    }

    SparseMatrixTM (const SparseMatrixTM & amat)
    : BaseSparseMatrix (amat), 
      data(nze), nul(TSCAL(0))
    { 
      AsVector() = amat.AsVector(); 
    }
      
    virtual ~SparseMatrixTM ();

    int Height() const { return size; }
    int Width() const { return width; }
    virtual int VHeight() const { return size; }
    virtual int VWidth() const { return width; }

    TM & operator[] (int i)  { return data[i]; }
    const TM & operator[] (int i) const { return data[i]; }

    TM & operator() (int row, int col)
    {
      return data[CreatePosition(row, col)];
    }

    const TM & operator() (int row, int col) const
    {
      size_t pos = GetPositionTest (row,col);
      if (pos != numeric_limits<size_t>::max())
	return data[pos];
      else
	return nul;
    }

    FlatVector<TM> GetRowValues(int i) const
      // { return FlatVector<TM> (firsti[i+1]-firsti[i], &data[firsti[i]]); }
    { return FlatVector<TM> (firsti[i+1]-firsti[i], data+firsti[i]); }


    virtual void AddElementMatrix(FlatArray<int> dnums1, 
                                  FlatArray<int> dnums2, 
                                  FlatMatrix<TSCAL> elmat,
                                  bool use_atomic = false);

    virtual BaseVector & AsVector() 
    {
      // asvec.AssignMemory (nze*sizeof(TM)/sizeof(TSCAL), (void*)&data[0]);
      asvec.AssignMemory (nze*sizeof(TM)/sizeof(TSCAL), (void*)data.Addr(0));
      return asvec; 
    }

    virtual const BaseVector & AsVector() const
    { 
      const_cast<VFlatVector<TSCAL>&> (asvec).
	AssignMemory (nze*sizeof(TM)/sizeof(TSCAL), (void*)&data[0]);
      return asvec; 
    }

    virtual void SetZero();


    ///
    virtual ostream & Print (ostream & ost) const;

    ///
    virtual void MemoryUsage (Array<MemoryUsageStruct*> & mu) const;


  };
  



  template<class TM, class TV_ROW, class TV_COL>
  class NGS_DLL_HEADER SparseMatrix : virtual public SparseMatrixTM<TM>
  {
  public:
    using SparseMatrixTM<TM>::firsti;
    using SparseMatrixTM<TM>::colnr;
    using SparseMatrixTM<TM>::data;
    using SparseMatrixTM<TM>::balance;


    typedef typename mat_traits<TM>::TSCAL TSCAL;
    typedef TV_ROW TVX;
    typedef TV_COL TVY;

    ///
    SparseMatrix (int as, int max_elsperrow)
      : SparseMatrixTM<TM> (as, max_elsperrow) { ; }

    SparseMatrix (const Array<int> & aelsperrow)
      : SparseMatrixTM<TM> (aelsperrow, aelsperrow.Size()) { ; }

    SparseMatrix (const Array<int> & aelsperrow, int awidth)
      : SparseMatrixTM<TM> (aelsperrow, awidth) { ; }

    SparseMatrix (int size, const Table<int> & rowelements, 
		  const Table<int> & colelements, bool symmetric)
      : SparseMatrixTM<TM> (size, rowelements, colelements, symmetric) { ; }

    SparseMatrix (const MatrixGraph & agraph, bool stealgraph);
    // : SparseMatrixTM<TM> (agraph, stealgraph) { ; }

    SparseMatrix (const SparseMatrix & amat)
      : SparseMatrixTM<TM> (amat) { ; }

    SparseMatrix (const SparseMatrixTM<TM> & amat)
      : SparseMatrixTM<TM> (amat) { ; }

    virtual shared_ptr<BaseMatrix> CreateMatrix () const;
    // virtual BaseMatrix * CreateMatrix (const Array<int> & elsperrow) const;
    ///
    virtual AutoVector CreateVector () const;

    virtual shared_ptr<BaseJacobiPrecond>
      CreateJacobiPrecond (shared_ptr<BitArray> inner) const
    { 
      return make_shared<JacobiPrecond<TM,TV_ROW,TV_COL>> (*this, inner);
    }
    
    virtual shared_ptr<BaseBlockJacobiPrecond>
      CreateBlockJacobiPrecond (shared_ptr<Table<int>> blocks,
                                const BaseVector * constraint = 0,
                                bool parallel = 1,
                                shared_ptr<BitArray> freedofs = NULL) const
    { 
      return make_shared<BlockJacobiPrecond<TM,TV_ROW,TV_COL>> (*this, blocks );
    }

    virtual shared_ptr<BaseMatrix> InverseMatrix (shared_ptr<BitArray> subset = nullptr) const;
    virtual shared_ptr<BaseMatrix> InverseMatrix (const Array<int> * clusters) const;

    virtual BaseSparseMatrix * Restrict (const SparseMatrixTM<double> & prol,
					 BaseSparseMatrix* cmat = NULL ) const;

  
    ///
    // template<class TEL>
    inline TVY RowTimesVector (int row, const FlatVector<TVX> vec) const
    {
      typedef typename mat_traits<TVY>::TSCAL TTSCAL;
      TVY sum = TTSCAL(0);
      for (size_t j = firsti[row]; j < firsti[row+1]; j++)
	sum += data[j] * vec(colnr[j]);
      return sum;

      /*
        // no need for low-level tuning anymore thx compiler-tech
      int nj = firsti[row+1] - firsti[row];
      const int * colpi = &colnr[0]+firsti[row];
      const TM * datap = &data[0]+firsti[row];
      // FlatVector<TVX> hvec = vec;
      const TVX * vecp = vec.Addr(0);
    
      typedef typename mat_traits<TVY>::TSCAL TTSCAL;
      TVY sum = TTSCAL(0);
      for (int j = 0; j < nj; j++, colpi++, datap++)
        sum += *datap * vecp[*colpi];
      // for (int j = 0; j < nj; j++)
      // sum += datap[j] * vecp[colpi[j]];
      return sum;
      */
    }

    ///
    void AddRowTransToVector (int row, TVY el, FlatVector<TVX> vec) const
    {
      // cannot be optimized by the compiler, since hvec could alias
      // the matrix --> keyword 'restrict' will hopefully solve this problem

      // size_t first = firsti[row];
      // size_t last = firsti[row+1];
      // for (size_t j = first; j < last; j++)
      // vec(colnr[j]) += Trans(data[j]) * el;

      /*
      const int * colpi = &colnr[0];
      const TM * datap = &data[0];
      TVX * vecp = vec.Addr(0);
      for (size_t j = first; j < last; j++)
        vecp[colpi[j]] += Trans(datap[j]) * el; 
      */

      size_t first = firsti[row];
      size_t last = firsti[row+1];
      // TVX * vecp = vec.Addr(0);
      // const int * colpi = &colnr[0];
      // const TM * datap = &data[0];
      const int * colpi = colnr.Addr(0);
      const TM * datap = data.Addr(0);

      // int d = vec.Addr(1)-vec.Addr(0);
      // if (d == 1)
        for (size_t j = first; j < last; j++)
          vec[colpi[j]] += Trans(datap[j]) * el; 
        // else
        // for (size_t j = first; j < last; j++)
        // vec[d*colnr[j]] += Trans(data[j]) * el; 
    }


    virtual void MultAdd (double s, const BaseVector & x, BaseVector & y) const;
    virtual void MultTransAdd (double s, const BaseVector & x, BaseVector & y) const;
    virtual void MultAdd (Complex s, const BaseVector & x, BaseVector & y) const;
    virtual void MultTransAdd (Complex s, const BaseVector & x, BaseVector & y) const;

    virtual void DoArchive (Archive & ar);
  };


  /// A symmetric sparse matrix
  template<class TM>
  class NGS_DLL_HEADER SparseMatrixSymmetricTM : virtual public SparseMatrixTM<TM>
  {
  protected:
    SparseMatrixSymmetricTM (int as, int max_elsperrow)
      : SparseMatrixTM<TM> (as, max_elsperrow) { ; }

    SparseMatrixSymmetricTM (const Array<int> & elsperrow)
      : SparseMatrixTM<TM> (elsperrow, elsperrow.Size()) { ; }

    SparseMatrixSymmetricTM (int size, const Table<int> & rowelements)
      : SparseMatrixTM<TM> (size, rowelements, rowelements, true) { ; }

    SparseMatrixSymmetricTM (const MatrixGraph & agraph, bool stealgraph)
      : SparseMatrixTM<TM> (agraph, stealgraph) { ; }

    SparseMatrixSymmetricTM (const SparseMatrixSymmetricTM & amat)
      : SparseMatrixTM<TM> (amat) { ; }

  public:
    typedef typename mat_traits<TM>::TSCAL TSCAL;
    virtual void AddElementMatrix(FlatArray<int> dnums, FlatMatrix<TSCAL> elmat, bool use_atomic = false);

    virtual void AddElementMatrix(FlatArray<int> dnums1, 
				  FlatArray<int> dnums2, 
				  FlatMatrix<TSCAL> elmat,
                                  bool use_atomic = false)
    {
      AddElementMatrix (dnums1, elmat, use_atomic);
    }
  };


  /// A symmetric sparse matrix
  template<class TM, class TV>
  class NGS_DLL_HEADER SparseMatrixSymmetric : virtual public SparseMatrixSymmetricTM<TM>, 
					       virtual public SparseMatrix<TM,TV,TV>
  {

  public:
    using SparseMatrixTM<TM>::firsti;
    using SparseMatrixTM<TM>::colnr;
    using SparseMatrixTM<TM>::data;

    typedef typename mat_traits<TM>::TSCAL TSCAL;
    typedef TV TV_COL;
    typedef TV TV_ROW;
    typedef TV TVY;
    typedef TV TVX;

    SparseMatrixSymmetric (int as, int max_elsperrow)
      : SparseMatrixTM<TM> (as, max_elsperrow) , 
	SparseMatrixSymmetricTM<TM> (as, max_elsperrow),
	SparseMatrix<TM,TV,TV> (as, max_elsperrow)
    { ; }
  
    SparseMatrixSymmetric (const Array<int> & elsperrow)
      : SparseMatrixTM<TM> (elsperrow, elsperrow.Size()), 
	SparseMatrixSymmetricTM<TM> (elsperrow),
	SparseMatrix<TM,TV,TV> (elsperrow, elsperrow.Size())
    { ; }

    SparseMatrixSymmetric (int size, const Table<int> & rowelements)
      : SparseMatrixTM<TM> (size, rowelements, rowelements, true),
	SparseMatrixSymmetricTM<TM> (size, rowelements),
	SparseMatrix<TM,TV,TV> (size, rowelements, rowelements, true)
    { ; }

    SparseMatrixSymmetric (const MatrixGraph & agraph, bool stealgraph);
    /*
      : SparseMatrixTM<TM> (agraph, stealgraph), 
	SparseMatrixSymmetricTM<TM> (agraph, stealgraph),
	SparseMatrix<TM,TV,TV> (agraph, stealgraph)
    { ; }
    */
    SparseMatrixSymmetric (const SparseMatrixSymmetric & amat)
      : SparseMatrixTM<TM> (amat), 
	SparseMatrixSymmetricTM<TM> (amat),
	SparseMatrix<TM,TV,TV> (amat)
    { 
      this->AsVector() = amat.AsVector(); 
    }

    SparseMatrixSymmetric (const SparseMatrixSymmetricTM<TM> & amat)
      : SparseMatrixTM<TM> (amat), 
	SparseMatrixSymmetricTM<TM> (amat),
	SparseMatrix<TM,TV,TV> (amat)
    { 
      this->AsVector() = amat.AsVector(); 
    }

  





    ///
    virtual ~SparseMatrixSymmetric ();

    SparseMatrixSymmetric & operator= (double s)
    {
      this->AsVector() = s;
      return *this;
    }

    virtual shared_ptr<BaseMatrix> CreateMatrix () const
    {
      return make_shared<SparseMatrixSymmetric> (*this);
    }

    /*
    virtual BaseMatrix * CreateMatrix (const Array<int> & elsperrow) const
    {
      return new SparseMatrix<TM,TV,TV>(elsperrow);
    }
    */

    virtual shared_ptr<BaseJacobiPrecond> CreateJacobiPrecond (shared_ptr<BitArray> inner) const
    { 
      return make_shared<JacobiPrecondSymmetric<TM,TV>> (*this, inner);
    }

    virtual shared_ptr<BaseBlockJacobiPrecond>
      CreateBlockJacobiPrecond (shared_ptr<Table<int>> blocks,
                                const BaseVector * constraint = 0,
                                bool parallel  = 1,
                                shared_ptr<BitArray> freedofs = NULL) const
    { 
      return make_shared<BlockJacobiPrecondSymmetric<TM,TV>> (*this, blocks);
    }


    virtual BaseSparseMatrix * Restrict (const SparseMatrixTM<double> & prol,
					 BaseSparseMatrix* cmat = NULL ) const;

    ///
    virtual void MultAdd (double s, const BaseVector & x, BaseVector & y) const;

    virtual void MultTransAdd (double s, const BaseVector & x, BaseVector & y) const
    {
      MultAdd (s, x, y);
    }


    /*
      y += s L * x
    */
    virtual void MultAdd1 (double s, const BaseVector & x, BaseVector & y,
			   const BitArray * ainner = NULL,
			   const Array<int> * acluster = NULL) const;


    /*
      y += s (D + L^T) * x
    */
    virtual void MultAdd2 (double s, const BaseVector & x, BaseVector & y,
			   const BitArray * ainner = NULL,
			   const Array<int> * acluster = NULL) const;
    




    using SparseMatrix<TM,TV,TV>::RowTimesVector;
    using SparseMatrix<TM,TV,TV>::AddRowTransToVector;


    TV_COL RowTimesVectorNoDiag (int row, const FlatVector<TVX> vec) const
    {
      size_t last = firsti[row+1];
      size_t first = firsti[row];
      if (last == first) return TVY(0);
      if (colnr[last-1] == row) last--;

      typedef typename mat_traits<TVY>::TSCAL TTSCAL;
      TVY sum = TTSCAL(0);

      for (size_t j = first; j < last; j++)
	sum += data[j] * vec(colnr[j]);
      return sum;

      /*
      const int * colpi = &colnr[0]; // +firsti[row];
      const TM * datap = &data[0]; // +firsti[row];
      const TVX * vecp = vec.Addr(0);

      // for (int j = first; j < last; j++, colpi++, datap++)
      // sum += *datap * vecp[*colpi];
      for (int j = first; j < last; j++)
	sum += datap[j] * vecp[colpi[j]];
      return sum;
      */
    }

    void AddRowTransToVectorNoDiag (int row, TVY el, FlatVector<TVX> vec) const
    {
      size_t first = firsti[row];
      size_t last = firsti[row+1];
      if (first == last) return;
      if (this->colnr[last-1] == row) last--;
      
      /*
      for (size_t j = first; j < last; j++)
        vec(colnr[j]) += Trans(data[j]) * el;
      */
      /*
      // hand tuning
      const int * colpi = &colnr[0]; // +firsti[row];
      const TM * datap = &data[0]; // +firsti[row];
      TVX * vecp = vec.Addr(0);
      for (size_t j = first; j < last; j++)
	vecp[colpi[j]] += Trans(datap[j]) * el;
      */
      // TVX * vecp = vec.Addr(0);
      // int d = vec.Addr(1)-vec.Addr(0);
      // if (d == 1)
      for (size_t j = first; j < last; j++)
        vec[colnr[j]] += Trans(data[j]) * el;
        // else
        // for (size_t j = first; j < last; j++)
        // vecp[d*colnr[j]] += Trans(data[j]) * el;
    }
  
    BaseSparseMatrix & AddMerge (double s, const SparseMatrixSymmetric  & m2);

    virtual shared_ptr<BaseMatrix> InverseMatrix (shared_ptr<BitArray> subset = nullptr) const;
    virtual shared_ptr<BaseMatrix> InverseMatrix (const Array<int> * clusters) const;
  };

  SparseMatrixTM<double> *
  MatMult (const SparseMatrix<double, double, double> & mata, const SparseMatrix<double, double, double> & matb);

#ifdef GOLD
#include <sparsematrix_spec.hpp>
#endif

  
#ifdef FILE_SPARSEMATRIX_CPP
#define SPARSEMATRIX_EXTERN
#else
#define SPARSEMATRIX_EXTERN extern
  

  SPARSEMATRIX_EXTERN template class SparseMatrix<double>;
  SPARSEMATRIX_EXTERN template class SparseMatrix<Complex>;
  SPARSEMATRIX_EXTERN template class SparseMatrix<double, Complex, Complex>;

#if MAX_SYS_DIM >= 1
  SPARSEMATRIX_EXTERN template class SparseMatrix<Mat<1,1,double> >;
  SPARSEMATRIX_EXTERN template class SparseMatrix<Mat<1,1,Complex> >;
#endif
#if MAX_SYS_DIM >= 2
  SPARSEMATRIX_EXTERN template class SparseMatrix<Mat<2,2,double> >;
  SPARSEMATRIX_EXTERN template class SparseMatrix<Mat<2,2,Complex> >;
#endif
#if MAX_SYS_DIM >= 3
  SPARSEMATRIX_EXTERN template class SparseMatrix<Mat<3,3,double> >;
  SPARSEMATRIX_EXTERN template class SparseMatrix<Mat<3,3,Complex> >;
#endif
#if MAX_SYS_DIM >= 4
  SPARSEMATRIX_EXTERN template class SparseMatrix<Mat<4,4,double> >;
  SPARSEMATRIX_EXTERN template class SparseMatrix<Mat<4,4,Complex> >;
#endif
#if MAX_SYS_DIM >= 5
  SPARSEMATRIX_EXTERN template class SparseMatrix<Mat<5,5,double> >;
  SPARSEMATRIX_EXTERN template class SparseMatrix<Mat<5,5,Complex> >;
#endif
#if MAX_SYS_DIM >= 6
  SPARSEMATRIX_EXTERN template class SparseMatrix<Mat<6,6,double> >;
  SPARSEMATRIX_EXTERN template class SparseMatrix<Mat<6,6,Complex> >;
#endif
#if MAX_SYS_DIM >= 7
  SPARSEMATRIX_EXTERN template class SparseMatrix<Mat<7,7,double> >;
  SPARSEMATRIX_EXTERN template class SparseMatrix<Mat<7,7,Complex> >;
#endif
#if MAX_SYS_DIM >= 8
  SPARSEMATRIX_EXTERN template class SparseMatrix<Mat<8,8,double> >;
  SPARSEMATRIX_EXTERN template class SparseMatrix<Mat<8,8,Complex> >;
#endif





  SPARSEMATRIX_EXTERN template class SparseMatrixSymmetric<double>;
  SPARSEMATRIX_EXTERN template class SparseMatrixSymmetric<Complex>;
  SPARSEMATRIX_EXTERN template class SparseMatrixSymmetric<double, Complex>;


#if MAX_SYS_DIM >= 1
  SPARSEMATRIX_EXTERN template class SparseMatrixSymmetric<Mat<1,1,double> >;
  SPARSEMATRIX_EXTERN template class SparseMatrixSymmetric<Mat<1,1,Complex> >;
#endif
#if MAX_SYS_DIM >= 2
  SPARSEMATRIX_EXTERN template class SparseMatrixSymmetric<Mat<2,2,double> >;
  SPARSEMATRIX_EXTERN template class SparseMatrixSymmetric<Mat<2,2,Complex> >;
#endif
#if MAX_SYS_DIM >= 3
  SPARSEMATRIX_EXTERN template class SparseMatrixSymmetric<Mat<3,3,double> >;
  SPARSEMATRIX_EXTERN template class SparseMatrixSymmetric<Mat<3,3,Complex> >;
#endif
#if MAX_SYS_DIM >= 4
  SPARSEMATRIX_EXTERN template class SparseMatrixSymmetric<Mat<4,4,double> >;
  SPARSEMATRIX_EXTERN template class SparseMatrixSymmetric<Mat<4,4,Complex> >;
#endif
#if MAX_SYS_DIM >= 5
  SPARSEMATRIX_EXTERN template class SparseMatrixSymmetric<Mat<5,5,double> >;
  SPARSEMATRIX_EXTERN template class SparseMatrixSymmetric<Mat<5,5,Complex> >;
#endif
#if MAX_SYS_DIM >= 6
  SPARSEMATRIX_EXTERN template class SparseMatrixSymmetric<Mat<6,6,double> >;
  SPARSEMATRIX_EXTERN template class SparseMatrixSymmetric<Mat<6,6,Complex> >;
#endif
#if MAX_SYS_DIM >= 7
  SPARSEMATRIX_EXTERN template class SparseMatrixSymmetric<Mat<7,7,double> >;
  SPARSEMATRIX_EXTERN template class SparseMatrixSymmetric<Mat<7,7,Complex> >;
#endif
#if MAX_SYS_DIM >= 8
  SPARSEMATRIX_EXTERN template class SparseMatrixSymmetric<Mat<8,8,double> >;
  SPARSEMATRIX_EXTERN template class SparseMatrixSymmetric<Mat<8,8,Complex> >;
#endif



#endif


}

#endif