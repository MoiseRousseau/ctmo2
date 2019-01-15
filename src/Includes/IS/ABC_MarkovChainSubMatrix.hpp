#pragma once

#include "../Utilities/Matrix.hpp"
#include "../Utilities/MPITools.hpp"
#include "../Utilities/GreenTau.hpp"
#include "Obs/Observables.hpp"
#include "ISData.hpp"

//We have the conig C0TIlde = C0 U CTilde
//C0 is only the interacting vertices.
//ctilde is only the non-interacting vertices (before insertions are proposed)

namespace MarkovSub
{

using Fourier::MatToTau;
using Fourier::MatToTauCluster;
using Vertex = Diagrammatic::Vertex;
using VertexPart = Diagrammatic::VertexPart;

typedef LinAlg::Matrix_t Matrix_t;

extern "C"
{
    unsigned int dcopy_(unsigned int const *, double const *, unsigned int const *, double *, unsigned int const *);
}

struct GammaData
{
    GammaData() : gammaUpI_(), gammaDownI_(){};
    Matrix_t gammaUpI_;
    Matrix_t gammaDownI_;
};

struct UpdData
{
    UpdData() : xup_(), yup_(), xdown_(), ydown_(){};

    SiteVector_t xup_; //New row to insert
    SiteVector_t yup_; //New column to insert
    SiteVector_t xdown_;
    SiteVector_t ydown_;

    SiteVector_t gammaUpIYup_; // gammaUpI_*yup;
    SiteVector_t gammaDownIYdown_;
    double dup_;
    double ddown_;
    double dTildeUpI_;
    double dTildeDownI_;

    void SetSize(const size_t &kk)
    {
        xup_.set_size(kk);
        yup_.set_size(kk);
        xdown_.set_size(kk);
        ydown_.set_size(kk);
        gammaUpIYup_.set_size(kk);
        gammaDownIYdown_.set_size(kk);
    }
};

struct NFData
{

    NFData() : FVup_(), FVdown_(), Nup_(), Ndown_(){};
    SiteVector_t FVup_;
    SiteVector_t FVdown_;
    Matrix_t Nup_;
    Matrix_t Ndown_;
};

struct GreenData
{

    GreenData() : greenInteractUp_(), greenInteractDown_(){};
    Matrix_t greenInteractUp_;
    Matrix_t greenInteractDown_;
};

class ABC_MarkovChainSubMatrix
{

    using GreenTau_t = GreenTau::GreenCluster0Tau;
    using Model_t = Models::ABC_Model_2D;

  public:
    const double PROBINSERT = 0.5;
    const double PROBREMOVE = 1.0 - PROBINSERT;

    ABC_MarkovChainSubMatrix(const Json &jjSim, const size_t &seed) : modelPtr_(new Model_t(jjSim)),
                                                                      rng_(seed),
                                                                      urng_(rng_, Utilities::UniformDistribution_t(0.0, 1.0)),
                                                                      gammadata_(),
                                                                      upddata_(),
                                                                      nfdata_(),
                                                                      greendata_(),
                                                                      dataCT_(
                                                                          new Markov::Obs::ISDataCT(
                                                                              jjSim,
                                                                              modelPtr_)),
                                                                      obs_(dataCT_, jjSim),
                                                                      vertexBuilder_(jjSim, modelPtr_->Nc()),
                                                                      KMAX_UPD_(jjSim["solver"]["kmax_upd"].get<double>()),
                                                                      KAux_(
                                                                          -jjSim["model"]["U"].get<double>() * dataCT_->beta_ * modelPtr_->Nc() /
                                                                          (((1.0 + jjSim["model"]["delta"].get<double>()) / jjSim["model"]["delta"].get<double>() - 1.0) *
                                                                           (jjSim["model"]["delta"].get<double>() / (1.0 + jjSim["model"]["delta"].get<double>()) - 1.0))

                                                                      )
    {
        const std::valarray<size_t> zeroPair = {0, 0};
        updStats_["Inserts"] = zeroPair;
        updStats_["Removes"] = zeroPair;
        updStats_["Flips"] = zeroPair;

        updatesProposed_ = 0;

        assert(jjSim["model"]["nOrb"].get<size_t>() == 1);
        Logging::Debug("MarkovChain Created.");
    }

    virtual ~ABC_MarkovChainSubMatrix(){};

    //Getters

    Matrix_t Nup() const
    {
        return nfdata_.Nup_;
    };
    Matrix_t Ndown() const
    {
        return nfdata_.Ndown_;
    };

    double beta() const
    {
        return dataCT_->beta_;
    };

    size_t updatesProposed() const
    {
        return updatesProposed_;
    }

