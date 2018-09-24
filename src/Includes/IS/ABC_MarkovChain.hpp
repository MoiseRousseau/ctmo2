#pragma once
#include <valarray>

#include "../Utilities/Utilities.hpp"
#include "../Utilities/LinAlg.hpp"
#include "../Utilities/Matrix.hpp"
#include "../Utilities/MPIUtilities.hpp"
#include "../Utilities/Fourier.hpp"
#include "../Utilities/GreenTau.hpp"
#include "Obs/Observables.hpp"
#include "ISData.hpp"

//#define DEBUG_TEST

namespace Markov
{

using Fourier::MatToTau;
using Fourier::MatToTauCluster;
using Vertex = Diagrammatic::Vertex;
using VertexPart = Diagrammatic::VertexPart;
typedef LinAlg::Matrix_t Matrix_t;

struct NFData
{

    NFData() : FVup_(), FVdown_(), Nup_(), Ndown_(), dummy_(){};
    SiteVector_t FVup_;
    SiteVector_t FVdown_;
    Matrix_t Nup_;
    Matrix_t Ndown_;
    Matrix_t dummy_;
};

template <typename TIOModel, typename TModel>
class ABC_MarkovChain
{

    using GreenTau_t = GreenTau::GreenCluster0Tau<TIOModel>;

  public:
    const size_t Nc = TModel::Nc;
    const double PROBFLIP = 0.25;
    const double PROBINSERT = 0.25;
    const double PROBREMOVE = 1.0 - PROBINSERT;

    ABC_MarkovChain(const Json &jj, const size_t &seed) : modelPtr_(new TModel(jj)),
                                                          rng_(seed),
                                                          urng_(rng_, Utilities::UniformDistribution_t(0.0, 1.0)),
                                                          nfdata_(),
                                                          dataCT_(
                                                              new Obs::ISDataCT<TIOModel, TModel>(
                                                                  jj,
                                                                  *modelPtr_)),
                                                          obs_(dataCT_, jj),
                                                          vertexBuilder_(jj, TModel::Nc)
    {
        const std::valarray<size_t> zeroPair = {0, 0};
        updStats_["Inserts"] = zeroPair;
        updStats_["Removes"] = zeroPair;
        updStats_["Flips"] = zeroPair;
        updatesProposed_ = 0;

        mpiUt::Print("MarkovChain Created \n");
    }

    virtual ~ABC_MarkovChain() = 0;

    //Getters
    TModel model() const
    {
        return (*modelPtr_);
    };

    Matrix_t Nup() const
    {
        return nfdata_.Nup_;
    };

    Matrix_t Ndown() const
    {
        return nfdata_.Ndown_;
    };

    size_t updatesProposed() const { return updatesProposed_; }

    double beta() const
    {
        return dataCT_->beta_;
    };

    virtual double gammaTrad(const FermionSpin_t &spin, const AuxSpin_t &auxTo, const AuxSpin_t &vauxFrom) = 0;
    virtual double FAux(const FermionSpin_t &spin, const AuxSpin_t &aux) = 0;

    // void ThermalizeFromConfig()
    // {
    //     if (mpiUt::LoadConfig(dataCT_->vertices_))
    //     {
    //         const size_t kk = dataCT_->vertices_.size();
    //         nfdata_.FVup_ = SiteVector_t(kk);
    //         nfdata_.FVdown_ = SiteVector_t(kk);
    //         for (size_t i = 0; i < kk; i++)
    //         {
    //             AuxSpin_t aux = dataCT_->vertices_.at(i).aux();
    //             nfdata_.FVup_(i) = FAux(FermionSpin_t::Up, aux);
    //             nfdata_.FVdown_(i) = FAux(FermionSpin_t::Down, aux);
    //         }

    //         nfdata_.Nup_.Resize(kk, kk);
    //         nfdata_.Ndown_.Resize(kk, kk);
    //         CleanUpdate();
    //     }
    // }

    void DoStep()
    {

        // if (urng_() < PROBFLIP)
        // {
        //     FlipAux();
        // }
        // else
        // {
        urng_() < PROBINSERT ? InsertVertex() : RemoveVertex();
        // }

        updatesProposed_++;
    }

