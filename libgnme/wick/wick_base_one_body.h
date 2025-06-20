#include <cassert>
#include <algorithm>
#include <libgnme/utils/linalg.h>
#include "wick_base.h"

namespace libgnme {

template<typename Tc, typename Tf, typename Tb>
void wick_base<Tc,Tf,Tb>::spin_one_body(
    arma::umat xhp, arma::umat whp,
    Tc &F, bool alpha)
{
    // Ensure outputs are zero'd
    F = 0.0; 
    
    // Establish number of bra/ket excitations
    size_t nx = xhp.n_rows; // Bra excitations
    size_t nw = whp.n_rows; // Ket excitations

    // Get reference to number of zeros for this spin
    const size_t &nz = alpha ? m_orba.m_nz : m_orbb.m_nz; 
   
    // Get reference to relevant contractions
    const arma::field<arma::Mat<Tc> > &X = alpha ? m_orba.m_X : m_orbb.m_X;
    const arma::field<arma::Mat<Tc> > &Y = alpha ? m_orba.m_Y : m_orbb.m_Y;

    // Get reference to relevant one-body contractions
    const arma::Col<Tc> &F0 = alpha ? m_one_body_int->F0a : m_one_body_int->F0b;
    const arma::field<arma::Mat<Tc> > &XFX = alpha ? m_one_body_int->XFXa : m_one_body_int->XFXb;

    // Get dimensions of zero-contractions
    size_t dim = (nz > 0) ? 2 : 1; 

    // Check we don't have a non-zero element
    if(nz > nw + nx + 1) return;

    // Shift w indices
    // TODO: Do we want to keep this?
    const size_t &wshift = alpha ? m_orba.m_refx.m_nact : m_orbb.m_refx.m_nact;
    whp += wshift;

    // Get particle-hole indices
    arma::uvec rows, cols;
    if(nx == 0 xor nw == 0)
    {
        rows = (nx > 0) ? xhp.col(1) : whp.col(0);
        cols = (nx > 0) ? xhp.col(0) : whp.col(1);
    }
    else if(nx > 0 and nw > 0) 
    {
        rows = arma::join_cols(xhp.col(1),whp.col(0));
        cols = arma::join_cols(xhp.col(0),whp.col(1));
    }

    // Start with overlap contribution
    if(nx == 0 and nw == 0)
    {   // No excitations, so return simple overlap
        F = F0(nz);
    }
    else if((nx+nw) == 1)
    {   // One excitation doesn't require determinant
        // Distribute zeros over 2 contractions
        std::vector<size_t> m(nz, 1); m.resize(2, 0); 
        do {
            F += X(m[0])(rows(0),cols(0)) * F0(m[1]) - XFX(m[0],m[1])(rows(0),cols(0));
        } while(std::prev_permutation(m.begin(), m.end()));
    }
    else
    {   // General case does require determinant
        // Construct matrix for no zero overlaps
        arma::Mat<Tc> D  = arma::trimatl(X(0).submat(rows,cols))
                         + arma::trimatu(Y(0).submat(rows,cols),1);
        // Construct matrix with all zero overlaps
        arma::Mat<Tc> Db = arma::trimatl(X(1).submat(rows,cols)) 
                         + arma::trimatu(Y(1).submat(rows,cols),1);

        // Matrix of F contractions
        arma::field<arma::Mat<Tc> > Ftmp(dim,dim); 
        for(size_t i=0; i<dim; i++)
        for(size_t j=0; j<dim; j++)
            Ftmp(i,j) = XFX(i,j).submat(rows,cols);

        // Compute contribution from the overlap and zeroth term
        std::vector<size_t> m(nz, 1); m.resize(nx+nw+1, 0); 
        arma::Col<size_t> ind(&m[1], nx+nw, false, true);
        // Loop over all possible contributions of zero overlaps
        do {
            // Evaluate overlap contribution
            // TODO: Can we put a Shermann-Morrison update here as well?
            arma::Mat<Tc> Dtmp = D * arma::diagmat(1-ind) + Db * arma::diagmat(ind);

            // Get matrix adjoint and determinant
            Tc detDtmp;
            size_t nzero;
            arma::Mat<Tc> adjDtmp; 
            adjoint_matrix(Dtmp, adjDtmp, detDtmp, nzero);
            adjDtmp = adjDtmp.t(); // Transpose makes row access easier later

            // Get the overlap contributions 
            F += F0(m[0]) * detDtmp;
            
            // Loop over the column swaps for contracted terms
            for(size_t i=0; i < nx+nw; i++)
            {   
                // Get replace column vector
                arma::Col<Tc> v1(Ftmp(m[0],m[i+1]).colptr(i), nx+nw, false, true);
                arma::Col<Tc> v2(Dtmp.colptr(i), nx+nw, false, true);
                // Get relevant column from transposed inverse matrix
                arma::Col<Tc> a(adjDtmp.colptr(i), nx+nw, false, true); 
                // Perform Shermann-Morrison style update
                F -= (detDtmp + arma::dot(v1-v2, a));
            }
        } while(std::prev_permutation(m.begin(), m.end()));
    }

    // Shift w indices
    // TODO: Do we want to keep this?
    whp -= wshift;

    return;
}

} // namespace libgnme