    void AssertSizes()
    {
        const size_t kk = dataCT_->vertices_.size();
        assert(kk == nfdata_.Nup_.n_rows());
        assert(dataCT_->vertices_.NUp() == kk);
        assert(dataCT_->vertices_.NDown() == kk);
        assert(kk == nfdata_.Nup_.n_cols());
        assert(kk == nfdata_.Ndown_.n_rows());
        assert(kk == nfdata_.Ndown_.n_cols());
    }

    virtual double gammaSubMatrix(const VertexPart &vpTo, const VertexPart &vpFrom) const = 0;
    // virtual double KAux_ = 0;
    virtual double FAux(const VertexPart &vPart) const = 0;

    void DoStep()
    {
        Logging::Trace("Start of DoStep.");
        PreparationSteps();

        for (size_t kk = 0; kk < KMAX_UPD_; kk++)
        {
            DoInnerStep();
            updatesProposed_++;
        }

        UpdateSteps();

        Logging::Trace("End of DoStep.");
    }

    void DoInnerStep()
    {
        Logging::Trace("Start of DoInnerStep.");
        urng_() < PROBINSERT ? InsertVertexSubMatrix() : RemoveVertexSubMatrix();
        Logging::Trace("Start of DoInnerStep.");
    }

    double CalculateDeterminantRatio(const Vertex &vertexTo, const Vertex &vertexFrom, const size_t &vertexIndex)
    {
        Logging::Trace("Start of CalculateDeterminantRatio.");
        assert(vertexTo.vStart().aux() == vertexTo.vEnd().aux());
        assert(vertexTo.vStart().aux() != vertexFrom.vEnd().aux());

        const double gammakup = gammaSubMatrix(vertexTo.vStart(), vertexFrom.vStart()); //Remeber that for spin diagonal interactions start==up and end==down
        const double gammakdown = gammaSubMatrix(vertexTo.vEnd(), vertexFrom.vEnd());
        upddata_.dup_ = (greendata_.greenInteractUp_(vertexIndex, vertexIndex) - (1.0 + gammakup) / gammakup);
        upddata_.ddown_ = (greendata_.greenInteractDown_(vertexIndex, vertexIndex) - (1.0 + gammakdown) / gammakdown);

        double ratio = 0.0;

        if (gammadata_.gammaUpI_.n_rows())
        {
            const size_t kkold = gammadata_.gammaUpI_.n_rows();
            upddata_.SetSize(kkold);

            for (size_t i = 0; i < kkold; i++)
            {
                upddata_.xup_(i) = greendata_.greenInteractUp_(vertexIndex, verticesUpdated_.at(i));
                upddata_.yup_(i) = greendata_.greenInteractUp_(verticesUpdated_[i], vertexIndex);

                upddata_.xdown_(i) = greendata_.greenInteractDown_(vertexIndex, verticesUpdated_[i]);
                upddata_.ydown_(i) = greendata_.greenInteractDown_(verticesUpdated_[i], vertexIndex);
            }

            MatrixVectorMult(gammadata_.gammaUpI_, upddata_.yup_, 1.0, upddata_.gammaUpIYup_);
            MatrixVectorMult(gammadata_.gammaDownI_, upddata_.ydown_, 1.0, upddata_.gammaDownIYdown_);
            upddata_.dTildeUpI_ = upddata_.dup_ - LinAlg::DotVectors(upddata_.xup_, upddata_.gammaUpIYup_); //this is in fact beta of submatrix gull article (last element of new matrix GammaI)
            upddata_.dTildeDownI_ = upddata_.ddown_ - LinAlg::DotVectors(upddata_.xdown_, upddata_.gammaDownIYdown_);

            ratio = gammakup * gammakdown * upddata_.dTildeUpI_ * upddata_.dTildeDownI_;
        }
        else
        {
            ratio = gammakup * gammakdown * upddata_.dup_ * upddata_.ddown_;
        }

        Logging::Trace("End of CalculateDeterminantRatio.");
        return ratio;
    }

    void AcceptMove(const double &probAcc)
    {
        Logging::Trace("Start of AcceptMove.");
        if (probAcc < 0.0)
        {
            dataCT_->sign_ *= -1;
        }

        if (gammadata_.gammaUpI_.n_rows())
        {
            LinAlg::BlockRankOneUpgrade(gammadata_.gammaUpI_, upddata_.gammaUpIYup_, upddata_.xup_, 1.0 / upddata_.dTildeUpI_);
            LinAlg::BlockRankOneUpgrade(gammadata_.gammaDownI_, upddata_.gammaDownIYdown_, upddata_.xdown_, 1.0 / upddata_.dTildeDownI_);
        }
        else
        {
            gammadata_.gammaUpI_ = Matrix_t(1, 1);
            gammadata_.gammaUpI_(0, 0) = 1.0 / upddata_.dup_;
            gammadata_.gammaDownI_ = Matrix_t(1, 1);
            gammadata_.gammaDownI_(0, 0) = 1.0 / upddata_.ddown_;
        }

        Logging::Trace("Start of AcceptMove.");
    }