    void FlipAux()
    {

        // if (dataCT_->vertices_.size())
        // {
        //     updStats_["Flips"][0]++;
        //     const size_t p = static_cast<size_t>(dataCT_->vertices_.size() * urng_());
        //     Vertex vertex = dataCT_->vertices_.at(p);
        //     vertex.FlipAux();
        //     const AuxSpin_t auxTo = vertex.aux();
        //     const AuxSpin_t auxFrom = dataCT_->vertices_.at(p).aux();

        //     const double fauxup = nfdata_.FVup_(p);
        //     const double fauxdown = nfdata_.FVdown_(p);
        //     const double fauxupM1 = fauxup - 1.0;
        //     const double fauxdownM1 = fauxdown - 1.0;
        //     const double gammakup = gammaTrad(FermionSpin_t::Up, auxTo, auxFrom);
        //     double gammakdown = gammaTrad(FermionSpin_t::Down, auxTo, auxFrom);

        //     const double ratioUp = 1.0 + (1.0 - (nfdata_.Nup_(p, p) * fauxup - 1.0) / (fauxupM1)) * gammakup;
        //     const double ratioDown = 1.0 + (1.0 - (nfdata_.Ndown_(p, p) * fauxdown - 1.0) / (fauxdownM1)) * gammakdown;

        //     const double ratioAcc = ratioUp * ratioDown;

        //     if (urng_() < std::abs(ratioAcc))
        //     {
        //         updStats_["Flips"][1]++;
        //         if (ratioAcc < 0.0)
        //         {
        //             dataCT_->sign_ *= -1;
        //         }

        //         //AssertSizes();
        //         const size_t kk = dataCT_->vertices_.size();
        //         double lambdaUp = gammakup / ratioUp;
        //         double lambdaDown = gammakdown / ratioDown;

        //         SiteVector_t rowpUp;
        //         SiteVector_t colpUp;
        //         LinAlg::ExtractRow(p, rowpUp, nfdata_.Nup_);
        //         LinAlg::ExtractCol(p, colpUp, nfdata_.Nup_);

        //         SiteVector_t rowpDown;
        //         SiteVector_t colpDown;
        //         LinAlg::ExtractRow(p, rowpDown, nfdata_.Ndown_);
        //         LinAlg::ExtractCol(p, colpDown, nfdata_.Ndown_);

        //         for (size_t j = 0; j < kk; j++)
        //         {
        //             for (size_t i = 0; i < kk; i++)
        //             {
        //                 if (i != p)
        //                 {
        //                     nfdata_.Nup_(i, j) += (colpUp(i) * fauxup / fauxupM1) * lambdaUp * rowpUp(j);
        //                     nfdata_.Ndown_(i, j) += (colpDown(i) * fauxdown / fauxdownM1) * lambdaDown * rowpDown(j);
        //                 }
        //                 else
        //                 {
        //                     nfdata_.Nup_(i, j) += (((colpUp(i) * fauxup - 1.0) / fauxupM1) - 1.0) * lambdaUp * rowpUp(j);
        //                     nfdata_.Ndown_(i, j) += (((colpDown(i) * fauxdown - 1.0) / fauxdownM1) - 1.0) * lambdaDown * rowpDown(j);
        //                 }
        //             }
        //         }

        //         dataCT_->vertices_.FlipAux(p);
        //         nfdata_.FVup_(p) = fauxdown;
        //         nfdata_.FVdown_(p) = fauxup;

        //         AssertSizes();
        //     }
        // }
    }

    void AssertSizes()
    {
        const size_t kk = dataCT_->vertices_.size();
        assert(nfdata_.Nup_.n_rows() + nfdata_.Ndown_.n_rows() == 2 * kk);
        assert(2 * dataCT_->vertices_.size() == dataCT_->vertices_.NUp() + dataCT_->vertices_.NDown());
        assert(dataCT_->vertices_.NUp() == dataCT_->vertices_.NDown());
    }

