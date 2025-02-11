
#include <AMReX_MLABecLaplacian.H>
#include <AMReX_MultiFabUtil.H>

#include <AMReX_MLABecLap_K.H>

namespace amrex {

MLABecLaplacian::MLABecLaplacian (const Vector<Geometry>& a_geom,
                                  const Vector<BoxArray>& a_grids,
                                  const Vector<DistributionMapping>& a_dmap,
                                  const LPInfo& a_info,
                                  const Vector<FabFactory<FArrayBox> const*>& a_factory,
                                  const int a_ncomp)
{
    define(a_geom, a_grids, a_dmap, a_info, a_factory, a_ncomp);
}

MLABecLaplacian::MLABecLaplacian (const Vector<Geometry>& a_geom,
                                  const Vector<BoxArray>& a_grids,
                                  const Vector<DistributionMapping>& a_dmap,
                                  const Vector<iMultiFab const*>& a_overset_mask,
                                  const LPInfo& a_info,
                                  const Vector<FabFactory<FArrayBox> const*>& a_factory,
                                  const int a_ncomp)
{
    define(a_geom, a_grids, a_dmap, a_overset_mask, a_info, a_factory, a_ncomp);
}

void
MLABecLaplacian::define (const Vector<Geometry>& a_geom,
                         const Vector<BoxArray>& a_grids,
                         const Vector<DistributionMapping>& a_dmap,
                         const LPInfo& a_info,
                         const Vector<FabFactory<FArrayBox> const*>& a_factory,
                         const int a_ncomp)
{
    BL_PROFILE("MLABecLaplacian::define()");
    m_ncomp = a_ncomp;
    MLCellABecLap::define(a_geom, a_grids, a_dmap, a_info, a_factory);
    define_ab_coeffs();
}

void
MLABecLaplacian::define (const Vector<Geometry>& a_geom,
                         const Vector<BoxArray>& a_grids,
                         const Vector<DistributionMapping>& a_dmap,
                         const Vector<iMultiFab const*>& a_overset_mask,
                         const LPInfo& a_info,
                         const Vector<FabFactory<FArrayBox> const*>& a_factory,
                         const int a_ncomp)
{
    BL_PROFILE("MLABecLaplacian::define(overset)");
    m_ncomp = a_ncomp;
    MLCellABecLap::define(a_geom, a_grids, a_dmap, a_overset_mask, a_info, a_factory);
    define_ab_coeffs();
}

void
MLABecLaplacian::define_ab_coeffs ()
{
    const int ncomp = getNComp();

    m_a_coeffs.resize(m_num_amr_levels);
    m_b_coeffs.resize(m_num_amr_levels);
    for (int amrlev = 0; amrlev < m_num_amr_levels; ++amrlev)
    {
        m_a_coeffs[amrlev].resize(m_num_mg_levels[amrlev]);
        m_b_coeffs[amrlev].resize(m_num_mg_levels[amrlev]);
        for (int mglev = 0; mglev < m_num_mg_levels[amrlev]; ++mglev)
        {
            m_a_coeffs[amrlev][mglev].define(m_grids[amrlev][mglev],
                                             m_dmap[amrlev][mglev],
                                             1, 0, MFInfo(), *m_factory[amrlev][mglev]);
            for (int idim = 0; idim < AMREX_SPACEDIM; ++idim)
            {
                const BoxArray& ba = amrex::convert(m_grids[amrlev][mglev],
                                                    IntVect::TheDimensionVector(idim));
                m_b_coeffs[amrlev][mglev][idim].define(ba,
                                                       m_dmap[amrlev][mglev],
                                                       ncomp, 0, MFInfo(), *m_factory[amrlev][mglev]);
            }
        }
    }
}

MLABecLaplacian::~MLABecLaplacian ()
{}

void
MLABecLaplacian::setScalars (Real a, Real b) noexcept
{
    m_a_scalar = a;
    m_b_scalar = b;
    if (a == 0.0)
    {
        for (int amrlev = 0; amrlev < m_num_amr_levels; ++amrlev)
        {
            m_a_coeffs[amrlev][0].setVal(0.0);
        }
    }
}

void
MLABecLaplacian::setACoeffs (int amrlev, const MultiFab& alpha)
{
    AMREX_ALWAYS_ASSERT_WITH_MESSAGE(alpha.nComp() == 1,
                                     "MLABecLaplacian::setACoeffs: alpha is supposed to be single component.");
    MultiFab::Copy(m_a_coeffs[amrlev][0], alpha, 0, 0, 1, 0);
    m_needs_update = true;
}

void
MLABecLaplacian::setACoeffs (int amrlev, Real alpha)
{
    m_a_coeffs[amrlev][0].setVal(alpha);
    m_needs_update = true;
}

void
MLABecLaplacian::setBCoeffs (int amrlev,
                             const Array<MultiFab const*,AMREX_SPACEDIM>& beta)
{
    const int ncomp = getNComp();
    AMREX_ALWAYS_ASSERT(beta[0]->nComp() == 1 || beta[0]->nComp() == ncomp);
    if (beta[0]->nComp() == ncomp)
        for (int idim = 0; idim < AMREX_SPACEDIM; ++idim) {
            for (int icomp = 0; icomp < ncomp; ++icomp) {
                MultiFab::Copy(m_b_coeffs[amrlev][0][idim], *beta[idim], icomp, icomp, 1, 0);
            }
        }
    else
        for (int idim = 0; idim < AMREX_SPACEDIM; ++idim) {
            for (int icomp = 0; icomp < ncomp; ++icomp) {
                MultiFab::Copy(m_b_coeffs[amrlev][0][idim], *beta[idim], 0, icomp, 1, 0);
            }
        }
    m_needs_update = true;
}

void
MLABecLaplacian::setBCoeffs (int amrlev, Real beta)
{
    for (int idim = 0; idim < AMREX_SPACEDIM; ++idim) {
        m_b_coeffs[amrlev][0][idim].setVal(beta);
    }
    m_needs_update = true;
}

void
MLABecLaplacian::setBCoeffs (int amrlev, Vector<Real> const& beta)
{
    const int ncomp = getNComp();
    for (int idim = 0; idim < AMREX_SPACEDIM; ++idim) {
        for (int icomp = 0; icomp < ncomp; ++icomp) {
            m_b_coeffs[amrlev][0][idim].setVal(beta[icomp]);
        }
    }
    m_needs_update = true;
}

void
MLABecLaplacian::averageDownCoeffs ()
{
    BL_PROFILE("MLABecLaplacian::averageDownCoeffs()");

    for (int amrlev = m_num_amr_levels-1; amrlev > 0; --amrlev)
    {
        auto& fine_a_coeffs = m_a_coeffs[amrlev];
        auto& fine_b_coeffs = m_b_coeffs[amrlev];

        averageDownCoeffsSameAmrLevel(amrlev, fine_a_coeffs, fine_b_coeffs);
        averageDownCoeffsToCoarseAmrLevel(amrlev);
    }

    averageDownCoeffsSameAmrLevel(0, m_a_coeffs[0], m_b_coeffs[0]);
}

void
MLABecLaplacian::averageDownCoeffsSameAmrLevel (int amrlev, Vector<MultiFab>& a,
                                                Vector<Array<MultiFab,AMREX_SPACEDIM> >& b)
{
    int nmglevs = a.size();
    for (int mglev = 1; mglev < nmglevs; ++mglev)
    {
        IntVect ratio = (amrlev > 0) ? IntVect(mg_coarsen_ratio) : mg_coarsen_ratio_vec[mglev-1];

        if (m_a_scalar == 0.0)
        {
            a[mglev].setVal(0.0);
        }
        else
        {
            amrex::average_down(a[mglev-1], a[mglev], 0, 1, ratio);
        }

        Vector<const MultiFab*> fine {AMREX_D_DECL(&(b[mglev-1][0]),
                                                   &(b[mglev-1][1]),
                                                   &(b[mglev-1][2]))};
        Vector<MultiFab*> crse {AMREX_D_DECL(&(b[mglev][0]),
                                             &(b[mglev][1]),
                                             &(b[mglev][2]))};

        amrex::average_down_faces(fine, crse, ratio, 0);
    }

    for (int mglev = 1; mglev < nmglevs; ++mglev)
    {
        if (m_overset_mask[amrlev][mglev]) {
            const Real fac = static_cast<Real>(1 << mglev); // 2**mglev
            const Real osfac = Real(2.0)*fac/(fac+Real(1.0));
            const int ncomp = getNComp();
#ifdef AMREX_USE_OMP
#pragma omp parallel if (Gpu::notInLaunchRegion())
#endif
            for (MFIter mfi(a[mglev],TilingIfNotGPU()); mfi.isValid(); ++mfi)
            {
                AMREX_D_TERM(Box const& xbx = mfi.nodaltilebox(0);,
                             Box const& ybx = mfi.nodaltilebox(1);,
                             Box const& zbx = mfi.nodaltilebox(2));
                AMREX_D_TERM(Array4<Real> const& bx = b[mglev][0].array(mfi);,
                             Array4<Real> const& by = b[mglev][1].array(mfi);,
                             Array4<Real> const& bz = b[mglev][2].array(mfi));
                Array4<int const> const& osm = m_overset_mask[amrlev][mglev]->const_array(mfi);
                AMREX_LAUNCH_HOST_DEVICE_LAMBDA_DIM
                    (xbx, t_xbx,
                     {
                         overset_rescale_bcoef_x(t_xbx, bx, osm, ncomp, osfac);
                     },
                     ybx, t_ybx,
                     {
                         overset_rescale_bcoef_y(t_ybx, by, osm, ncomp, osfac);
                     },
                     zbx, t_zbx,
                     {
                         overset_rescale_bcoef_z(t_zbx, bz, osm, ncomp, osfac);
                     });
            }
        }
    }
}

void
MLABecLaplacian::averageDownCoeffsToCoarseAmrLevel (int flev)
{
    auto& fine_a_coeffs = m_a_coeffs[flev  ].back();
    auto& fine_b_coeffs = m_b_coeffs[flev  ].back();
    auto& crse_a_coeffs = m_a_coeffs[flev-1].front();
    auto& crse_b_coeffs = m_b_coeffs[flev-1].front();

    if (m_a_scalar != 0.0) {
        // We coarsen from the back of flev to the front of flev-1.
        // So we use mg_coarsen_ratio.
        amrex::average_down(fine_a_coeffs, crse_a_coeffs, 0, 1, mg_coarsen_ratio);
    }

    amrex::average_down_faces(amrex::GetArrOfConstPtrs(fine_b_coeffs),
                              amrex::GetArrOfPtrs(crse_b_coeffs),
                              IntVect(mg_coarsen_ratio), m_geom[flev-1][0]);
}

void
MLABecLaplacian::applyMetricTermsCoeffs ()
{
#if (AMREX_SPACEDIM != 3)
    for (int alev = 0; alev < m_num_amr_levels; ++alev)
    {
        const int mglev = 0;
        applyMetricTerm(alev, mglev, m_a_coeffs[alev][mglev]);
        for (int idim = 0; idim < AMREX_SPACEDIM; ++idim)
        {
            applyMetricTerm(alev, mglev, m_b_coeffs[alev][mglev][idim]);
        }
    }
#endif
}

//
// Suppose we are solving `alpha u - del (beta grad u) = rhs` (Scalar
// coefficients can be easily added back in the end) and there is Robin BC
// `a u + b du/dn = f` at the upper end of the x-direction.  The 1D
// discretization at the last cell i is
//
//    alpha u_i + (beta_{i-1/2} (du/dx)_{i-1/2} - beta_{i+1/2} (du/dx)_{i+1/2}) / h = rhs_i
//
// where h is the cell size.  At `i+1/2` (i.e., the boundary), we have
//
//    a (u_i + u_{i+1})/2 + b (u_{i+1}-u_i)/h = f,
//
// according to the Robin BC.  This gives
//
//    u_{i+1} = A + B u_i,
//
// where `A = f/(b/h + a/2)` and `B = (b/h - a/2) / (b/h + a/2).  We then
// use `u_i` and `u_{i+1}` to compute `(du/dx)_{i+1/2}`.  The discretization
// at cell i then becomes
//
//    \tilde{alpha}_i u_i + (beta_{i-1/2} (du/dx)_{i-1/2} - 0) / h = \tilde{rhs}_i
//
// This is equivalent to having homogeneous Neumann BC with modified alpha and rhs.
//
//    \tilde{alpha}_i = alpha_i + (1-B) beta_{i+1/2} / h^2
//    \tilde{rhs}_i = rhs_i + A beta_{i+1/2} / h^2
//
void
MLABecLaplacian::applyRobinBCTermsCoeffs ()
{
    if (!hasRobinBC()) return;

    const int ncomp = getNComp();
    if (m_a_scalar == Real(0.0)) m_a_scalar = Real(1.0);
    const Real bovera = m_b_scalar/m_a_scalar;

    for (int amrlev = 0; amrlev < m_num_amr_levels; ++amrlev) {
        const int mglev = 0;
        const Box& domain = m_geom[amrlev][mglev].Domain();
        const Real dxi = m_geom[amrlev][mglev].InvCellSize(0);
        const Real dyi = (AMREX_SPACEDIM >= 2) ? m_geom[amrlev][mglev].InvCellSize(1) : Real(1.0);
        const Real dzi = (AMREX_SPACEDIM == 3) ? m_geom[amrlev][mglev].InvCellSize(2) : Real(1.0);

        MFItInfo mfi_info;
        if (Gpu::notInLaunchRegion()) mfi_info.SetDynamic(true);

#ifdef AMREX_USE_OMP
#pragma omp parallel if (Gpu::notInLaunchRegion())
#endif
        for (MFIter mfi(m_a_coeffs[amrlev][mglev], mfi_info); mfi.isValid(); ++mfi)
        {
            const Box& vbx = mfi.validbox();
            Array4<Real> const& afab = m_a_coeffs[amrlev][mglev].array(mfi);
            for (int idim = 0; idim < AMREX_SPACEDIM; ++idim) {
                Array4<Real const> const& bfab = m_b_coeffs[amrlev][mglev][idim].const_array(mfi);
                const Box& blo = amrex::adjCellLo(vbx,idim);
                const Box& bhi = amrex::adjCellHi(vbx,idim);
                bool outside_domain_lo = !(domain.contains(blo));
                bool outside_domain_hi = !(domain.contains(bhi));
                if ((!outside_domain_lo) && (!outside_domain_hi)) continue;
                for (int icomp = 0; icomp < ncomp; ++icomp) {
                    Array4<Real const> const& rbc = (*m_robin_bcval[amrlev])[mfi].const_array(icomp*3);
                    if (m_lobc_orig[icomp][idim] == LinOpBCType::Robin && outside_domain_lo)
                    {
                        if (idim == 0) {
                            Real fac = bovera*dxi*dxi;
                            AMREX_HOST_DEVICE_FOR_3D(blo, i, j, k,
                            {
                                Real B = (rbc(i,j,k,1)*dxi - rbc(i,j,k,0)*Real(0.5))
                                    /    (rbc(i,j,k,1)*dxi + rbc(i,j,k,0)*Real(0.5));
                                afab(i+1,j,k,icomp) += fac*bfab(i+1,j,k,icomp)*(Real(1.0)-B);
                            });
                        } else if (idim == 1) {
                            Real fac = bovera*dyi*dyi;
                            AMREX_HOST_DEVICE_FOR_3D(blo, i, j, k,
                            {
                                Real B = (rbc(i,j,k,1)*dyi - rbc(i,j,k,0)*Real(0.5))
                                    /    (rbc(i,j,k,1)*dyi + rbc(i,j,k,0)*Real(0.5));
                                afab(i,j+1,k,icomp) += fac*bfab(i,j+1,k,icomp)*(Real(1.0)-B);
                            });
                        } else {
                            Real fac = bovera*dzi*dzi;
                            AMREX_HOST_DEVICE_FOR_3D(blo, i, j, k,
                            {
                                Real B = (rbc(i,j,k,1)*dzi - rbc(i,j,k,0)*Real(0.5))
                                    /    (rbc(i,j,k,1)*dzi + rbc(i,j,k,0)*Real(0.5));
                                afab(i,j,k+1,icomp) += fac*bfab(i,j,k+1,icomp)*(Real(1.0)-B);
                            });
                        }
                    }
                    if (m_hibc_orig[icomp][idim] == LinOpBCType::Robin && outside_domain_hi)
                    {
                        if (idim == 0) {
                            Real fac = bovera*dxi*dxi;
                            AMREX_HOST_DEVICE_FOR_3D(bhi, i, j, k,
                            {
                                Real B = (rbc(i,j,k,1)*dxi - rbc(i,j,k,0)*Real(0.5))
                                    /    (rbc(i,j,k,1)*dxi + rbc(i,j,k,0)*Real(0.5));
                                afab(i-1,j,k,icomp) += fac*bfab(i,j,k,icomp)*(Real(1.0)-B);
                            });
                        } else if (idim == 1) {
                            Real fac = bovera*dyi*dyi;
                            AMREX_HOST_DEVICE_FOR_3D(bhi, i, j, k,
                            {
                                Real B = (rbc(i,j,k,1)*dyi - rbc(i,j,k,0)*Real(0.5))
                                    /    (rbc(i,j,k,1)*dyi + rbc(i,j,k,0)*Real(0.5));
                                afab(i,j-1,k,icomp) += fac*bfab(i,j,k,icomp)*(Real(1.0)-B);
                            });
                        } else {
                            Real fac = bovera*dzi*dzi;
                            AMREX_HOST_DEVICE_FOR_3D(bhi, i, j, k,
                            {
                                Real B = (rbc(i,j,k,1)*dzi - rbc(i,j,k,0)*Real(0.5))
                                    /    (rbc(i,j,k,1)*dzi + rbc(i,j,k,0)*Real(0.5));
                                afab(i,j,k-1,icomp) += fac*bfab(i,j,k,icomp)*(Real(1.0)-B);
                            });
                        }
                    }
                }
            }
        }
    }
}

void
MLABecLaplacian::prepareForSolve ()
{
    BL_PROFILE("MLABecLaplacian::prepareForSolve()");

    MLCellABecLap::prepareForSolve();

#if (AMREX_SPACEDIM != 3)
    applyMetricTermsCoeffs();
#endif

    applyRobinBCTermsCoeffs();

    averageDownCoeffs();

    m_is_singular.clear();
    m_is_singular.resize(m_num_amr_levels, false);
    auto itlo = std::find(m_lobc[0].begin(), m_lobc[0].end(), BCType::Dirichlet);
    auto ithi = std::find(m_hibc[0].begin(), m_hibc[0].end(), BCType::Dirichlet);
    if (itlo == m_lobc[0].end() && ithi == m_hibc[0].end())
    {  // No Dirichlet
        for (int alev = 0; alev < m_num_amr_levels; ++alev)
        {
            // For now this assumes that overset regions are treated as Dirichlet bc's
            if (m_domain_covered[alev] && !m_overset_mask[alev][0])
            {
                if (m_a_scalar == 0.0)
                {
                    m_is_singular[alev] = true;
                }
                else
                {
                    Real asum = m_a_coeffs[alev].back().sum();
                    Real amax = m_a_coeffs[alev].back().norm0();
                    m_is_singular[alev] = (asum <= amax * 1.e-12);
                }
            }
        }
    }

    m_needs_update = false;
}

void
MLABecLaplacian::Fapply (int amrlev, int mglev, MultiFab& out, const MultiFab& in) const
{
    BL_PROFILE("MLABecLaplacian::Fapply()");

    const MultiFab& acoef = m_a_coeffs[amrlev][mglev];
    AMREX_D_TERM(const MultiFab& bxcoef = m_b_coeffs[amrlev][mglev][0];,
                 const MultiFab& bycoef = m_b_coeffs[amrlev][mglev][1];,
                 const MultiFab& bzcoef = m_b_coeffs[amrlev][mglev][2];);

    const auto dxinv = m_geom[amrlev][mglev].InvCellSizeArray();

    const Real ascalar = m_a_scalar;
    const Real bscalar = m_b_scalar;

    const int ncomp = getNComp();

#ifdef AMREX_USE_GPU
    if (Gpu::inLaunchRegion() && out.isFusingCandidate()) {
        const auto& xma = in.const_arrays();
        const auto& yma = out.arrays();
        const auto& ama = acoef.arrays();
        AMREX_D_TERM(const auto& bxma = bxcoef.const_arrays();,
                     const auto& byma = bycoef.const_arrays();,
                     const auto& bzma = bzcoef.const_arrays(););
        if (m_overset_mask[amrlev][mglev]) {
            const auto& osmma = m_overset_mask[amrlev][mglev]->const_arrays();
            ParallelFor(out, IntVect(0), ncomp,
            [=] AMREX_GPU_DEVICE (int box_no, int i, int j, int k, int n) noexcept
            {
                mlabeclap_adotx_os(i,j,k,n, yma[box_no], xma[box_no], ama[box_no],
                                   AMREX_D_DECL(bxma[box_no],byma[box_no],bzma[box_no]),
                                   osmma[box_no], dxinv, ascalar, bscalar);
            });
        } else {
            ParallelFor(out, IntVect(0), ncomp,
            [=] AMREX_GPU_DEVICE (int box_no, int i, int j, int k, int n) noexcept
            {
                mlabeclap_adotx(i,j,k,n, yma[box_no], xma[box_no], ama[box_no],
                                AMREX_D_DECL(bxma[box_no],byma[box_no],bzma[box_no]),
                                dxinv, ascalar, bscalar);
            });
        }
        Gpu::streamSynchronize();
    } else
#endif
    {
#ifdef AMREX_USE_OMP
#pragma omp parallel if (Gpu::notInLaunchRegion())
#endif
        for (MFIter mfi(out, TilingIfNotGPU()); mfi.isValid(); ++mfi)
        {
            const Box& bx = mfi.tilebox();
            const auto& xfab = in.array(mfi);
            const auto& yfab = out.array(mfi);
            const auto& afab = acoef.array(mfi);
            AMREX_D_TERM(const auto& bxfab = bxcoef.array(mfi);,
                         const auto& byfab = bycoef.array(mfi);,
                         const auto& bzfab = bzcoef.array(mfi););
            if (m_overset_mask[amrlev][mglev]) {
                const auto& osm = m_overset_mask[amrlev][mglev]->const_array(mfi);
                AMREX_HOST_DEVICE_PARALLEL_FOR_4D(bx, ncomp, i, j, k, n,
                {
                    mlabeclap_adotx_os(i,j,k,n, yfab, xfab, afab, AMREX_D_DECL(bxfab,byfab,bzfab),
                                       osm, dxinv, ascalar, bscalar);
                });
            } else {
                AMREX_HOST_DEVICE_PARALLEL_FOR_4D(bx, ncomp, i, j, k, n,
                {
                    mlabeclap_adotx(i,j,k,n, yfab, xfab, afab, AMREX_D_DECL(bxfab,byfab,bzfab),
                                    dxinv, ascalar, bscalar);
                });
            }
        }
    }
}

void
MLABecLaplacian::normalize (int amrlev, int mglev, MultiFab& mf) const
{
    BL_PROFILE("MLABecLaplacian::normalize()");

    const MultiFab& acoef = m_a_coeffs[amrlev][mglev];
    AMREX_D_TERM(const MultiFab& bxcoef = m_b_coeffs[amrlev][mglev][0];,
                 const MultiFab& bycoef = m_b_coeffs[amrlev][mglev][1];,
                 const MultiFab& bzcoef = m_b_coeffs[amrlev][mglev][2];);

    const auto dxinv = m_geom[amrlev][mglev].InvCellSizeArray();

    const Real ascalar = m_a_scalar;
    const Real bscalar = m_b_scalar;

    const int ncomp = getNComp();

#ifdef AMREX_USE_GPU
    if (Gpu::inLaunchRegion() && mf.isFusingCandidate()) {
        const auto& ma = mf.arrays();
        const auto& ama = acoef.const_arrays();
        AMREX_D_TERM(const auto& bxma = bxcoef.const_arrays();,
                     const auto& byma = bycoef.const_arrays();,
                     const auto& bzma = bzcoef.const_arrays(););
        ParallelFor(mf, IntVect(0), ncomp,
        [=] AMREX_GPU_DEVICE (int box_no, int i, int j, int k, int n) noexcept
        {
            mlabeclap_normalize(i,j,k,n, ma[box_no], ama[box_no],
                                AMREX_D_DECL(bxma[box_no],byma[box_no],bzma[box_no]),
                                dxinv, ascalar, bscalar);
        });
        Gpu::streamSynchronize();
    } else
#endif
    {
#ifdef AMREX_USE_OMP
#pragma omp parallel if (Gpu::notInLaunchRegion())
#endif
        for (MFIter mfi(mf, TilingIfNotGPU()); mfi.isValid(); ++mfi)
        {
            const Box& bx = mfi.tilebox();
            const auto& fab = mf.array(mfi);
            const auto& afab = acoef.array(mfi);
            AMREX_D_TERM(const auto& bxfab = bxcoef.array(mfi);,
                         const auto& byfab = bycoef.array(mfi);,
                         const auto& bzfab = bzcoef.array(mfi););

            AMREX_HOST_DEVICE_PARALLEL_FOR_4D(bx, ncomp, i, j, k, n,
            {
                mlabeclap_normalize(i,j,k,n, fab, afab, AMREX_D_DECL(bxfab,byfab,bzfab),
                                    dxinv, ascalar, bscalar);
            });
        }
    }
}

void
MLABecLaplacian::Fsmooth (int amrlev, int mglev, MultiFab& sol, const MultiFab& rhs, int redblack) const
{
    BL_PROFILE("MLABecLaplacian::Fsmooth()");

    bool regular_coarsening = true;
    if (amrlev == 0 && mglev > 0) {
        regular_coarsening = mg_coarsen_ratio_vec[mglev-1] == mg_coarsen_ratio;
    }

    const MultiFab& acoef = m_a_coeffs[amrlev][mglev];
    AMREX_ALWAYS_ASSERT(acoef.nGrowVect() == 0);
    AMREX_D_TERM(const MultiFab& bxcoef = m_b_coeffs[amrlev][mglev][0];,
                 const MultiFab& bycoef = m_b_coeffs[amrlev][mglev][1];,
                 const MultiFab& bzcoef = m_b_coeffs[amrlev][mglev][2];);
    const auto& undrrelxr = m_undrrelxr[amrlev][mglev];
    const auto& maskvals  = m_maskvals [amrlev][mglev];

    OrientationIter oitr;

    const FabSet& f0 = undrrelxr[oitr()]; ++oitr;
    const FabSet& f1 = undrrelxr[oitr()]; ++oitr;
#if (AMREX_SPACEDIM > 1)
    const FabSet& f2 = undrrelxr[oitr()]; ++oitr;
    const FabSet& f3 = undrrelxr[oitr()]; ++oitr;
#if (AMREX_SPACEDIM > 2)
    const FabSet& f4 = undrrelxr[oitr()]; ++oitr;
    const FabSet& f5 = undrrelxr[oitr()]; ++oitr;
#endif
#endif

    const MultiMask& mm0 = maskvals[0];
    const MultiMask& mm1 = maskvals[1];
#if (AMREX_SPACEDIM > 1)
    const MultiMask& mm2 = maskvals[2];
    const MultiMask& mm3 = maskvals[3];
#if (AMREX_SPACEDIM > 2)
    const MultiMask& mm4 = maskvals[4];
    const MultiMask& mm5 = maskvals[5];
#endif
#endif

    const int nc = getNComp();
    const Real* h = m_geom[amrlev][mglev].CellSize();
    AMREX_D_TERM(const Real dhx = m_b_scalar/(h[0]*h[0]);,
                 const Real dhy = m_b_scalar/(h[1]*h[1]);,
                 const Real dhz = m_b_scalar/(h[2]*h[2]));
    const Real alpha = m_a_scalar;

#ifdef AMREX_USE_GPU
    if (Gpu::inLaunchRegion() && sol.isFusingCandidate()
        && (m_overset_mask[amrlev][mglev] || regular_coarsening))
    {
        const auto& m0ma = mm0.const_arrays();
        const auto& m1ma = mm1.const_arrays();
#if (AMREX_SPACEDIM > 1)
        const auto& m2ma = mm2.const_arrays();
        const auto& m3ma = mm3.const_arrays();
#if (AMREX_SPACEDIM > 2)
        const auto& m4ma = mm4.const_arrays();
        const auto& m5ma = mm5.const_arrays();
#endif
#endif

        const auto& solnma = sol.arrays();
        const auto& rhsma = rhs.const_arrays();
        const auto& ama = acoef.const_arrays();

        AMREX_D_TERM(const auto& bxma = bxcoef.const_arrays();,
                     const auto& byma = bycoef.const_arrays();,
                     const auto& bzma = bzcoef.const_arrays(););

        const auto& f0ma = f0.const_arrays();
        const auto& f1ma = f1.const_arrays();
#if (AMREX_SPACEDIM > 1)
        const auto& f2ma = f2.const_arrays();
        const auto& f3ma = f3.const_arrays();
#if (AMREX_SPACEDIM > 2)
        const auto& f4ma = f4.const_arrays();
        const auto& f5ma = f5.const_arrays();
#endif
#endif

        if (m_overset_mask[amrlev][mglev]) {
            const auto& osmma = m_overset_mask[amrlev][mglev]->const_arrays();
            ParallelFor(sol, IntVect(0), nc,
            [=] AMREX_GPU_DEVICE (int box_no, int i, int j, int k, int n) noexcept
            {
                Box vbx(ama[box_no]);
                abec_gsrb_os(i,j,k,n, solnma[box_no], rhsma[box_no], alpha, ama[box_no],
                             AMREX_D_DECL(dhx, dhy, dhz),
                             AMREX_D_DECL(bxma[box_no],byma[box_no],bzma[box_no]),
                             AMREX_D_DECL(m0ma[box_no],m2ma[box_no],m4ma[box_no]),
                             AMREX_D_DECL(m1ma[box_no],m3ma[box_no],m5ma[box_no]),
                             AMREX_D_DECL(f0ma[box_no],f2ma[box_no],f4ma[box_no]),
                             AMREX_D_DECL(f1ma[box_no],f3ma[box_no],f5ma[box_no]),
                             osmma[box_no], vbx, redblack);
            });
        } else if (regular_coarsening) {
            ParallelFor(sol, IntVect(0), nc,
            [=] AMREX_GPU_DEVICE (int box_no, int i, int j, int k, int n) noexcept
            {
                Box vbx(ama[box_no]);
                abec_gsrb(i,j,k,n, solnma[box_no], rhsma[box_no], alpha, ama[box_no],
                          AMREX_D_DECL(dhx, dhy, dhz),
                          AMREX_D_DECL(bxma[box_no],byma[box_no],bzma[box_no]),
                          AMREX_D_DECL(m0ma[box_no],m2ma[box_no],m4ma[box_no]),
                          AMREX_D_DECL(m1ma[box_no],m3ma[box_no],m5ma[box_no]),
                          AMREX_D_DECL(f0ma[box_no],f2ma[box_no],f4ma[box_no]),
                          AMREX_D_DECL(f1ma[box_no],f3ma[box_no],f5ma[box_no]),
                          vbx, redblack);
            });
        }
        Gpu::streamSynchronize();
    } else
#endif
    {
        MFItInfo mfi_info;
        if (Gpu::notInLaunchRegion()) mfi_info.EnableTiling().SetDynamic(true);

#ifdef AMREX_USE_OMP
#pragma omp parallel if (Gpu::notInLaunchRegion())
#endif
        for (MFIter mfi(sol,mfi_info); mfi.isValid(); ++mfi)
        {
            const auto& m0 = mm0.array(mfi);
            const auto& m1 = mm1.array(mfi);
#if (AMREX_SPACEDIM > 1)
            const auto& m2 = mm2.array(mfi);
            const auto& m3 = mm3.array(mfi);
#if (AMREX_SPACEDIM > 2)
            const auto& m4 = mm4.array(mfi);
            const auto& m5 = mm5.array(mfi);
#endif
#endif

            const Box& tbx = mfi.tilebox();
            const Box& vbx = mfi.validbox();
            const auto& solnfab = sol.array(mfi);
            const auto& rhsfab  = rhs.const_array(mfi);
            const auto& afab    = acoef.const_array(mfi);

            AMREX_D_TERM(const auto& bxfab = bxcoef.const_array(mfi);,
                         const auto& byfab = bycoef.const_array(mfi);,
                         const auto& bzfab = bzcoef.const_array(mfi););

            const auto& f0fab = f0.const_array(mfi);
            const auto& f1fab = f1.const_array(mfi);
#if (AMREX_SPACEDIM > 1)
            const auto& f2fab = f2.const_array(mfi);
            const auto& f3fab = f3.const_array(mfi);
#if (AMREX_SPACEDIM > 2)
            const auto& f4fab = f4.const_array(mfi);
            const auto& f5fab = f5.const_array(mfi);
#endif
#endif

            if (m_overset_mask[amrlev][mglev]) {
                const auto& osm = m_overset_mask[amrlev][mglev]->const_array(mfi);
                AMREX_HOST_DEVICE_PARALLEL_FOR_4D(tbx, nc, i, j, k, n,
                {
                    abec_gsrb_os(i,j,k,n, solnfab, rhsfab, alpha, afab,
                                 AMREX_D_DECL(dhx, dhy, dhz),
                                 AMREX_D_DECL(bxfab, byfab, bzfab),
                                 AMREX_D_DECL(m0,m2,m4),
                                 AMREX_D_DECL(m1,m3,m5),
                                 AMREX_D_DECL(f0fab,f2fab,f4fab),
                                 AMREX_D_DECL(f1fab,f3fab,f5fab),
                                 osm, vbx, redblack);
                });
            } else if (regular_coarsening) {
                AMREX_HOST_DEVICE_PARALLEL_FOR_4D(tbx, nc, i, j, k, n,
                {
                    abec_gsrb(i,j,k,n, solnfab, rhsfab, alpha, afab,
                              AMREX_D_DECL(dhx, dhy, dhz),
                              AMREX_D_DECL(bxfab, byfab, bzfab),
                              AMREX_D_DECL(m0,m2,m4),
                              AMREX_D_DECL(m1,m3,m5),
                              AMREX_D_DECL(f0fab,f2fab,f4fab),
                              AMREX_D_DECL(f1fab,f3fab,f5fab),
                              vbx, redblack);
                });
            } else {
                Gpu::LaunchSafeGuard lsg(false); // xxxxx gpu todo
                // line solve does not with with GPU
                AMREX_LAUNCH_HOST_DEVICE_LAMBDA ( tbx, thread_box,
                {
                    abec_gsrb_with_line_solve(thread_box, solnfab, rhsfab, alpha, afab,
                                              AMREX_D_DECL(dhx, dhy, dhz),
                                              AMREX_D_DECL(bxfab, byfab, bzfab),
                                              AMREX_D_DECL(m0,m2,m4),
                                              AMREX_D_DECL(m1,m3,m5),
                                              AMREX_D_DECL(f0fab,f2fab,f4fab),
                                              AMREX_D_DECL(f1fab,f3fab,f5fab),
                                              vbx, redblack, nc);
                });
            }
        }
    }
}

void
MLABecLaplacian::FFlux (int amrlev, const MFIter& mfi,
                        const Array<FArrayBox*,AMREX_SPACEDIM>& flux,
                        const FArrayBox& sol, Location, const int face_only) const
{
    BL_PROFILE("MLABecLaplacian::FFlux()");

    const int mglev = 0;
    const Box& box = mfi.tilebox();
    const Real* dxinv = m_geom[amrlev][mglev].InvCellSize();
    const int ncomp = getNComp();
    FFlux(box, dxinv, m_b_scalar,
          Array<FArrayBox const*,AMREX_SPACEDIM>{{AMREX_D_DECL(&(m_b_coeffs[amrlev][mglev][0][mfi]),
                                                               &(m_b_coeffs[amrlev][mglev][1][mfi]),
                                                               &(m_b_coeffs[amrlev][mglev][2][mfi]))}},
          flux, sol, face_only, ncomp);
}

void
MLABecLaplacian::FFlux (Box const& box, Real const* dxinv, Real bscalar,
                        Array<FArrayBox const*, AMREX_SPACEDIM> const& bcoef,
                        Array<FArrayBox*,AMREX_SPACEDIM> const& flux,
                        FArrayBox const& sol, int face_only, int ncomp)
{
    AMREX_D_TERM(const auto bx = bcoef[0]->const_array();,
                 const auto by = bcoef[1]->const_array();,
                 const auto bz = bcoef[2]->const_array(););
    AMREX_D_TERM(const auto& fxarr = flux[0]->array();,
                 const auto& fyarr = flux[1]->array();,
                 const auto& fzarr = flux[2]->array(););
    const auto& solarr = sol.array();

    if (face_only)
    {
        Real fac = bscalar*dxinv[0];
        Box blo = amrex::bdryLo(box, 0);
        int blen = box.length(0);
        AMREX_LAUNCH_HOST_DEVICE_LAMBDA ( blo, tbox,
        {
            mlabeclap_flux_xface(tbox, fxarr, solarr, bx, fac, blen, ncomp);
        });
#if (AMREX_SPACEDIM >= 2)
        fac = bscalar*dxinv[1];
        blo = amrex::bdryLo(box, 1);
        blen = box.length(1);
        AMREX_LAUNCH_HOST_DEVICE_LAMBDA ( blo, tbox,
        {
            mlabeclap_flux_yface(tbox, fyarr, solarr, by, fac, blen, ncomp);
        });
#endif
#if (AMREX_SPACEDIM == 3)
        fac = bscalar*dxinv[2];
        blo = amrex::bdryLo(box, 2);
        blen = box.length(2);
        AMREX_LAUNCH_HOST_DEVICE_LAMBDA ( blo, tbox,
        {
            mlabeclap_flux_zface(tbox, fzarr, solarr, bz, fac, blen, ncomp);
        });
#endif
    }
    else
    {
        Real fac = bscalar*dxinv[0];
        Box bflux = amrex::surroundingNodes(box, 0);
        AMREX_LAUNCH_HOST_DEVICE_LAMBDA ( bflux, tbox,
        {
            mlabeclap_flux_x(tbox, fxarr, solarr, bx, fac, ncomp);
        });
#if (AMREX_SPACEDIM >= 2)
        fac = bscalar*dxinv[1];
        bflux = amrex::surroundingNodes(box, 1);
        AMREX_LAUNCH_HOST_DEVICE_LAMBDA ( bflux, tbox,
        {
            mlabeclap_flux_y(tbox, fyarr, solarr, by, fac, ncomp);
        });
#endif
#if (AMREX_SPACEDIM == 3)
        fac = bscalar*dxinv[2];
        bflux = amrex::surroundingNodes(box, 2);
        AMREX_LAUNCH_HOST_DEVICE_LAMBDA ( bflux, tbox,
        {
            mlabeclap_flux_z(tbox, fzarr, solarr, bz, fac, ncomp);
        });
#endif
    }
}

void
MLABecLaplacian::update ()
{
    if (MLCellABecLap::needsUpdate()) MLCellABecLap::update();

#if (AMREX_SPACEDIM != 3)
    applyMetricTermsCoeffs();
#endif

    averageDownCoeffs();

    m_is_singular.clear();
    m_is_singular.resize(m_num_amr_levels, false);
    auto itlo = std::find(m_lobc[0].begin(), m_lobc[0].end(), BCType::Dirichlet);
    auto ithi = std::find(m_hibc[0].begin(), m_hibc[0].end(), BCType::Dirichlet);
    if (itlo == m_lobc[0].end() && ithi == m_hibc[0].end())
    {  // No Dirichlet
        for (int alev = 0; alev < m_num_amr_levels; ++alev)
        {
            // For now this assumes that overset regions are treated as Dirichlet bc's
            if (m_domain_covered[alev] && !m_overset_mask[alev][0])
            {
                if (m_a_scalar == 0.0)
                {
                    m_is_singular[alev] = true;
                }
                else
                {
                    Real asum = m_a_coeffs[alev].back().sum();
                    Real amax = m_a_coeffs[alev].back().norm0();
                    m_is_singular[alev] = (asum <= amax * 1.e-12);
                }
            }
        }
    }

    m_needs_update = false;
}

bool
MLABecLaplacian::supportNSolve () const
{
    bool support = false;
    if (m_overset_mask[0][0]) {
        if (m_geom[0].back().Domain().coarsenable(MLLinOp::mg_coarsen_ratio,
                                                  MLLinOp::mg_domain_min_width)
            && m_grids[0].back().coarsenable(MLLinOp::mg_coarsen_ratio, MLLinOp::mg_box_min_width))
        {
            support = true;
        }
    }
    return support;
}

std::unique_ptr<MLLinOp>
MLABecLaplacian::makeNLinOp (int /*grid_size*/) const
{
    if (m_overset_mask[0][0] == nullptr) return nullptr;

    const Geometry& geom = m_geom[0].back();
    const BoxArray& ba = m_grids[0].back();
    const DistributionMapping& dm = m_dmap[0].back();

    std::unique_ptr<MLLinOp> r{new MLABecLaplacian({geom}, {ba}, {dm}, m_lpinfo_arg)};

    auto nop = dynamic_cast<MLABecLaplacian*>(r.get());
    if (!nop) {
        return nullptr;
    }

    nop->m_parent = this;

    nop->setMaxOrder(maxorder);
    nop->setVerbose(verbose);

    nop->setDomainBC(m_lobc, m_hibc);

    if (needsCoarseDataForBC())
    {
        const Real* dx0 = m_geom[0][0].CellSize();
        const Real fac = Real(0.5)*m_coarse_data_crse_ratio;
        RealVect cbloc {AMREX_D_DECL(dx0[0]*fac, dx0[1]*fac, dx0[2]*fac)};
        nop->setCoarseFineBCLocation(cbloc);
    }

    nop->setScalars(m_a_scalar, m_b_scalar);

    MultiFab const& alpha_bottom = m_a_coeffs[0].back();
    iMultiFab const& osm_bottom = *m_overset_mask[0].back();
    const int ncomp = alpha_bottom.nComp();
    MultiFab alpha(ba, dm, ncomp, 0);

    Real a_max = alpha_bottom.norminf(0, 0, true, true);
    AMREX_D_TERM(Real bx_max = m_b_coeffs[0].back()[0].norminf(0,0,true,true);,
                 Real by_max = m_b_coeffs[0].back()[1].norminf(0,0,true,true);,
                 Real bz_max = m_b_coeffs[0].back()[1].norminf(0,0,true,true));
    const Real* dxinv = geom.InvCellSize();
    Real huge_alpha = Real(1.e30) *
        amrex::max(a_max*std::abs(m_a_scalar),
                   AMREX_D_DECL(std::abs(m_b_scalar)*bx_max*dxinv[0]*dxinv[0],
                                std::abs(m_b_scalar)*by_max*dxinv[1]*dxinv[1],
                                std::abs(m_b_scalar)*bz_max*dxinv[2]*dxinv[2]));
    ParallelAllReduce::Max(huge_alpha, ParallelContext::CommunicatorSub());

#ifdef AMREX_USE_GPU
    if (Gpu::inLaunchRegion() && alpha.isFusingCandidate()) {
        auto const& ama = alpha.arrays();
        auto const& abotma = alpha_bottom.const_arrays();
        auto const& mma = osm_bottom.const_arrays();
        ParallelFor(alpha, IntVect(0), ncomp,
        [=] AMREX_GPU_DEVICE (int box_no, int i, int j, int k, int n) noexcept
        {
            if (mma[box_no](i,j,k)) {
                ama[box_no](i,j,k,n) = abotma[box_no](i,j,k,n);
            } else {
                ama[box_no](i,j,k,n) = huge_alpha;
            }
        });
        Gpu::streamSynchronize();
    } else
#endif
    {
#ifdef AMREX_USE_OMP
#pragma omp parallel if (Gpu::notInLaunchRegion())
#endif
        for (MFIter mfi(alpha,TilingIfNotGPU()); mfi.isValid(); ++mfi) {
            Box const& bx = mfi.tilebox();
            Array4<Real> const& a = alpha.array(mfi);
            Array4<Real const> const& abot = alpha_bottom.const_array(mfi);
            Array4<int const> const& m = osm_bottom.const_array(mfi);
            AMREX_HOST_DEVICE_PARALLEL_FOR_4D(bx, ncomp, i, j, k, n,
            {
                if (m(i,j,k)) {
                    a(i,j,k,n) = abot(i,j,k,n);
                } else {
                    a(i,j,k,n) = huge_alpha;
                }
            });
        }
    }

    nop->setACoeffs(0, alpha);
    nop->setBCoeffs(0, GetArrOfConstPtrs(m_b_coeffs[0].back()));

    return r;
}

void
MLABecLaplacian::copyNSolveSolution (MultiFab& dst, MultiFab const& src) const
{
    if (m_overset_mask[0].back() == nullptr) return;

    const int ncomp = dst.nComp();

#ifdef AMREX_USE_GPU
    if (Gpu::inLaunchRegion() && dst.isFusingCandidate()) {
        auto const& dstma = dst.arrays();
        auto const& srcma = src.const_arrays();
        auto const& mma = m_overset_mask[0].back()->const_arrays();
        ParallelFor(dst, IntVect(0), ncomp,
        [=] AMREX_GPU_DEVICE (int box_no, int i, int j, int k, int n) noexcept
        {
            if (mma[box_no](i,j,k)) {
                dstma[box_no](i,j,k,n) = srcma[box_no](i,j,k,n);
            } else {
                dstma[box_no](i,j,k,n) = Real(0.0);
            }
        });
        Gpu::streamSynchronize();
    } else
#endif
    {
#ifdef AMREX_USE_OMP
#pragma omp parallel if (Gpu::notInLaunchRegion())
#endif
        for (MFIter mfi(dst,TilingIfNotGPU()); mfi.isValid(); ++mfi) {
            Box const& bx = mfi.tilebox();
            Array4<Real> const& dfab = dst.array(mfi);
            Array4<Real const> const& sfab = src.const_array(mfi);
            Array4<int const> const& m = m_overset_mask[0].back()->const_array(mfi);
            AMREX_HOST_DEVICE_PARALLEL_FOR_4D(bx, ncomp, i, j, k, n,
            {
                if (m(i,j,k)) {
                    dfab(i,j,k,n) = sfab(i,j,k,n);
                } else {
                    dfab(i,j,k,n) = Real(0.0);
                }
            });
        }
    }
}

}