    void RemoveVertexSubMatrix()
    {
        Logging::Trace("Start of RemoveVertexSubMatrix.");

        if (vertices0_.size() && verticesRemovable_.size())
        {
            updStats_["Removes"][0]++;
            const size_t ii = static_cast<int>(urng_() * verticesRemovable_.size());
            const size_t vertexIndex = verticesRemovable_.at(ii);

            Vertex vertex = vertices0Tilde_.at(vertexIndex);
            std::cout << "vertexIndex in remove = " << vertexIndex << std::endl;
            assert(vertex.vStart().aux() != AuxSpin_t::Zero);

            if (vertexIndex >= vertices0_.size())
            {
                // RemovePreviouslyInserted(vertexIndex);
            }
            else
            {

                vertex.SetAux(AuxSpin_t::Zero);

                double ratio = CalculateDeterminantRatio(vertex, vertices0Tilde_.at(vertexIndex), vertexIndex);
                double probAcc = static_cast<double>(nPhyscialVertices_) * ratio / KAux_;
                probAcc *= PROBINSERT / PROBREMOVE;

                if (urng_() < std::abs(probAcc))
                {
                    std::cout << "remove accepted" << std::endl;
                    updStats_["Removes"][1]++;
                    verticesUpdated_.push_back(vertexIndex);
                    verticesToRemove_.push_back(vertexIndex);
                    verticesRemovable_.erase(verticesRemovable_.begin() + ii);
                    dataCT_->vertices_.at(vertexIndex) = vertex;
                    assert(vertex.vStart() == dataCT_->vertices_.at(vertexIndex).vStart());
                    assert(vertex.vEnd() == dataCT_->vertices_.at(vertexIndex).vEnd());

                    nPhyscialVertices_ -= 1;
                    nfdata_.FVup_(vertexIndex) = 1.0;
                    nfdata_.FVdown_(vertexIndex) = 1.0;

                    AcceptMove(probAcc);
                }
            }
        }

        Logging::Trace("Start of RemoveVertexSubMatrix.");
    }

    void RemovePreviouslyInserted(const size_t &vertexIndex) //vertexIndex which is in cTilde
    {
        Logging::Trace("Start of RemovePreviouslyInserted.");

        using itType_t = std::vector<size_t>::const_iterator;
        const itType_t ppit = std::find<itType_t, size_t>(verticesUpdated_.begin(), verticesUpdated_.end(), vertexIndex);
        if (ppit == verticesUpdated_.end())
        {
            throw std::runtime_error("Bad index in find vertexIndex in verticesUpdated_!");
        }

        const size_t pp = std::distance<itType_t>(verticesUpdated_.begin(), ppit); //the index of the updated vertex in the gammaSigma Matrices

        const Vertex vFrom = dataCT_->vertices_.at(vertexIndex);
        assert(vFrom.vStart().aux() != AuxSpin_t::Zero); //we are about to remove a vertex that has been inserted !
        VertexPart vpTo;
        vpTo.SetAux(AuxSpin_t::Zero);
        vpTo.SetSpin(FermionSpin_t::Up);
        const double gammappupI = -1.0 / gammaSubMatrix(vFrom.vStart(), vpTo);
        vpTo.SetSpin(FermionSpin_t::Down);
        const double gammappdownI = -1.0 / gammaSubMatrix(vFrom.vEnd(), vpTo);
        const double ratio = gammappupI * gammappdownI * gammadata_.gammaUpI_(pp, pp) * gammadata_.gammaDownI_(pp, pp);
        const double probAcc = PROBINSERT / PROBREMOVE * double(nPhyscialVertices_) * ratio / KAux_;

        if (urng_() < std::abs(probAcc))
        {
            nPhyscialVertices_--;
            if (probAcc < 0.0)
            {
                dataCT_->sign_ *= -1;
            }

            updStats_["Removes"][1]++;
            LinAlg::BlockRankOneDowngrade(gammadata_.gammaUpI_, pp);
            LinAlg::BlockRankOneDowngrade(gammadata_.gammaDownI_, pp);

            const size_t kkm1 = verticesUpdated_.size() - 1;
            std::iter_swap(verticesUpdated_.begin() + pp, verticesUpdated_.begin() + kkm1);
            verticesUpdated_.pop_back();

            //Taken from : https://stackoverflow.com/questions/39912/how-do-i-remove-an-item-from-a-stl-vector-with-a-certain-value
            verticesRemovable_.erase(std::remove(verticesRemovable_.begin(), verticesRemovable_.end(), vertexIndex), verticesRemovable_.end());

            //just a check, make sure that the vertex is not in the verticesToRemove just yet
            const itType_t rrit = std::find<itType_t, size_t>(verticesToRemove_.begin(), verticesToRemove_.end(), vertexIndex);
            if (rrit != verticesToRemove_.end())
            {
                throw std::runtime_error("Bad index in find vertexIndex in verticesToRemove_!");
            }
            //end of check
            verticesToRemove_.push_back(vertexIndex);

            nfdata_.FVup_(vertexIndex) = 1.0;
            nfdata_.FVdown_(vertexIndex) = 1.0;
            dataCT_->vertices_.at(vertexIndex).SetAux(AuxSpin_t::Zero);
        }

        Logging::Trace("Start of RemovePreviouslyInserted.");
    }