    void InsertVertex()
    {
        AssertSizes();
        updStats_["Inserts"][0]++;
        Vertex vertex = vertexBuilder_.BuildVertex(urng_);
        VertexPart x = vertex.vStart();
        VertexPart y = vertex.vEnd();

        if (x.spin() == y.spin())
        {
            InsertVertexSameSpin(vertex);
        }
        else
        {
            InsertVertexDiffSpin(vertex);
        }
    }

    void InsertVertexDiffSpin(const Vertex &vertex)
    {

        const double fauxup = FAux(FermionSpin_t::Up, vertex.aux());
        const double fauxdown = FAux(FermionSpin_t::Down, vertex.aux());
        const double fauxupM1 = fauxup - 1.0;
        const double fauxdownM1 = fauxdown - 1.0;

        const VertexPart vertexPartUp = vertex.vStart();
        const VertexPart vertexPartDown = vertex.vEnd();
        assert(vertexPartUp.spin() == FermionSpin_t::Up);
        assert(vertexPartDown.spin() == FermionSpin_t::Down);

        const double sUp = fauxup - GetGreenTau0Up(vertexPartUp, vertexPartUp) * fauxupM1;
        const double sDown = fauxdown - GetGreenTau0Down(vertexPartDown, vertexPartDown) * fauxdownM1;

        if (dataCT_->vertices_.size())
        {
            AssertSizes();
            const size_t kkoldUp = dataCT_->vertices_.NUp();
            const size_t kknewUp = kkoldUp + 1;
            const size_t kkoldDown = dataCT_->vertices_.NDown();
            const size_t kknewDown = kkoldDown + 1;

            SiteVector_t newLastColUp_(kkoldUp);
            SiteVector_t newLastRowUp_(kkoldUp);
            SiteVector_t newLastColDown_(kkoldDown);
            SiteVector_t newLastRowDown_(kkoldDown);

            //Probably put this in a method
            for (size_t iUp = 0; iUp < dataCT_->vertices_.NUp(); iUp++)
            {
                newLastRowUp_(iUp) = -GetGreenTau0Up(vertexPartUp, dataCT_->vertices_.atUp(iUp)) * (nfdata_.FVup_(iUp) - 1.0);
                newLastColUp_(iUp) = -GetGreenTau0Up(dataCT_->vertices_.atUp(iUp), vertexPartUp) * fauxupM1;
            }

            for (size_t iDown = 0; iDown < dataCT_->vertices_.NDown(); iDown++)
            {
                newLastRowDown_(iDown) = -GetGreenTau0Down(vertexPartDown, dataCT_->vertices_.atDown(iDown)) * (nfdata_.FVdown_(iDown) - 1.0);
                newLastColDown_(iDown) = -GetGreenTau0Down(dataCT_->vertices_.atDown(iDown), vertexPartDown) * fauxdownM1;
            }

            SiteVector_t NQUp(kkoldUp); //NQ = N*Q
            SiteVector_t NQDown(kkoldDown);
            MatrixVectorMult(nfdata_.Nup_, newLastColUp_, 1.0, NQUp);
            MatrixVectorMult(nfdata_.Ndown_, newLastColDown_, 1.0, NQDown);
            const double sTildeUpI = sUp - LinAlg::DotVectors(newLastRowUp_, NQUp);
            const double sTildeDownI = sDown - LinAlg::DotVectors(newLastRowDown_, NQDown);

            const double ratio = sTildeUpI * sTildeDownI;
            const double ratioAcc = PROBREMOVE / PROBINSERT * vertex.probProb() / static_cast<size_t>(dataCT_->vertices_.size() + 1) * ratio;
            if (urng_() < std::abs(ratioAcc))
            {
                updStats_["Inserts"][1]++;
                if (ratioAcc < .0)
                {
                    dataCT_->sign_ *= -1;
                }

                LinAlg::BlockRankOneUpgrade(nfdata_.Nup_, NQUp, newLastRowUp_, 1.0 / sTildeUpI);
                LinAlg::BlockRankOneUpgrade(nfdata_.Ndown_, NQDown, newLastRowDown_, 1.0 / sTildeDownI);
                nfdata_.FVup_.resize(kknewUp);
                nfdata_.FVdown_.resize(kknewDown);
                nfdata_.FVup_(kkoldUp) = fauxup;
                nfdata_.FVdown_(kkoldDown) = fauxdown;
                dataCT_->vertices_.AppendVertex(vertex);
                AssertSizes();
            }
        }
        else
        {
            AssertSizes();
            const double ratioAcc = PROBREMOVE / PROBINSERT * vertex.probProb() * sUp * sDown;

            if (urng_() < std::abs(ratioAcc))
            {
                if (ratioAcc < 0.0)
                {
                    dataCT_->sign_ *= -1;
                }

                nfdata_.Nup_ = Matrix_t(1, 1);
                nfdata_.Ndown_ = Matrix_t(1, 1);
                nfdata_.Nup_(0, 0) = 1.0 / sUp;
                nfdata_.Ndown_(0, 0) = 1.0 / sDown;

                nfdata_.FVup_ = SiteVector_t(1);
                nfdata_.FVdown_ = SiteVector_t(1);
                nfdata_.FVup_(0) = fauxup;
                nfdata_.FVdown_(0) = fauxdown;

                dataCT_->vertices_.AppendVertex(vertex);
            }
        }
    }