    void InsertVertexSubMatrix()
    {

        Logging::Trace("Start of InsertVertexSubMatrix.");

        if (verticesInsertable_.size())
        {
            updStats_["Inserts"][0]++;
            const size_t ii = static_cast<Site_t>(verticesInsertable_.size() * urng_());
            std::cout << "ii = " << ii << std::endl;
            const size_t vertexIndex = verticesInsertable_.at(ii);
            Vertex vertex = vertices0Tilde_.at(vertexIndex);
            // assert(false);
            std::cout << "vertexIndex in insert = " << vertexIndex << std::endl;

            assert(vertex.vStart().aux() == AuxSpin_t::Zero);
            assert(vertex.vEnd().aux() == AuxSpin_t::Zero);

            // assert(false);
            vertex.SetAux(urng_() < 0.5 ? AuxSpin_t::Up : AuxSpin_t::Down);
            Vertex vertextest = vertex;

            assert(vertex.vStart() == vertextest.vStart());
            std::cout << "Here bitch " << std::endl;

            assert(vertex.vStart().aux() != AuxSpin_t::Zero);
            assert(vertex.vEnd().aux() != AuxSpin_t::Zero);
            // assert(false);

            const double ratio = CalculateDeterminantRatio(vertex, vertices0Tilde_.at(vertexIndex), vertexIndex);
            const size_t kknew = nPhyscialVertices_ + 1;

            const double probAcc = PROBREMOVE / PROBINSERT * KAux_ / static_cast<double>(kknew) * ratio;

            //dont propose to insert the same vertex, even if update rejected.
            verticesInsertable_.erase(verticesInsertable_.begin() + ii);

            if (urng_() < std::abs(probAcc))
            {

                std::cout << "insert accepted" << std::endl;

                updStats_["Inserts"][1]++;
                verticesUpdated_.push_back(vertexIndex);
                dataCT_->vertices_.at(vertexIndex) = vertex;
                assert(dataCT_->vertices_.at(vertexIndex).vStart() == vertex.vStart());
                // assert(false);
                // dataCT_->vertices_.at(vertexIndex).SetAux(vertex.vStart().aux());
                // dataCT_->vertices_.at(vertexIndex).vStart() = vertex.vStart();
                // dataCT_->vertices_.at(vertexIndex).vEnd() = vertex.vEnd();

                // std::cout << int(dataCT_->vertices_.at(vertexIndex).aux()) << std::endl;

                assert(vertex.vStart().aux() != AuxSpin_t::Zero);
                // assert(vertex.vStart() == dataCT_->vertices_.at(vertexIndex).vStart());

                assert(dataCT_->vertices_.at(vertexIndex).vStart().aux() != AuxSpin_t::Zero);
                verticesToRemove_.erase(std::remove(verticesToRemove_.begin(), verticesToRemove_.end(), vertexIndex), verticesToRemove_.end());
                verticesRemovable_.push_back(vertexIndex);
                nPhyscialVertices_ += 1;
                nfdata_.FVup_(vertexIndex) = FAux(vertex.vStart());
                nfdata_.FVdown_(vertexIndex) = FAux(vertex.vEnd());

                AcceptMove(probAcc);
            }
        }

        Logging::Trace("Start of InsertVertexSubMatrix.");
    }

    void CleanUpdate()
    {

        Logging::Trace("Start of CleanUpdate.");

        AssertSizes();

        const size_t kkup = dataCT_->vertices_.NUp();
        const size_t kkdown = dataCT_->vertices_.NDown();

        if (kkup != 0)
        {

            for (size_t iUp = 0; iUp < kkup; iUp++)
            {
                for (size_t jUp = 0; jUp < kkup; jUp++)
                {

                    nfdata_.Nup_(iUp, jUp) = GetGreenTau0(dataCT_->vertices_.atUp(iUp), dataCT_->vertices_.atUp(jUp)) * (nfdata_.FVup_(jUp) - 1.0);

                    if (iUp == jUp)
                    {
                        nfdata_.Nup_(iUp, iUp) -= nfdata_.FVup_(iUp);
                    }
                }
            }
            nfdata_.Nup_.Inverse();
        }

        if (kkdown != 0)
        {
            for (size_t iDown = 0; iDown < kkdown; iDown++)
            {
                for (size_t jDown = 0; jDown < kkdown; jDown++)
                {

                    nfdata_.Ndown_(iDown, jDown) = GetGreenTau0(dataCT_->vertices_.atDown(iDown), dataCT_->vertices_.atDown(jDown)) * (nfdata_.FVdown_(jDown) - 1.0);

                    if (iDown == jDown)
                    {
                        nfdata_.Ndown_(iDown, iDown) -= nfdata_.FVdown_(iDown);
                    }
                }
            }
            nfdata_.Ndown_.Inverse();
        }

        Logging::Trace("Start of CleanUpdate.");
    }

    double GetGreenTau0(const VertexPart &x, const VertexPart &y) const
    {
        assert(x.spin() == y.spin());
        if (x.spin() == FermionSpin_t::Up)
        {
            return (dataCT_->green0CachedUp_(x.superSite(), y.superSite(), x.tau() - y.tau()));
        }
        else
        {
#ifdef AFM
            return (dataCT_->green0CachedDown_(x.superSite(), y.superSite(), x.tau() - y.tau()));
#else
            return (dataCT_->green0CachedUp_(x.superSite(), y.superSite(), x.tau() - y.tau()));
#endif
        }
    }

    void Measure()
    {

        Logging::Trace("Start of Measure.");

        AssertSizes();
        const SiteVector_t FVupM1 = (nfdata_.FVup_ - 1.0);
        const SiteVector_t FVdownM1 = (nfdata_.FVdown_ - 1.0);
        DDMGMM(FVupM1, nfdata_.Nup_, *(dataCT_->MupPtr_));
        DDMGMM(FVdownM1, nfdata_.Ndown_, *(dataCT_->MdownPtr_));
        obs_.Measure();

        Logging::Trace("End of CleanUpdate.");
    }

    void SaveMeas()
    {

        obs_.Save();
        // Logging::Trace("updsamespin = " + std::to_string(updsamespin_));
        SaveUpd("Measurements");
        if (mpiUt::Tools::Rank() == mpiUt::Tools::master)
        {
            dataCT_->vertices_.SaveConfig("Config.dat");
        }
        Logging::Info("Finished Saving MarkovChain.");
    }

    void SaveTherm()
    {

        SaveUpd("Thermalization");
        for (UpdStats_t::iterator it = updStats_.begin(); it != updStats_.end(); ++it)
        {
            std::string key = it->first;
            updStats_[key] = 0.0;
        }
    }

    void SaveUpd(const std::string &updType)
    {
        std::vector<UpdStats_t> updStatsVec;
#ifdef HAVEMPI

        mpi::communicator world;
        if (mpiUt::Tools::Rank() == mpiUt::Tools::master)
        {
            mpi::gather(world, updStats_, updStatsVec, mpiUt::Tools::master);
        }
        else
        {
            mpi::gather(world, updStats_, mpiUt::Tools::master);
        }
        if (mpiUt::Tools::Rank() == mpiUt::Tools::master)
        {
            Logging::Info("\n\n Statistics Updates of " + updType + ":\n" + mpiUt::Tools::SaveUpdStats(updStatsVec) + "\n\n");
        }

#else
        updStatsVec.push_back(updStats_);
        Logging::Info("\n\n Statistics Updates of " + updType + ":\n" + mpiUt::Tools::SaveUpdStats(updStatsVec) + "\n\n");

#endif
    }

    void PreparationSteps()
    {

        Logging::Trace("Start of PreparationSteps.");

        nPhyscialVertices_ = dataCT_->vertices_.size();
        InsertNonInteractVertices();
        EnlargeN();
        UpdateGreenInteract();

        Logging::Trace("End of PreparationSteps.");
    }

    void UpdateSteps()
    {
        Logging::Trace("Start of UpdateSteps.");

        UpdateN();
        RemoveNonInteract();
        // dataCT_->vertices_.size() > verticesToRemove_.size() ? RemoveNonInteractEfficient() : RemoveNonInteract();

        Logging::Trace("Start of UpdateSteps.");
    }