    void InsertVertexSameSpin(const Vertex &vertex)
    {
        assert(false);
    }

    void RemoveVertex()
    {
        AssertSizes();
        updStats_["Removes"][0]++;
        if (dataCT_->vertices_.size())
        {
            const size_t pp = static_cast<int>(urng_() * dataCT_->vertices_.size());
            const Vertex vertex = dataCT_->vertices_.at(pp);
            const VertexPart x = vertex.vStart();
            const VertexPart y = vertex.vEnd();

            if (x.spin() == y.spin())
            {
                RemoveVertexSameSpin(pp);
            }
            else
            {
                RemoveVertexDiffSpin(pp);
            }
        }
    }

    void RemoveVertexDiffSpin(const size_t &pp)
    {
        const Vertex vertex = dataCT_->vertices_.at(pp);
        // const VertexPart x = vertex.vStart();
        // const VertexPart y = vertex.vEnd();

        //In theory we should find the proper index for each spin
        const double ratioAcc = PROBINSERT / PROBREMOVE * static_cast<double>(dataCT_->vertices_.size()) / vertex.probProb() * nfdata_.Nup_(pp, pp) * nfdata_.Ndown_(pp, pp);

        if (urng_() < std::abs(ratioAcc))
        {
            //AssertSizes();
            updStats_["Removes"][1]++;
            if (ratioAcc < .0)
            {
                dataCT_->sign_ *= -1;
            }

            //The update matrices of size k-1 x k-1 with the pp row and col deleted and the last row and col now at index pp

            const size_t kkUp = dataCT_->vertices_.NUp();
            const size_t kkUpm1 = kkUp - 1;
            const size_t kkDown = dataCT_->vertices_.NDown();
            const size_t kkDownm1 = kkDown - 1;
            const auto ppSpins = dataCT_->vertices_.GetIndicesSpins(pp);
            assert(pp == ppSpins.first);
            assert(pp == ppSpins.second);

            LinAlg::BlockRankOneDowngrade(nfdata_.Nup_, ppSpins.first);
            LinAlg::BlockRankOneDowngrade(nfdata_.Ndown_, ppSpins.second);

            nfdata_.FVup_.swap_rows(ppSpins.first, kkUpm1);
            nfdata_.FVdown_.swap_rows(ppSpins.second, kkDownm1);
            nfdata_.FVup_.resize(kkUpm1);
            nfdata_.FVdown_.resize(kkDownm1);

            dataCT_->vertices_.RemoveVertex(pp);
            //AssertSizes();
        }
    }

    void RemoveVertexSameSpin(const size_t &pp)
    {
        assert(false);
    }