    void UpdateN()
    {
        Logging::Trace("Start of UpdateN.");

        if (verticesUpdated_.size())
        {
            const size_t NN = vertices0Tilde_.size();
            const size_t LK = verticesUpdated_.size();

            Matrix_t DMatrixUp(NN, NN);
            Matrix_t DMatrixDown(NN, NN);
            DMatrixUp.Zeros();
            DMatrixDown.Zeros();

            Matrix_t GTildeColUp(NN, LK);
            Matrix_t GTildeColDown(NN, LK);
            Matrix_t NTildeRowUp(LK, NN);
            Matrix_t NTildeRowDown(LK, NN);

            for (size_t i = 0; i < NN; i++)
            {
                const Vertex vI = dataCT_->vertices_.at(i);
                const Vertex vI0Tilde = vertices0Tilde_.at(i);
                DMatrixUp(i, i) = 1.0 / (1.0 + gammaSubMatrix(vI.vStart(), vI0Tilde.vStart()));
                DMatrixDown(i, i) = 1.0 / (1.0 + gammaSubMatrix(vI.vEnd(), vI0Tilde.vEnd()));

                for (size_t j = 0; j < verticesUpdated_.size(); j++)
                {
                    GTildeColUp(i, j) = greendata_.greenInteractUp_(i, verticesUpdated_.at(j));
                    GTildeColDown(i, j) = greendata_.greenInteractDown_(i, verticesUpdated_[j]);

                    NTildeRowUp(j, i) = nfdata_.Nup_(verticesUpdated_[j], i);
                    NTildeRowDown(j, i) = nfdata_.Ndown_(verticesUpdated_[j], i);
                }
            }

            Matrix_t tmp(gammadata_.gammaUpI_.n_rows(), NTildeRowUp.n_cols());
            LinAlg::DGEMM(1.0, 0.0, gammadata_.gammaUpI_, NTildeRowUp, tmp);
            LinAlg::DGEMM(-1.0, 1.0, GTildeColUp, tmp, nfdata_.Nup_);

            LinAlg::DGEMM(1.0, 0.0, gammadata_.gammaDownI_, NTildeRowDown, tmp);
            LinAlg::DGEMM(-1.0, 1.0, GTildeColDown, tmp, nfdata_.Ndown_);

            for (size_t j = 0; j < nfdata_.Nup_.n_cols(); j++)
            {
                for (size_t i = 0; i < nfdata_.Nup_.n_rows(); i++)
                {
                    nfdata_.Nup_(i, j) *= DMatrixUp(i, i);
                    nfdata_.Ndown_(i, j) *= DMatrixDown(i, i);
                }
            }

            gammadata_.gammaUpI_.Clear();
            gammadata_.gammaDownI_.Clear();
        }
        Logging::Trace("End of UpdateN.");
    }

    void EnlargeN()
    {
        Logging::Trace("Start of EnlargeN.");

        //build the B matrices
        const size_t N0 = vertices0_.size(); //here vertices0 is the same as vertices withouth the non-interacting spins
        if (N0)
        {
            Matrix_t BUp(KMAX_UPD_, N0);
            Matrix_t BDown(KMAX_UPD_, N0);

            for (size_t j = 0; j < N0; j++)
            {
                for (size_t i = 0; i < KMAX_UPD_; i++)
                {
                    Vertex vertexI = vertices0Tilde_.at(N0 + i);
                    Vertex vertexJ = dataCT_->vertices_.at(j);
                    BUp(i, j) = GetGreenTau0(vertexI.vStart(), vertexJ.vStart()) * (nfdata_.FVup_(j) - 1.0);
                    BDown(i, j) = GetGreenTau0(vertexI.vEnd(), vertexJ.vEnd()) * (nfdata_.FVdown_(j) - 1.0);
                }
            }

            Matrix_t BUpMTilde(BUp.n_rows(), nfdata_.Nup_.n_cols());
            Matrix_t BDownMTilde(BDown.n_rows(), nfdata_.Ndown_.n_cols());
            LinAlg::DGEMM(1.0, 0.0, BUp, nfdata_.Nup_, BUpMTilde);
            LinAlg::DGEMM(1.0, 0.0, BDown, nfdata_.Ndown_, BDownMTilde);

            const size_t newSize = N0 + KMAX_UPD_;

            nfdata_.Nup_.Resize(newSize, newSize);
            nfdata_.Ndown_.Resize(newSize, newSize);

            nfdata_.FVup_.resize(newSize);
            (nfdata_.FVdown_).resize(newSize);

            //utiliser Lapack ici ?
            for (size_t i = N0; i < newSize; i++)
            {
                nfdata_.FVup_(i) = 1.0;
                (nfdata_.FVdown_)(i) = 1.0;
            }

            //utiliser Lapack ici, slacpy, ?
            Matrix_t eye(newSize - N0, newSize - N0);
            eye.Eye();
            nfdata_.Nup_.SubMat(0, N0, N0 - 1, newSize - 1, 0.0);
            nfdata_.Nup_.SubMat(N0, 0, newSize - 1, N0 - 1, BUpMTilde);
            nfdata_.Nup_.SubMat(N0, N0, newSize - 1, newSize - 1, eye);

            nfdata_.Ndown_.SubMat(0, N0, N0 - 1, newSize - 1, 0.0);
            nfdata_.Ndown_.SubMat(N0, 0, newSize - 1, N0 - 1, BDownMTilde);
            nfdata_.Ndown_.SubMat(N0, N0, newSize - 1, newSize - 1, eye);
        }
        else
        {

            nfdata_.Nup_ = Matrix_t(KMAX_UPD_, KMAX_UPD_);
            nfdata_.Nup_.Eye();
            nfdata_.Ndown_ = Matrix_t(KMAX_UPD_, KMAX_UPD_);
            nfdata_.Ndown_.Eye();
            nfdata_.FVup_ = SiteVector_t(KMAX_UPD_).ones();
            nfdata_.FVdown_ = SiteVector_t(KMAX_UPD_).ones();
        }

        Logging::Trace("End of EnlargeN.");
    }

    void UpdateGreenInteract()
    {

        Logging::Trace("Start of UpdateGreenInteract.");

        const size_t N0 = vertices0_.size();
        const size_t NN = N0 + KMAX_UPD_;
        assert(NN == vertices0Tilde_.size());
        Matrix_t green0up(NN, KMAX_UPD_);
        Matrix_t green0down(NN, KMAX_UPD_);

        //ici updater que ce qui est necessaire, updater seulement les colonnes updater
        greendata_.greenInteractUp_ = nfdata_.Nup_;
        greendata_.greenInteractDown_ = nfdata_.Ndown_;
        for (size_t j = 0; j < N0; j++)
        {

            const double factup = 1.0 / (nfdata_.FVup_(j) - 1.0);
            greendata_.greenInteractUp_.MultCol(j, nfdata_.FVup_(j) * factup);
            greendata_.greenInteractUp_(j, j) -= factup;

            const double factdown = 1.0 / (nfdata_.FVdown_(j) - 1.0);
            greendata_.greenInteractDown_.MultCol(j, nfdata_.FVdown_(j) * factdown);
            greendata_.greenInteractDown_(j, j) -= factdown;
        }

        for (size_t j = 0; j < KMAX_UPD_; j++)
        {

            for (size_t i = 0; i < NN; i++)
            {
                green0up(i, j) = GetGreenTau0(vertices0Tilde_.at(i).vStart(), vertices0Tilde_.at(N0 + j).vStart());
                green0down(i, j) = GetGreenTau0(vertices0Tilde_.at(i).vEnd(), vertices0Tilde_.at(N0 + j).vEnd());
            }
        }

        LinAlg::DGEMM(1.0, 0.0, nfdata_.Nup_, green0up, greendata_.greenInteractUp_, N0);
        LinAlg::DGEMM(1.0, 0.0, nfdata_.Ndown_, green0down, greendata_.greenInteractDown_, N0);

        Logging::Trace("Start of UpdateGreenInteract.");
    }

    void RemoveNonInteract()
    {
        Logging::Trace("in removenoninteract ");
        std::sort(verticesToRemove_.begin(), verticesToRemove_.end());

        for (size_t i = 0; i < verticesToRemove_.size(); i++)
        {
            const size_t index = verticesToRemove_[i] - i; //if verticesToRemove_ in increasing order of index

            //cela serait plus rapide de faire un swap et ensuite d'enlever les dernieres colones?
            nfdata_.Nup_.ShedRowAndCol(index);

            nfdata_.Ndown_.ShedRowAndCol(index);

            nfdata_.FVup_.shed_row(index);
            (nfdata_.FVdown_).shed_row(index);

            dataCT_->vertices_.EraseVertexOneOrbital(index);
        }

        verticesToRemove_.clear();
        verticesInsertable_.clear();
        verticesUpdated_.clear();

        assert(nfdata_.Ndown_.n_rows() == dataCT_->vertices_.size());
        verticesRemovable_.clear();
        AssertSanity();
        Logging::Trace("in removenoninteract ");
    }

    void RemoveNonInteractEfficient()
    {
        Logging::Trace("in RemovenonInteractEfficient ");

        std::sort(verticesToRemove_.begin(), verticesToRemove_.end());
        std::vector<size_t> verticesInteracting;
        for (size_t ii = 0; ii < vertices0Tilde_.size(); ii++)
        {
            verticesInteracting.push_back(ii);
        }

        // std::cout << "Here 1" << std::endl;
        for (size_t ii = 0; ii < verticesToRemove_.size(); ii++)
        {
            // std::cout << "in loop  " << ii << std::endl;
            size_t indexToRemove = verticesToRemove_.at(ii) - ii;
            verticesInteracting.erase(verticesInteracting.begin() + indexToRemove);
        }

        const size_t INTERS = verticesInteracting.size(); //interact size
        assert(INTERS > verticesToRemove_.size());
        // std::cout << "Here 2" << std::endl;

        for (size_t i = 0; i < std::min<size_t>(verticesInteracting.size(), verticesToRemove_.size()); i++)
        {
            const size_t indexToRemove = verticesToRemove_.at(i);
            const size_t indexInteracting = verticesInteracting.at(INTERS - 1 - i);
            if (indexInteracting > indexToRemove)
            {
                nfdata_.Nup_.SwapRowsAndCols(indexToRemove, indexInteracting);
                nfdata_.Ndown_.SwapRowsAndCols(indexToRemove, indexInteracting);
                nfdata_.FVup_.swap_rows(indexToRemove, indexInteracting);
                nfdata_.FVdown_.swap_rows(indexToRemove, indexInteracting);

                dataCT_->vertices_.EraseVertexOneOrbital(indexToRemove);
                // dataCT_->vertices_.SwapVertexOneOrbital(indexToRemove, indexInteracting);
            }
        }

        nfdata_.Nup_.Resize(INTERS, INTERS);
        nfdata_.Ndown_.Resize(INTERS, INTERS);

        nfdata_.FVup_.resize(INTERS);
        nfdata_.FVdown_.resize(INTERS);
        // dataCT_->vertices_.Resize(INTERS);

        verticesToRemove_.clear();
        verticesInsertable_.clear();
        verticesUpdated_.clear();

        assert(nfdata_.Ndown_.n_rows() == dataCT_->vertices_.size());
        verticesRemovable_.clear();
        AssertSanity();

        Logging::Trace("End RemovenonInteractEfficient ");
    }