    void CleanUpdate()
    {
        //mpiUt::Print("Cleaning, sign, k =  " + std::to_string(dataCT_->sign_) + ",  " + std::to_string(dataCT_->vertices_.size()));
        const size_t kk = dataCT_->vertices_.size();
        const size_t kkup = dataCT_->vertices_.NUp();
        const size_t kkdown = dataCT_->vertices_.NDown();

        if (kk == 0)
        {
            return;
        }

        AssertSizes();
        for (size_t iUp = 0; iUp < kkup; iUp++)
        {
            for (size_t jUp = 0; jUp < kkdown; jUp++)
            {

                nfdata_.Nup_(iUp, jUp) = -GetGreenTau0Up(dataCT_->vertices_.atUp(iUp), dataCT_->vertices_.atUp(jUp)) * (nfdata_.FVup_(jUp) - 1.0);

                if (iUp == jUp)
                {
                    nfdata_.Nup_(iUp, iUp) += nfdata_.FVup_(iUp);
                }
            }
        }

        for (size_t iDown = 0; iDown < kkdown; iDown++)
        {
            for (size_t jDown = 0; jDown < kkdown; jDown++)
            {

                nfdata_.Ndown_(iDown, jDown) = -GetGreenTau0Down(dataCT_->vertices_.atDown(iDown), dataCT_->vertices_.atDown(jDown)) * (nfdata_.FVdown_(jDown) - 1.0);

                if (iDown == jDown)
                {
                    nfdata_.Ndown_(iDown, iDown) += nfdata_.FVdown_(iDown);
                }
            }
        }

        nfdata_.Nup_.Inverse();
        nfdata_.Ndown_.Inverse();
    }

    double GetGreenTau0Up(const VertexPart &x, const VertexPart &y) const
    {
        assert(x.spin() == y.spin());
        return (dataCT_->green0CachedUp_(x.superSite(), y.superSite(), x.tau() - y.tau()));
    }

    double GetGreenTau0Down(const VertexPart &x, const VertexPart &y) const
    {

#ifdef AFM
        assert(x.spin() == y.spin());
        return (dataCT_->green0CachedDown_(x.superSite(), y.superSite(), x.tau() - y.tau()));
#else
        return GetGreenTau0Up(x, y);

#endif
    }

    void Measure()
    {
        AssertSizes();
        const SiteVector_t FVupM1 = -(nfdata_.FVup_ - 1.0);
        const SiteVector_t FVdownM1 = -(nfdata_.FVdown_ - 1.0);
        DDMGMM(FVupM1, nfdata_.Nup_, *(dataCT_->MupPtr_));
        DDMGMM(FVdownM1, nfdata_.Ndown_, *(dataCT_->MdownPtr_));
        obs_.Measure();
    }

    void SaveMeas()
    {

        obs_.Save();
        // mpiUt::SaveConfig(dataCT_->vertices_);
        SaveUpd("upd.meas");
    }

    void SaveTherm()
    {

        SaveUpd("upd.therm");
        for (UpdStats_t::iterator it = updStats_.begin(); it != updStats_.end(); ++it)
        {
            std::string key = it->first;
            updStats_[key] = 0.0;
        }
    }

    void SaveUpd(const std::string fname)
    {
        std::vector<UpdStats_t> updStatsVec;
#ifdef HAVEMPI

        mpi::communicator world;
        if (mpiUt::Rank() == mpiUt::master)
        {
            mpi::gather(world, updStats_, updStatsVec, mpiUt::master);
        }
        else
        {
            mpi::gather(world, updStats_, mpiUt::master);
        }
        if (mpiUt::Rank() == mpiUt::master)
        {
            mpiUt::SaveUpdStats(fname, updStatsVec);
        }

#else
        updStatsVec.push_back(updStats_);
        mpiUt::SaveUpdStats(fname, updStatsVec);
#endif

        mpiUt::Print("Finished Saving MarkovChain.");
    }

  protected:
    //attributes
    std::shared_ptr<TModel> modelPtr_;
    Utilities::EngineTypeMt19937_t rng_;
    Utilities::UniformRngMt19937_t urng_;
    NFData nfdata_;
    std::shared_ptr<Obs::ISDataCT<TIOModel, TModel>> dataCT_;
    Obs::Observables<TIOModel, TModel> obs_;
    Diagrammatic::VertexBuilder vertexBuilder_;

    UpdStats_t updStats_; //[0] = number of propsed, [1]=number of accepted

    size_t updatesProposed_;
}; // namespace Markov

template <typename TIOModel, typename TModel>
ABC_MarkovChain<TIOModel, TModel>::~ABC_MarkovChain() {} //destructors must exist

} // namespace Markov