    void InsertNonInteractVertices()
    {

        Logging::Trace("Start InsertNonInteractVertices. ");

        // AssertSanity();
        vertices0_ = dataCT_->vertices_;
        const size_t N0 = dataCT_->vertices_.size();
        assert(N0 == nfdata_.Nup_.n_rows());

        for (size_t i = 0; i < KMAX_UPD_; i++)
        {
            Vertex vertex = vertexBuilder_.BuildVertexHubbardIntra(urng_);
            assert(vertex.vStart().spin() == FermionSpin_t::Up);
            assert(vertex.vEnd().spin() == FermionSpin_t::Down);

            vertex.SetAux(AuxSpin_t::Zero);
            dataCT_->vertices_.AppendVertex(vertex);

            verticesInsertable_.push_back(N0 + i);
        }
        verticesToRemove_ = verticesInsertable_;

        for (size_t i = 0; i < N0; i++)
        {
            verticesRemovable_.push_back(i);
        }

        vertices0Tilde_ = dataCT_->vertices_;

        Logging::Trace("End InsertNonInteractVertices. ");
    }

    void AssertSanity()
    {
        Logging::Trace("Start AssertSanity. ");

        assert(gammadata_.gammaDownI_.n_rows() == gammadata_.gammaUpI_.n_cols());
        Logging::Trace("expansion order = " + std::to_string(dataCT_->vertices_.size()));

        for (size_t i = 0; i < dataCT_->vertices_.size(); i++)
        {
            std::cout << "i = " << i << std::endl;
            const auto vp = dataCT_->vertices_.at(i).vStart();
            if (vp.aux() == AuxSpin_t::Zero)
            {
                std::cout << "aux is 0 !" << std::endl;
            }

            std::cout << "FAuxUp(aux),  nfdata_.FVup_(i)= " << FAux(vp) << ", " << nfdata_.FVup_(i) << std::endl;
            assert(vp.aux() != AuxSpin_t::Zero);
            assert(std::abs(FAux(dataCT_->vertices_.at(i).vStart()) - nfdata_.FVup_(i)) < 1e-10);
            assert(std::abs(FAux(dataCT_->vertices_.at(i).vEnd()) - nfdata_.FVdown_(i)) < 1e-10);
        }

        Logging::Trace("End AssertSanity. ");
    }

    void PrintVector(std::vector<size_t> v)
    {
        for (size_t i = 0; i < v.size(); i++)
        {
            std::cout << v.at(i) << " ";
        }
        std::cout << std::endl;
    }

  protected:
    //attributes
    std::shared_ptr<Model_t> modelPtr_;
    Utilities::EngineTypeMt19937_t rng_;
    Utilities::UniformRngMt19937_t urng_;
    GammaData gammadata_;
    UpdData upddata_;
    NFData nfdata_;
    GreenData greendata_;

    std::shared_ptr<Markov::Obs::ISDataCT> dataCT_;
    Markov::Obs::Observables obs_;
    Diagrammatic::VertexBuilder vertexBuilder_;

    std::vector<size_t> verticesUpdated_;
    std::vector<size_t> verticesRemovable_; //the interacting vertices that can be removed
    std::vector<size_t> verticesToRemove_;
    std::vector<size_t> verticesInsertable_; //the vertices at which you can insert
    Diagrammatic::Vertices vertices0_;       //the initial config, withouth the noninteracting vertices
    Diagrammatic::Vertices vertices0Tilde_;

    UpdStats_t updStats_; //[0] = number of propsed, [1]=number of accepted

    const size_t KMAX_UPD_;
    const size_t KAux_;

    size_t nPhyscialVertices_;
    size_t updatesProposed_;
};

} // namespace MarkovSub
