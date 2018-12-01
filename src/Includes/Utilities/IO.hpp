#pragma once

#include "Utilities.hpp"
#include "MPIUtilities.hpp"

namespace IO
{
const size_t Nx1 = 1;
const size_t Nx2 = 2;
const size_t Nx4 = 4;
const size_t Nx6 = 6;
const size_t Nx8 = 8;

template <size_t TNX, size_t TNY, size_t TNZ = 1>
class Base_IOModel
{

  public:
    const size_t Nc = TNX * TNY * TNZ;
    static const size_t INVALID;

    Base_IOModel(){};

    void FinishConstructor()
    {
        ConstructfullSiteToIndepSite();
        SetnOfAssociatedSites();
        ConstructFillingSites();
        AssertSanity();
    }

    void ConstructFillingSites()
    {
        const size_t KK = indepSites_.size();
        for (size_t ii = 0; ii < KK; ii++)
        {
            Site_t s1 = indepSites_.at(ii).first;
            Site_t s2 = indepSites_.at(ii).second;
            if (s1 == s2)
            {
                fillingSites_.push_back(s1);
                fillingSitesIndex_.push_back(ii);
            }
        }
    }

    void ConstructfullSiteToIndepSite()
    {
        //construct equivalentSites_ also.
        equivalentSites_.clear();
        equivalentSites_.resize(indepSites_.size());

        for (size_t ii = 0; ii < indepSites_.size(); ii++)
        {
            std::pair<size_t, size_t> pairii = indepSites_.at(ii);
            equivalentSites_.at(ii).push_back(pairii);
        }

        for (Site_t s1 = 0; s1 < Nc; ++s1)
        {
            for (Site_t s2 = 0; s2 < Nc; ++s2)
            {

                const std::pair<Site_t, Site_t> pairSites = GreenSites_.at(s1).at(s2);
                std::vector<std::pair<size_t, size_t>>::iterator llit = std::find(indepSites_.begin(), indepSites_.end(), pairSites);
                if (llit == indepSites_.end())
                {
                    //Try the pair with first and second exchanged:
                    llit = std::find(indepSites_.begin(), indepSites_.end(), std::make_pair(pairSites.second, pairSites.first));
                    if (llit == indepSites_.end())
                    {
                        throw std::runtime_error("Bad index in FindIndepSiteIndex!");
                    }
                }
                const size_t llDistance = std::distance(indepSites_.begin(), llit);
                fullSiteToIndepSite_.push_back(llDistance);
                equivalentSites_.at(llDistance).push_back(std::make_pair(s1, s2));

                if (s1 == s2)
                {
                    assert(indepSites_.at(llDistance).first == indepSites_.at(llDistance).second);
                }
            }
        }
    }

    size_t FindIndepSiteIndex(const Site_t &s1, const Site_t &s2) const
    {
        return fullSiteToIndepSite_.at(s1 * Nc + s2);
    }

    size_t FindIndepSuperSiteIndex(const SuperSite_t &s1, const SuperSite_t &s2, const size_t &NOrb)
    {

        const size_t NSitesIndep = indepSites_.size();
        const size_t siteIndex = fullSiteToIndepSite_.at(s1.first * Nc + s2.first); //arranged by row major ordering here, one of the only places where this happens
        const size_t orbitalIndex = Utilities::GetIndepOrbitalIndex(s1.second, s2.second, NOrb);
        return (siteIndex + orbitalIndex * NSitesIndep);
    }

    void SetnOfAssociatedSites()
    {
        nOfAssociatedSites_.resize(indepSites_.size());
        // nOfFillingSites_ = 0;

        for (size_t ii = 0; ii < Nc; ++ii)
        {
            for (size_t jj = 0; jj < Nc; ++jj)
            {
                const size_t ll = FindIndepSiteIndex(ii, jj);
                nOfAssociatedSites_.at(ll) = nOfAssociatedSites_.at(ll) + 1;
            }
        }
    }

#ifdef DCA
    //read a green in .dat format.
    ClusterCubeCD_t ReadGreenKDat(const std::string &filename, const size_t &NOrb) const
    {
        mpiUt::Print("In IOModel ReadGreenKDat ");

        const size_t shutUpWarning = NOrb;
        std::cout << shutUpWarning << "WARNING, Norb not implemented in ReadGreenKDat" << std::endl;

        ClusterMatrix_t fileMat;
        ClusterMatrixCD_t tmp(Nc, Nc);
        fileMat.load(filename);
        assert(!fileMat.has_nan());
        assert(!fileMat.has_inf());
        assert(fileMat.n_cols == 2 * Nc + 1);
        fileMat.shed_col(0); // we dont want the matsubara frequencies

        ClusterCubeCD_t cubetmp(Nc, Nc, fileMat.n_rows);
        cubetmp.zeros();

        for (size_t n = 0; n < cubetmp.n_slices; ++n)
        {
            tmp.zeros();
            for (size_t KIndex = 0; KIndex < Nc; ++KIndex)
            {
                tmp(KIndex, KIndex) = cd_t(fileMat(n, 2 * KIndex), fileMat(n, 2 * KIndex + 1));
            }

            cubetmp.slice(n) = tmp;
        }

        return cubetmp;
    }
#endif

    //Read green in .dat format.
    ClusterCubeCD_t ReadGreenDat(const std::string &filename, const size_t &NOrb)
    {
        mpiUt::Print("In IOModel ReadGreenNDat ");

        const size_t NN = Nc * NOrb;
        const size_t NOrbIndep = GetNOrbIndep(NOrb);
        const size_t NSitesIndep = indepSites_.size();

        ClusterMatrix_t fileMat;
        ClusterMatrixCD_t tmp(NN, NN);
        fileMat.load(filename);
        assert(!fileMat.has_nan());
        assert(!fileMat.has_inf());

        assert(fileMat.n_cols == NOrbIndep * 2 * NSitesIndep + 1);
        fileMat.shed_col(0); // we dont want the matsubara frequencies

        ClusterCubeCD_t cubetmp(NN, NN, fileMat.n_rows);

        for (size_t n = 0; n < cubetmp.n_slices; ++n)
        {
            for (Orbital_t o1 = 0; o1 < NOrb; ++o1)
            {
                for (Orbital_t o2 = o1; o2 < NOrb; ++o2)
                {
                    for (size_t ii = 0; ii < Nc; ++ii)
                    {
                        for (size_t jj = 0; jj < Nc; ++jj)
                        {
                            const size_t indexIndepSuperSite = FindIndepSuperSiteIndex(std::make_pair(ii, o1), std::make_pair(jj, o2), NOrb);
                            tmp(ii + o1 * Nc, jj + o2 * Nc) = cd_t(fileMat(n, 2 * indexIndepSuperSite), fileMat(n, 2 * indexIndepSuperSite + 1));
                            tmp(jj + o2 * Nc, ii + o1 * Nc) = tmp(ii + o1 * Nc, jj + o2 * Nc); //symmetrize
                        }
                    }
                }
            }

            cubetmp.slice(n) = tmp;
        }

        return cubetmp;
    }

    void SaveCube(const std::string &fname, const ClusterCubeCD_t &green, const double &beta,
                  const size_t &NOrb, const size_t &precision = 14, const bool &saveArma = false)
    {
        assert(!green.has_nan());
        assert(!green.has_inf());
        const size_t NMat = green.n_slices;
        const size_t NOrbIndep = GetNOrbIndep(NOrb);
        const size_t NSitesIndep = this->indepSites_.size();

        assert(green.n_rows == green.n_cols);
        assert(green.n_rows == Nc * NOrb);
        ClusterMatrixCD_t greenOut(NMat, NOrbIndep * NSitesIndep);

        std::ofstream fout;
        fout.open(fname + std::string(".dat"), std::ios::out);
        for (size_t nn = 0; nn < green.n_slices; nn++)
        {
            const double iwn = (2.0 * nn + 1.0) * M_PI / beta;
            fout << std::setprecision(precision) << iwn << " ";

            for (Orbital_t o1 = 0; o1 < NOrb; ++o1)
            {
                for (Orbital_t o2 = o1; o2 < NOrb; ++o2)
                {
                    for (Site_t ii = 0; ii < NSitesIndep; ++ii)
                    {
                        const Site_t r1 = this->indepSites_.at(ii).first;
                        const Site_t r2 = this->indepSites_.at(ii).second;

                        const cd_t value = green(r1 + o1 * Nc, r2 + o2 * Nc, nn);
                        const size_t indexIndepSuperSite = FindIndepSuperSiteIndex(std::make_pair(r1, o1), std::make_pair(r2, o2), NOrb);
                        greenOut(nn, indexIndepSuperSite) = value;
                        fout << std::setprecision(precision) << value.real()
                             << " "
                             << std::setprecision(precision) << value.imag()
                             << " ";
                    }
                }
            }
            fout << "\n";
        }

        fout.close();

        if (saveArma)
        {
            greenOut.save(fname + std::string(".arma"), arma::arma_ascii);
        }
    }

#ifdef DCA
    void SaveK(const std::string &fname, const ClusterCubeCD_t &green, const double &beta, const size_t &NOrb, const size_t &precision = 14) const
    {
        const size_t shutUpWarning = NOrb;
        std::cout << shutUpWarning << "WARNING, Norb not implemented in SaveK" << std::endl;

        std::ofstream fout;
        fout.open(fname + std::string(".dat"), std::ios::out);
        for (size_t nn = 0; nn < green.n_slices; ++nn)
        {
            const double iwn = (2.0 * nn + 1.0) * M_PI / beta;
            fout << std::setprecision(precision) << iwn << " ";

            for (Site_t ii = 0; ii < Nc; ++ii)
            {

                fout << std::setprecision(precision) << green(ii, ii, nn).real()
                     << " "
                     << std::setprecision(precision) << green(ii, ii, nn).imag()
                     << " ";
            }
            fout << "\n";
        }

        fout.close();
    }

#endif

    //get the number of indep orbitals
    static size_t GetNOrbIndep(const size_t &NOrb)
    {
        // size_t NOrbIndep = 0;
        // for (Orbital_t o1 = 0; o1 < NOrb; ++o1)
        // {
        //     for (Orbital_t o2 = o1; o2 < NOrb; ++o2)
        //     {
        //         NOrbIndep++;
        //     }
        // }

        //The number of indep orbitals should be equal to the number of elements in a triangular matrix, i.e:
        // assert(NOrbIndep == NOrb * (NOrb + 1) / 2);
        // return NOrbIndep;
        return (NOrb * (NOrb + 1) / 2);
    }

    std::pair<size_t, size_t> GetIndices(const size_t &indepSuperSiteIndex, const size_t &NOrb) const
    {
        const size_t LL = indepSites_.size();

        const size_t indepSiteIndex = indepSuperSiteIndex % LL;
        const size_t indepOrbitalIndex = indepSuperSiteIndex / LL;

        const auto orbitalPair = GetIndicesOrbital(indepOrbitalIndex, NOrb);
        const size_t o1 = orbitalPair.first;
        const size_t o2 = orbitalPair.second;

        const std::pair<Site_t, Site_t> sites = indepSites_.at(indepSiteIndex);
        return {sites.first + o1 * Nc, sites.second + o2 * Nc};
    }

    std::pair<size_t, size_t> GetIndicesOrbital(const size_t &indepOrbitalIndex, const size_t &NOrb) const
    {
        size_t tmp = 0;
        for (size_t o1 = 0; o1 < NOrb; ++o1)
        {
            for (size_t o2 = o1; o2 < NOrb; ++o2)
            {
                if (tmp == indepOrbitalIndex)
                {
                    return {o1, o2};
                }
                ++tmp;
            }
        }

        throw std::runtime_error("Shit man");
    }

    size_t GetNIndepSuperSites(const size_t &NOrb) const
    {
        return (indepSites_.size() * GetNOrbIndep(NOrb));
    }

    template <typename T1_t, typename T2_t = ClusterMatrixCD_t>
    T2_t IndepToFull(const T1_t &indepElements, const size_t &NOrb) //in practice T1_t will be a Sitevector_t or SitevectorCD_t
    {
        assert(indepElements.n_elem == GetNOrbIndep(NOrb) * indepSites_.size());
        T2_t fullMatrix(NOrb * Nc, NOrb * Nc);
        fullMatrix.zeros();

        for (Orbital_t o1 = 0; o1 < NOrb; ++o1)
        {
            for (Orbital_t o2 = o1; o2 < NOrb; ++o2)
            {
                for (size_t ii = 0; ii < Nc; ++ii)
                {
                    for (size_t jj = 0; jj < Nc; ++jj)
                    {

                        const size_t indexIndepSuperSite = FindIndepSuperSiteIndex(std::make_pair(ii, o1), std::make_pair(jj, o2), NOrb);
                        fullMatrix(ii + o1 * Nc, jj + o2 * Nc) = indepElements(indexIndepSuperSite);
                        fullMatrix(jj + o2 * Nc, ii + o1 * Nc) = fullMatrix(ii + o1 * Nc, jj + o2 * Nc); //symmetrize
                    }
                }
            }
        }

        return fullMatrix;
    }

    //from th full cube return the independant in tabular form
    template <typename T1_t>
    ClusterMatrixCD_t FullCubeToIndep(const T1_t &greenCube) //T1_t = {ClusterCube_t or ClustercubeCD_t}
    {
        const size_t NOrb = greenCube.n_rows / Nc;
        const size_t NIndepSuperSites = GetNIndepSuperSites(NOrb);
        ClusterMatrixCD_t indepTabular(greenCube.n_slices, NIndepSuperSites);
        indepTabular.zeros();

        for (size_t i = 0; i < NIndepSuperSites; ++i)
        {
            const auto indices = GetIndices(i, NOrb);
            const size_t s1 = indices.first;
            const size_t s2 = indices.second;

            for (size_t n = 0; n < greenCube.n_slices; ++n)
            {
                indepTabular(n, i) = greenCube(s1, s2, n);
            }
        }

        return indepTabular;
    }

    void AssertSanity()
    {
        size_t sum = 0.0;
        assert(fillingSites_.size() == fillingSitesIndex_.size());

        for (size_t ii = 0; ii < fillingSitesIndex_.size(); ++ii)
        {
            sum += nOfAssociatedSites_.at(fillingSitesIndex_.at(ii));
        }
        assert(sum == Nc);

        //make sure fullSitetoIndepSite is ok
        assert(fullSiteToIndepSite_.size() == Nc * Nc);

        //make sure equivalentSites_ is ok
        assert(equivalentSites_.size() == indepSites_.size());
        for (size_t ii = 0; ii < equivalentSites_.size(); ++ii)
        {
            std::pair<size_t, size_t> pairii = equivalentSites_.at(ii).at(0);
            for (size_t jj = 0; jj < equivalentSites_.at(ii).size(); ++jj)
            {
                const size_t s1 = equivalentSites_.at(ii).at(jj).first;
                const size_t s2 = equivalentSites_.at(ii).at(jj).second;
                assert(pairii == GreenSites_.at(s1).at(s2));
            }
        }
    }

    ClusterCubeCD_t AverageOrbitals(const ClusterCubeCD_t green) const
    {
        //For now only averages the diagonal blocks.
        const size_t n_rows = green.n_rows;
        const size_t n_cols = green.n_cols;
        const size_t n_slices = green.n_slices;
        assert(n_rows == n_cols);
        const size_t NOrb = green.n_rows / Nc;

        ClusterCubeCD_t result(n_rows, n_cols, n_slices);
        result.zeros();

        using arma::span;
        const size_t Ncm1 = Nc - 1;

        for (size_t oo = 0; oo < NOrb; ++oo)
        {
            result.subcube(span(0, Ncm1), span(0, Ncm1), span(0, n_slices - 1)) += green.subcube(span(Nc * oo, Ncm1 + Nc * oo), span(Nc * oo, Ncm1 + Nc * oo), span(0, n_slices - 1));
        }
        result /= static_cast<double>(NOrb);

        for (size_t oo = 1; oo < NOrb; ++oo)
        {
            result.subcube(span(Nc * oo, Ncm1 + Nc * oo), span(Nc * oo, Ncm1 + Nc * oo), span(0, n_slices - 1)) = result.subcube(span(0, Ncm1), span(0, Ncm1), span(0, n_slices - 1));
        }

        return result;
    }

    std::pair<size_t, size_t> FindSitesRng(const size_t &s1, const size_t &s2, const double &rngDouble)
    {
        const size_t indepSiteIndex = FindIndepSiteIndex(s1, s2);
        const size_t equivalentSize = equivalentSites_.at(indepSiteIndex).size();
        const size_t intRng = rngDouble * equivalentSize;
        return equivalentSites_.at(indepSiteIndex).at(intRng);
    }

    //Getters
    std::vector<std::pair<size_t, size_t>> const indepSites()
    {
        return indepSites_;
    };
    std::vector<std::vector<std::pair<size_t, size_t>>> const GreenSites()
    {
        return GreenSites_;
    };
    std::vector<std::vector<std::pair<size_t, size_t>>> const equivalentSites()
    {
        return equivalentSites_;
    };
    std::vector<size_t> const nOfAssociatedSites()
    {
        return nOfAssociatedSites_;
    };
    std::vector<size_t> const fillingSites()
    {
        return fillingSites_;
    };
    std::vector<size_t> const fillingSitesIndex()
    {
        return fillingSitesIndex_;
    };
    std::vector<size_t> const downEquivalentSites()
    {
        return downEquivalentSites_;
    };

  protected:
    std::vector<std::pair<size_t, size_t>> indepSites_;
    std::vector<std::vector<std::pair<size_t, size_t>>> GreenSites_;
    std::vector<std::vector<std::pair<size_t, size_t>>> equivalentSites_; // for example for square lattice equivalentsites.at(0) = {{0.0}, {1,1} , {2,2}, {3,3}}
    std::vector<size_t> nOfAssociatedSites_;
    std::vector<size_t> fullSiteToIndepSite_;
    std::vector<size_t> fillingSites_;
    std::vector<size_t> fillingSitesIndex_; //the indexes of the fillingsites in the indepSites_
    std::vector<size_t> downEquivalentSites_;
};

template <size_t TNX, size_t TNY, size_t TNZ>
const size_t Base_IOModel<TNX, TNY, TNZ>::INVALID = 999;

class IOTriangle2x2 : public Base_IOModel<Nx2, Nx2>
{
  public:
    IOTriangle2x2() : Base_IOModel<Nx2, Nx2>()
    {
        this->indepSites_ = {
            {0, 0}, {1, 1}, {0, 1}, {0, 3}, {1, 2}};

        this->GreenSites_ = {
            {{0, 0}, {0, 1}, {0, 1}, {0, 3}},
            {{0, 1}, {1, 1}, {1, 2}, {0, 1}},
            {{0, 1}, {1, 2}, {1, 1}, {0, 1}},
            {{0, 3}, {0, 1}, {0, 1}, {0, 0}}};

        FinishConstructor();
    }
};

class IOSquare2x2 : public Base_IOModel<Nx2, Nx2>
{
  public:
    IOSquare2x2() : Base_IOModel<Nx2, Nx2>()
    {
        this->indepSites_ = {
            {0, 0}, {0, 1}, {0, 3}};

        this->GreenSites_ = {
            {{0, 0}, {0, 1}, {0, 1}, {0, 3}},
            {{0, 1}, {0, 0}, {0, 3}, {0, 1}},
            {{0, 1}, {0, 3}, {0, 0}, {0, 1}},
            {{0, 3}, {0, 1}, {0, 1}, {0, 0}}};

        FinishConstructor();
    }
};

class IOSquare2x2_AFM : public Base_IOModel<Nx2, Nx2>
{
  public:
    IOSquare2x2_AFM() : Base_IOModel<Nx2, Nx2>()
    {
        this->indepSites_ = {
            {0, 0}, {1, 1}, {0, 1}, {0, 3}, {1, 2}};

        this->GreenSites_ = {
            {{0, 0}, {0, 1}, {0, 1}, {0, 3}},
            {{0, 1}, {1, 1}, {1, 2}, {0, 1}},
            {{0, 1}, {1, 2}, {1, 1}, {0, 1}},
            {{0, 3}, {0, 1}, {0, 1}, {0, 0}}};

        this->downEquivalentSites_ = {1, 0, 2, 4, 3};
        FinishConstructor();
    }
};

class IOSIAM : public Base_IOModel<Nx1, Nx1>
{
  public:
    IOSIAM() : Base_IOModel<Nx1, Nx1>()
    {
        this->indepSites_ = {
            {0, 0}};

        this->GreenSites_ = {
            {{0, 0}}};

        FinishConstructor();
    }
};

class IOSquare4x4 : public Base_IOModel<Nx4, Nx4>
{
  public:
    IOSquare4x4() : Base_IOModel<Nx4, Nx4>()
    {
        this->indepSites_ = {
            {0, 0}, {0, 1}, {0, 2}, {0, 3}, {0, 5}, {0, 6}, {0, 7}, {0, 10}, {0, 11}, {0, 15}, {1, 1}, {1, 2}, {1, 4}, {1, 5}, {1, 6}, {1, 7}, {1, 9}, {1, 10}, {1, 11}, {1, 13}, {1, 14}, {5, 5}, {5, 6}, {5, 10}};

        this->GreenSites_ = {
            {{0, 0}, {0, 1}, {0, 2}, {0, 3}, {0, 1}, {0, 5}, {0, 6}, {0, 7}, {0, 2}, {0, 6}, {0, 10}, {0, 11}, {0, 3}, {0, 7}, {0, 11}, {0, 15}},
            {{0, 1}, {1, 1}, {1, 2}, {0, 2}, {1, 4}, {1, 5}, {1, 6}, {1, 7}, {1, 7}, {1, 9}, {1, 10}, {1, 11}, {0, 7}, {1, 13}, {1, 14}, {0, 11}},
            {{0, 2}, {1, 2}, {1, 1}, {0, 1}, {1, 7}, {1, 6}, {1, 5}, {1, 4}, {1, 11}, {1, 10}, {1, 9}, {1, 7}, {0, 11}, {1, 14}, {1, 13}, {0, 7}},
            {{0, 3}, {0, 2}, {0, 1}, {0, 0}, {0, 7}, {0, 6}, {0, 5}, {0, 1}, {0, 11}, {0, 10}, {0, 6}, {0, 2}, {0, 15}, {0, 11}, {0, 7}, {0, 3}},
            {{0, 1}, {1, 4}, {1, 7}, {0, 7}, {1, 1}, {1, 5}, {1, 9}, {1, 13}, {1, 2}, {1, 6}, {1, 10}, {1, 14}, {0, 2}, {1, 7}, {1, 11}, {0, 11}},
            {{0, 5}, {1, 5}, {1, 6}, {0, 6}, {1, 5}, {5, 5}, {5, 6}, {1, 9}, {1, 6}, {5, 6}, {5, 10}, {1, 10}, {0, 6}, {1, 9}, {1, 10}, {0, 10}},
            {{0, 6}, {1, 6}, {1, 5}, {0, 5}, {1, 9}, {5, 6}, {5, 5}, {1, 5}, {1, 10}, {5, 10}, {5, 6}, {1, 6}, {0, 10}, {1, 10}, {1, 9}, {0, 6}},
            {{0, 7}, {1, 7}, {1, 4}, {0, 1}, {1, 13}, {1, 9}, {1, 5}, {1, 1}, {1, 14}, {1, 10}, {1, 6}, {1, 2}, {0, 11}, {1, 11}, {1, 7}, {0, 2}},
            {{0, 2}, {1, 7}, {1, 11}, {0, 11}, {1, 2}, {1, 6}, {1, 10}, {1, 14}, {1, 1}, {1, 5}, {1, 9}, {1, 13}, {0, 1}, {1, 4}, {1, 7}, {0, 7}},
            {{0, 6}, {1, 9}, {1, 10}, {0, 10}, {1, 6}, {5, 6}, {5, 10}, {1, 10}, {1, 5}, {5, 5}, {5, 6}, {1, 9}, {0, 5}, {1, 5}, {1, 6}, {0, 6}},
            {{0, 10}, {1, 10}, {1, 9}, {0, 6}, {1, 10}, {5, 10}, {5, 6}, {1, 6}, {1, 9}, {5, 6}, {5, 5}, {1, 5}, {0, 6}, {1, 6}, {1, 5}, {0, 5}},
            {{0, 11}, {1, 11}, {1, 7}, {0, 2}, {1, 14}, {1, 10}, {1, 6}, {1, 2}, {1, 13}, {1, 9}, {1, 5}, {1, 1}, {0, 7}, {1, 7}, {1, 4}, {0, 1}},
            {{0, 3}, {0, 7}, {0, 11}, {0, 15}, {0, 2}, {0, 6}, {0, 10}, {0, 11}, {0, 1}, {0, 5}, {0, 6}, {0, 7}, {0, 0}, {0, 1}, {0, 2}, {0, 3}},
            {{0, 7}, {1, 13}, {1, 14}, {0, 11}, {1, 7}, {1, 9}, {1, 10}, {1, 11}, {1, 4}, {1, 5}, {1, 6}, {1, 7}, {0, 1}, {1, 1}, {1, 2}, {0, 2}},
            {{0, 11}, {1, 14}, {1, 13}, {0, 7}, {1, 11}, {1, 10}, {1, 9}, {1, 7}, {1, 7}, {1, 6}, {1, 5}, {1, 4}, {0, 2}, {1, 2}, {1, 1}, {0, 1}},
            {{0, 15}, {0, 11}, {0, 7}, {0, 3}, {0, 11}, {0, 10}, {0, 6}, {0, 2}, {0, 7}, {0, 6}, {0, 5}, {0, 1}, {0, 3}, {0, 2}, {0, 1}, {0, 0}},
        };

        FinishConstructor();
    }
};

class IOSquare4x4_DCA : public Base_IOModel<Nx4, Nx4>
{
  public:
    IOSquare4x4_DCA() : Base_IOModel<Nx4, Nx4>()
    {
        this->indepSites_ = {{0, 0}, {0, 1}, {0, 2}, {0, 5}, {0, 6}, {0, 10}};

        this->GreenSites_ = {
            {{0, 0}, {0, 1}, {0, 2}, {0, 1}, {0, 1}, {0, 5}, {0, 6}, {0, 5}, {0, 2}, {0, 6}, {0, 10}, {0, 6}, {0, 1}, {0, 5}, {0, 6}, {0, 5}},
            {{0, 1}, {0, 0}, {0, 1}, {0, 2}, {0, 5}, {0, 1}, {0, 5}, {0, 6}, {0, 6}, {0, 2}, {0, 6}, {0, 10}, {0, 5}, {0, 1}, {0, 5}, {0, 6}},
            {{0, 2}, {0, 1}, {0, 0}, {0, 1}, {0, 6}, {0, 5}, {0, 1}, {0, 5}, {0, 10}, {0, 6}, {0, 2}, {0, 6}, {0, 6}, {0, 5}, {0, 1}, {0, 5}},
            {{0, 1}, {0, 2}, {0, 1}, {0, 0}, {0, 5}, {0, 6}, {0, 5}, {0, 1}, {0, 6}, {0, 10}, {0, 6}, {0, 2}, {0, 5}, {0, 6}, {0, 5}, {0, 1}},
            {{0, 1}, {0, 5}, {0, 6}, {0, 5}, {0, 0}, {0, 1}, {0, 2}, {0, 1}, {0, 1}, {0, 5}, {0, 6}, {0, 5}, {0, 2}, {0, 6}, {0, 10}, {0, 6}},
            {{0, 5}, {0, 1}, {0, 5}, {0, 6}, {0, 1}, {0, 0}, {0, 1}, {0, 2}, {0, 5}, {0, 1}, {0, 5}, {0, 6}, {0, 6}, {0, 2}, {0, 6}, {0, 10}},
            {{0, 6}, {0, 5}, {0, 1}, {0, 5}, {0, 2}, {0, 1}, {0, 0}, {0, 1}, {0, 6}, {0, 5}, {0, 1}, {0, 5}, {0, 10}, {0, 6}, {0, 2}, {0, 6}},
            {{0, 5}, {0, 6}, {0, 5}, {0, 1}, {0, 1}, {0, 2}, {0, 1}, {0, 0}, {0, 5}, {0, 6}, {0, 5}, {0, 1}, {0, 6}, {0, 10}, {0, 6}, {0, 2}},
            {{0, 2}, {0, 6}, {0, 10}, {0, 6}, {0, 1}, {0, 5}, {0, 6}, {0, 5}, {0, 0}, {0, 1}, {0, 2}, {0, 1}, {0, 1}, {0, 5}, {0, 6}, {0, 5}},
            {{0, 6}, {0, 2}, {0, 6}, {0, 10}, {0, 5}, {0, 1}, {0, 5}, {0, 6}, {0, 1}, {0, 0}, {0, 1}, {0, 2}, {0, 5}, {0, 1}, {0, 5}, {0, 6}},
            {{0, 10}, {0, 6}, {0, 2}, {0, 6}, {0, 6}, {0, 5}, {0, 1}, {0, 5}, {0, 2}, {0, 1}, {0, 0}, {0, 1}, {0, 6}, {0, 5}, {0, 1}, {0, 5}},
            {{0, 6}, {0, 10}, {0, 6}, {0, 2}, {0, 5}, {0, 6}, {0, 5}, {0, 1}, {0, 1}, {0, 2}, {0, 1}, {0, 0}, {0, 5}, {0, 6}, {0, 5}, {0, 1}},
            {{0, 1}, {0, 5}, {0, 6}, {0, 5}, {0, 2}, {0, 6}, {0, 10}, {0, 6}, {0, 1}, {0, 5}, {0, 6}, {0, 5}, {0, 0}, {0, 1}, {0, 2}, {0, 1}},
            {{0, 5}, {0, 1}, {0, 5}, {0, 6}, {0, 6}, {0, 2}, {0, 6}, {0, 10}, {0, 5}, {0, 1}, {0, 5}, {0, 6}, {0, 1}, {0, 0}, {0, 1}, {0, 2}},
            {{0, 6}, {0, 5}, {0, 1}, {0, 5}, {0, 10}, {0, 6}, {0, 2}, {0, 6}, {0, 6}, {0, 5}, {0, 1}, {0, 5}, {0, 2}, {0, 1}, {0, 0}, {0, 1}},
            {{0, 5}, {0, 6}, {0, 5}, {0, 1}, {0, 6}, {0, 10}, {0, 6}, {0, 2}, {0, 5}, {0, 6}, {0, 5}, {0, 1}, {0, 1}, {0, 2}, {0, 1}, {0, 0}}

        };

        FinishConstructor();
    }
};

class IOSquare6x6 : public Base_IOModel<Nx6, Nx6>
{
  public:
    IOSquare6x6() : Base_IOModel<Nx6, Nx6>()
    {
        this->indepSites_ = {
            {0, 0}, {0, 1}, {0, 11}, {0, 14}, {0, 15}, {0, 16}, {0, 17}, {0, 21}, {0, 22}, {0, 23}, {0, 28}, {0, 29}, {0, 2}, {0, 35}, {1, 1}, {1, 2}, {1, 3}, {1, 4}, {1, 6}, {1, 7}, {1, 8}, {1, 9}, {1, 10}, {0, 3}, {1, 11}, {1, 12}, {1, 13}, {1, 14}, {1, 15}, {1, 16}, {1, 17}, {1, 18}, {1, 19}, {1, 20}, {0, 4}, {1, 21}, {1, 22}, {1, 23}, {1, 25}, {1, 26}, {1, 27}, {1, 28}, {1, 29}, {1, 31}, {1, 32}, {0, 5}, {1, 33}, {1, 34}, {2, 2}, {2, 3}, {2, 7}, {2, 8}, {2, 9}, {2, 10}, {2, 12}, {2, 13}, {0, 7}, {2, 14}, {2, 15}, {2, 16}, {2, 17}, {2, 19}, {2, 20}, {2, 21}, {2, 22}, {2, 23}, {2, 25}, {0, 8}, {2, 26}, {2, 27}, {2, 28}, {2, 32}, {2, 33}, {7, 7}, {7, 8}, {7, 9}, {7, 10}, {7, 14}, {0, 9}, {7, 15}, {7, 16}, {7, 21}, {7, 22}, {7, 28}, {8, 8}, {8, 9}, {8, 13}, {8, 14}, {8, 15}, {0, 10}, {8, 16}, {8, 20}, {8, 21}, {8, 22}, {8, 26}, {8, 27}, {14, 14}, {14, 15}, {14, 21}};

        this->GreenSites_ = {
            {{0, 0}, {0, 1}, {0, 2}, {0, 3}, {0, 4}, {0, 5}, {0, 1}, {0, 7}, {0, 8}, {0, 9}, {0, 10}, {0, 11}, {0, 2}, {0, 8}, {0, 14}, {0, 15}, {0, 16}, {0, 17}, {0, 3}, {0, 9}, {0, 15}, {0, 21}, {0, 22}, {0, 23}, {0, 4}, {0, 10}, {0, 16}, {0, 22}, {0, 28}, {0, 29}, {0, 5}, {0, 11}, {0, 17}, {0, 23}, {0, 29}, {0, 35}},
            {{0, 1}, {1, 1}, {1, 2}, {1, 3}, {1, 4}, {0, 4}, {1, 6}, {1, 7}, {1, 8}, {1, 9}, {1, 10}, {1, 11}, {1, 12}, {1, 13}, {1, 14}, {1, 15}, {1, 16}, {1, 17}, {1, 18}, {1, 19}, {1, 20}, {1, 21}, {1, 22}, {1, 23}, {1, 11}, {1, 25}, {1, 26}, {1, 27}, {1, 28}, {1, 29}, {0, 11}, {1, 31}, {1, 32}, {1, 33}, {1, 34}, {0, 29}},
            {{0, 2}, {1, 2}, {2, 2}, {2, 3}, {1, 3}, {0, 3}, {1, 12}, {2, 7}, {2, 8}, {2, 9}, {2, 10}, {1, 18}, {2, 12}, {2, 13}, {2, 14}, {2, 15}, {2, 16}, {2, 17}, {2, 17}, {2, 19}, {2, 20}, {2, 21}, {2, 22}, {2, 23}, {1, 17}, {2, 25}, {2, 26}, {2, 27}, {2, 28}, {1, 23}, {0, 17}, {1, 32}, {2, 32}, {2, 33}, {1, 33}, {0, 23}},
            {{0, 3}, {1, 3}, {2, 3}, {2, 2}, {1, 2}, {0, 2}, {1, 18}, {2, 10}, {2, 9}, {2, 8}, {2, 7}, {1, 12}, {2, 17}, {2, 16}, {2, 15}, {2, 14}, {2, 13}, {2, 12}, {2, 23}, {2, 22}, {2, 21}, {2, 20}, {2, 19}, {2, 17}, {1, 23}, {2, 28}, {2, 27}, {2, 26}, {2, 25}, {1, 17}, {0, 23}, {1, 33}, {2, 33}, {2, 32}, {1, 32}, {0, 17}},
            {{0, 4}, {1, 4}, {1, 3}, {1, 2}, {1, 1}, {0, 1}, {1, 11}, {1, 10}, {1, 9}, {1, 8}, {1, 7}, {1, 6}, {1, 17}, {1, 16}, {1, 15}, {1, 14}, {1, 13}, {1, 12}, {1, 23}, {1, 22}, {1, 21}, {1, 20}, {1, 19}, {1, 18}, {1, 29}, {1, 28}, {1, 27}, {1, 26}, {1, 25}, {1, 11}, {0, 29}, {1, 34}, {1, 33}, {1, 32}, {1, 31}, {0, 11}},
            {{0, 5}, {0, 4}, {0, 3}, {0, 2}, {0, 1}, {0, 0}, {0, 11}, {0, 10}, {0, 9}, {0, 8}, {0, 7}, {0, 1}, {0, 17}, {0, 16}, {0, 15}, {0, 14}, {0, 8}, {0, 2}, {0, 23}, {0, 22}, {0, 21}, {0, 15}, {0, 9}, {0, 3}, {0, 29}, {0, 28}, {0, 22}, {0, 16}, {0, 10}, {0, 4}, {0, 35}, {0, 29}, {0, 23}, {0, 17}, {0, 11}, {0, 5}},
            {{0, 1}, {1, 6}, {1, 12}, {1, 18}, {1, 11}, {0, 11}, {1, 1}, {1, 7}, {1, 13}, {1, 19}, {1, 25}, {1, 31}, {1, 2}, {1, 8}, {1, 14}, {1, 20}, {1, 26}, {1, 32}, {1, 3}, {1, 9}, {1, 15}, {1, 21}, {1, 27}, {1, 33}, {1, 4}, {1, 10}, {1, 16}, {1, 22}, {1, 28}, {1, 34}, {0, 4}, {1, 11}, {1, 17}, {1, 23}, {1, 29}, {0, 29}},
            {{0, 7}, {1, 7}, {2, 7}, {2, 10}, {1, 10}, {0, 10}, {1, 7}, {7, 7}, {7, 8}, {7, 9}, {7, 10}, {1, 25}, {2, 7}, {7, 8}, {7, 14}, {7, 15}, {7, 16}, {2, 25}, {2, 10}, {7, 9}, {7, 15}, {7, 21}, {7, 22}, {2, 28}, {1, 10}, {7, 10}, {7, 16}, {7, 22}, {7, 28}, {1, 28}, {0, 10}, {1, 25}, {2, 25}, {2, 28}, {1, 28}, {0, 28}},
            {{0, 8}, {1, 8}, {2, 8}, {2, 9}, {1, 9}, {0, 9}, {1, 13}, {7, 8}, {8, 8}, {8, 9}, {7, 9}, {1, 19}, {2, 13}, {8, 13}, {8, 14}, {8, 15}, {8, 16}, {2, 19}, {2, 16}, {8, 16}, {8, 20}, {8, 21}, {8, 22}, {2, 22}, {1, 16}, {7, 16}, {8, 26}, {8, 27}, {7, 22}, {1, 22}, {0, 16}, {1, 26}, {2, 26}, {2, 27}, {1, 27}, {0, 22}},
            {{0, 9}, {1, 9}, {2, 9}, {2, 8}, {1, 8}, {0, 8}, {1, 19}, {7, 9}, {8, 9}, {8, 8}, {7, 8}, {1, 13}, {2, 19}, {8, 16}, {8, 15}, {8, 14}, {8, 13}, {2, 13}, {2, 22}, {8, 22}, {8, 21}, {8, 20}, {8, 16}, {2, 16}, {1, 22}, {7, 22}, {8, 27}, {8, 26}, {7, 16}, {1, 16}, {0, 22}, {1, 27}, {2, 27}, {2, 26}, {1, 26}, {0, 16}},
            {{0, 10}, {1, 10}, {2, 10}, {2, 7}, {1, 7}, {0, 7}, {1, 25}, {7, 10}, {7, 9}, {7, 8}, {7, 7}, {1, 7}, {2, 25}, {7, 16}, {7, 15}, {7, 14}, {7, 8}, {2, 7}, {2, 28}, {7, 22}, {7, 21}, {7, 15}, {7, 9}, {2, 10}, {1, 28}, {7, 28}, {7, 22}, {7, 16}, {7, 10}, {1, 10}, {0, 28}, {1, 28}, {2, 28}, {2, 25}, {1, 25}, {0, 10}},
            {{0, 11}, {1, 11}, {1, 18}, {1, 12}, {1, 6}, {0, 1}, {1, 31}, {1, 25}, {1, 19}, {1, 13}, {1, 7}, {1, 1}, {1, 32}, {1, 26}, {1, 20}, {1, 14}, {1, 8}, {1, 2}, {1, 33}, {1, 27}, {1, 21}, {1, 15}, {1, 9}, {1, 3}, {1, 34}, {1, 28}, {1, 22}, {1, 16}, {1, 10}, {1, 4}, {0, 29}, {1, 29}, {1, 23}, {1, 17}, {1, 11}, {0, 4}},
            {{0, 2}, {1, 12}, {2, 12}, {2, 17}, {1, 17}, {0, 17}, {1, 2}, {2, 7}, {2, 13}, {2, 19}, {2, 25}, {1, 32}, {2, 2}, {2, 8}, {2, 14}, {2, 20}, {2, 26}, {2, 32}, {2, 3}, {2, 9}, {2, 15}, {2, 21}, {2, 27}, {2, 33}, {1, 3}, {2, 10}, {2, 16}, {2, 22}, {2, 28}, {1, 33}, {0, 3}, {1, 18}, {2, 17}, {2, 23}, {1, 23}, {0, 23}},
            {{0, 8}, {1, 13}, {2, 13}, {2, 16}, {1, 16}, {0, 16}, {1, 8}, {7, 8}, {8, 13}, {8, 16}, {7, 16}, {1, 26}, {2, 8}, {8, 8}, {8, 14}, {8, 20}, {8, 26}, {2, 26}, {2, 9}, {8, 9}, {8, 15}, {8, 21}, {8, 27}, {2, 27}, {1, 9}, {7, 9}, {8, 16}, {8, 22}, {7, 22}, {1, 27}, {0, 9}, {1, 19}, {2, 19}, {2, 22}, {1, 22}, {0, 22}},
            {{0, 14}, {1, 14}, {2, 14}, {2, 15}, {1, 15}, {0, 15}, {1, 14}, {7, 14}, {8, 14}, {8, 15}, {7, 15}, {1, 20}, {2, 14}, {8, 14}, {14, 14}, {14, 15}, {8, 20}, {2, 20}, {2, 15}, {8, 15}, {14, 15}, {14, 21}, {8, 21}, {2, 21}, {1, 15}, {7, 15}, {8, 20}, {8, 21}, {7, 21}, {1, 21}, {0, 15}, {1, 20}, {2, 20}, {2, 21}, {1, 21}, {0, 21}},
            {{0, 15}, {1, 15}, {2, 15}, {2, 14}, {1, 14}, {0, 14}, {1, 20}, {7, 15}, {8, 15}, {8, 14}, {7, 14}, {1, 14}, {2, 20}, {8, 20}, {14, 15}, {14, 14}, {8, 14}, {2, 14}, {2, 21}, {8, 21}, {14, 21}, {14, 15}, {8, 15}, {2, 15}, {1, 21}, {7, 21}, {8, 21}, {8, 20}, {7, 15}, {1, 15}, {0, 21}, {1, 21}, {2, 21}, {2, 20}, {1, 20}, {0, 15}},
            {{0, 16}, {1, 16}, {2, 16}, {2, 13}, {1, 13}, {0, 8}, {1, 26}, {7, 16}, {8, 16}, {8, 13}, {7, 8}, {1, 8}, {2, 26}, {8, 26}, {8, 20}, {8, 14}, {8, 8}, {2, 8}, {2, 27}, {8, 27}, {8, 21}, {8, 15}, {8, 9}, {2, 9}, {1, 27}, {7, 22}, {8, 22}, {8, 16}, {7, 9}, {1, 9}, {0, 22}, {1, 22}, {2, 22}, {2, 19}, {1, 19}, {0, 9}},
            {{0, 17}, {1, 17}, {2, 17}, {2, 12}, {1, 12}, {0, 2}, {1, 32}, {2, 25}, {2, 19}, {2, 13}, {2, 7}, {1, 2}, {2, 32}, {2, 26}, {2, 20}, {2, 14}, {2, 8}, {2, 2}, {2, 33}, {2, 27}, {2, 21}, {2, 15}, {2, 9}, {2, 3}, {1, 33}, {2, 28}, {2, 22}, {2, 16}, {2, 10}, {1, 3}, {0, 23}, {1, 23}, {2, 23}, {2, 17}, {1, 18}, {0, 3}},
            {{0, 3}, {1, 18}, {2, 17}, {2, 23}, {1, 23}, {0, 23}, {1, 3}, {2, 10}, {2, 16}, {2, 22}, {2, 28}, {1, 33}, {2, 3}, {2, 9}, {2, 15}, {2, 21}, {2, 27}, {2, 33}, {2, 2}, {2, 8}, {2, 14}, {2, 20}, {2, 26}, {2, 32}, {1, 2}, {2, 7}, {2, 13}, {2, 19}, {2, 25}, {1, 32}, {0, 2}, {1, 12}, {2, 12}, {2, 17}, {1, 17}, {0, 17}},
            {{0, 9}, {1, 19}, {2, 19}, {2, 22}, {1, 22}, {0, 22}, {1, 9}, {7, 9}, {8, 16}, {8, 22}, {7, 22}, {1, 27}, {2, 9}, {8, 9}, {8, 15}, {8, 21}, {8, 27}, {2, 27}, {2, 8}, {8, 8}, {8, 14}, {8, 20}, {8, 26}, {2, 26}, {1, 8}, {7, 8}, {8, 13}, {8, 16}, {7, 16}, {1, 26}, {0, 8}, {1, 13}, {2, 13}, {2, 16}, {1, 16}, {0, 16}},
            {{0, 15}, {1, 20}, {2, 20}, {2, 21}, {1, 21}, {0, 21}, {1, 15}, {7, 15}, {8, 20}, {8, 21}, {7, 21}, {1, 21}, {2, 15}, {8, 15}, {14, 15}, {14, 21}, {8, 21}, {2, 21}, {2, 14}, {8, 14}, {14, 14}, {14, 15}, {8, 20}, {2, 20}, {1, 14}, {7, 14}, {8, 14}, {8, 15}, {7, 15}, {1, 20}, {0, 14}, {1, 14}, {2, 14}, {2, 15}, {1, 15}, {0, 15}},
            {{0, 21}, {1, 21}, {2, 21}, {2, 20}, {1, 20}, {0, 15}, {1, 21}, {7, 21}, {8, 21}, {8, 20}, {7, 15}, {1, 15}, {2, 21}, {8, 21}, {14, 21}, {14, 15}, {8, 15}, {2, 15}, {2, 20}, {8, 20}, {14, 15}, {14, 14}, {8, 14}, {2, 14}, {1, 20}, {7, 15}, {8, 15}, {8, 14}, {7, 14}, {1, 14}, {0, 15}, {1, 15}, {2, 15}, {2, 14}, {1, 14}, {0, 14}},
            {{0, 22}, {1, 22}, {2, 22}, {2, 19}, {1, 19}, {0, 9}, {1, 27}, {7, 22}, {8, 22}, {8, 16}, {7, 9}, {1, 9}, {2, 27}, {8, 27}, {8, 21}, {8, 15}, {8, 9}, {2, 9}, {2, 26}, {8, 26}, {8, 20}, {8, 14}, {8, 8}, {2, 8}, {1, 26}, {7, 16}, {8, 16}, {8, 13}, {7, 8}, {1, 8}, {0, 16}, {1, 16}, {2, 16}, {2, 13}, {1, 13}, {0, 8}},
            {{0, 23}, {1, 23}, {2, 23}, {2, 17}, {1, 18}, {0, 3}, {1, 33}, {2, 28}, {2, 22}, {2, 16}, {2, 10}, {1, 3}, {2, 33}, {2, 27}, {2, 21}, {2, 15}, {2, 9}, {2, 3}, {2, 32}, {2, 26}, {2, 20}, {2, 14}, {2, 8}, {2, 2}, {1, 32}, {2, 25}, {2, 19}, {2, 13}, {2, 7}, {1, 2}, {0, 17}, {1, 17}, {2, 17}, {2, 12}, {1, 12}, {0, 2}},
            {{0, 4}, {1, 11}, {1, 17}, {1, 23}, {1, 29}, {0, 29}, {1, 4}, {1, 10}, {1, 16}, {1, 22}, {1, 28}, {1, 34}, {1, 3}, {1, 9}, {1, 15}, {1, 21}, {1, 27}, {1, 33}, {1, 2}, {1, 8}, {1, 14}, {1, 20}, {1, 26}, {1, 32}, {1, 1}, {1, 7}, {1, 13}, {1, 19}, {1, 25}, {1, 31}, {0, 1}, {1, 6}, {1, 12}, {1, 18}, {1, 11}, {0, 11}},
            {{0, 10}, {1, 25}, {2, 25}, {2, 28}, {1, 28}, {0, 28}, {1, 10}, {7, 10}, {7, 16}, {7, 22}, {7, 28}, {1, 28}, {2, 10}, {7, 9}, {7, 15}, {7, 21}, {7, 22}, {2, 28}, {2, 7}, {7, 8}, {7, 14}, {7, 15}, {7, 16}, {2, 25}, {1, 7}, {7, 7}, {7, 8}, {7, 9}, {7, 10}, {1, 25}, {0, 7}, {1, 7}, {2, 7}, {2, 10}, {1, 10}, {0, 10}},
            {{0, 16}, {1, 26}, {2, 26}, {2, 27}, {1, 27}, {0, 22}, {1, 16}, {7, 16}, {8, 26}, {8, 27}, {7, 22}, {1, 22}, {2, 16}, {8, 16}, {8, 20}, {8, 21}, {8, 22}, {2, 22}, {2, 13}, {8, 13}, {8, 14}, {8, 15}, {8, 16}, {2, 19}, {1, 13}, {7, 8}, {8, 8}, {8, 9}, {7, 9}, {1, 19}, {0, 8}, {1, 8}, {2, 8}, {2, 9}, {1, 9}, {0, 9}},
            {{0, 22}, {1, 27}, {2, 27}, {2, 26}, {1, 26}, {0, 16}, {1, 22}, {7, 22}, {8, 27}, {8, 26}, {7, 16}, {1, 16}, {2, 22}, {8, 22}, {8, 21}, {8, 20}, {8, 16}, {2, 16}, {2, 19}, {8, 16}, {8, 15}, {8, 14}, {8, 13}, {2, 13}, {1, 19}, {7, 9}, {8, 9}, {8, 8}, {7, 8}, {1, 13}, {0, 9}, {1, 9}, {2, 9}, {2, 8}, {1, 8}, {0, 8}},
            {{0, 28}, {1, 28}, {2, 28}, {2, 25}, {1, 25}, {0, 10}, {1, 28}, {7, 28}, {7, 22}, {7, 16}, {7, 10}, {1, 10}, {2, 28}, {7, 22}, {7, 21}, {7, 15}, {7, 9}, {2, 10}, {2, 25}, {7, 16}, {7, 15}, {7, 14}, {7, 8}, {2, 7}, {1, 25}, {7, 10}, {7, 9}, {7, 8}, {7, 7}, {1, 7}, {0, 10}, {1, 10}, {2, 10}, {2, 7}, {1, 7}, {0, 7}},
            {{0, 29}, {1, 29}, {1, 23}, {1, 17}, {1, 11}, {0, 4}, {1, 34}, {1, 28}, {1, 22}, {1, 16}, {1, 10}, {1, 4}, {1, 33}, {1, 27}, {1, 21}, {1, 15}, {1, 9}, {1, 3}, {1, 32}, {1, 26}, {1, 20}, {1, 14}, {1, 8}, {1, 2}, {1, 31}, {1, 25}, {1, 19}, {1, 13}, {1, 7}, {1, 1}, {0, 11}, {1, 11}, {1, 18}, {1, 12}, {1, 6}, {0, 1}},
            {{0, 5}, {0, 11}, {0, 17}, {0, 23}, {0, 29}, {0, 35}, {0, 4}, {0, 10}, {0, 16}, {0, 22}, {0, 28}, {0, 29}, {0, 3}, {0, 9}, {0, 15}, {0, 21}, {0, 22}, {0, 23}, {0, 2}, {0, 8}, {0, 14}, {0, 15}, {0, 16}, {0, 17}, {0, 1}, {0, 7}, {0, 8}, {0, 9}, {0, 10}, {0, 11}, {0, 0}, {0, 1}, {0, 2}, {0, 3}, {0, 4}, {0, 5}},
            {{0, 11}, {1, 31}, {1, 32}, {1, 33}, {1, 34}, {0, 29}, {1, 11}, {1, 25}, {1, 26}, {1, 27}, {1, 28}, {1, 29}, {1, 18}, {1, 19}, {1, 20}, {1, 21}, {1, 22}, {1, 23}, {1, 12}, {1, 13}, {1, 14}, {1, 15}, {1, 16}, {1, 17}, {1, 6}, {1, 7}, {1, 8}, {1, 9}, {1, 10}, {1, 11}, {0, 1}, {1, 1}, {1, 2}, {1, 3}, {1, 4}, {0, 4}},
            {{0, 17}, {1, 32}, {2, 32}, {2, 33}, {1, 33}, {0, 23}, {1, 17}, {2, 25}, {2, 26}, {2, 27}, {2, 28}, {1, 23}, {2, 17}, {2, 19}, {2, 20}, {2, 21}, {2, 22}, {2, 23}, {2, 12}, {2, 13}, {2, 14}, {2, 15}, {2, 16}, {2, 17}, {1, 12}, {2, 7}, {2, 8}, {2, 9}, {2, 10}, {1, 18}, {0, 2}, {1, 2}, {2, 2}, {2, 3}, {1, 3}, {0, 3}},
            {{0, 23}, {1, 33}, {2, 33}, {2, 32}, {1, 32}, {0, 17}, {1, 23}, {2, 28}, {2, 27}, {2, 26}, {2, 25}, {1, 17}, {2, 23}, {2, 22}, {2, 21}, {2, 20}, {2, 19}, {2, 17}, {2, 17}, {2, 16}, {2, 15}, {2, 14}, {2, 13}, {2, 12}, {1, 18}, {2, 10}, {2, 9}, {2, 8}, {2, 7}, {1, 12}, {0, 3}, {1, 3}, {2, 3}, {2, 2}, {1, 2}, {0, 2}},
            {{0, 29}, {1, 34}, {1, 33}, {1, 32}, {1, 31}, {0, 11}, {1, 29}, {1, 28}, {1, 27}, {1, 26}, {1, 25}, {1, 11}, {1, 23}, {1, 22}, {1, 21}, {1, 20}, {1, 19}, {1, 18}, {1, 17}, {1, 16}, {1, 15}, {1, 14}, {1, 13}, {1, 12}, {1, 11}, {1, 10}, {1, 9}, {1, 8}, {1, 7}, {1, 6}, {0, 4}, {1, 4}, {1, 3}, {1, 2}, {1, 1}, {0, 1}},
            {{0, 35}, {0, 29}, {0, 23}, {0, 17}, {0, 11}, {0, 5}, {0, 29}, {0, 28}, {0, 22}, {0, 16}, {0, 10}, {0, 4}, {0, 23}, {0, 22}, {0, 21}, {0, 15}, {0, 9}, {0, 3}, {0, 17}, {0, 16}, {0, 15}, {0, 14}, {0, 8}, {0, 2}, {0, 11}, {0, 10}, {0, 9}, {0, 8}, {0, 7}, {0, 1}, {0, 5}, {0, 4}, {0, 3}, {0, 2}, {0, 1}, {0, 0}}};

        FinishConstructor();
    }
};

class IOSquare8x8 : public Base_IOModel<Nx8, Nx8>
{
  public:
    IOSquare8x8() : Base_IOModel<Nx8, Nx8>()
    {
        this->indepSites_ = {
            {0, 0}, {0, 1}, {0, 11}, {2, 10}, {2, 11}, {2, 12}, {2, 13}, {2, 14}, {2, 16}, {2, 17}, {2, 18}, {2, 19}, {2, 20}, {0, 12}, {2, 21}, {2, 22}, {2, 23}, {2, 24}, {2, 25}, {2, 26}, {2, 27}, {2, 28}, {2, 29}, {2, 30}, {0, 13}, {2, 31}, {2, 32}, {2, 33}, {2, 34}, {2, 35}, {2, 36}, {2, 37}, {2, 38}, {2, 39}, {2, 41}, {0, 14}, {2, 42}, {2, 43}, {2, 44}, {2, 45}, {2, 46}, {2, 47}, {2, 49}, {2, 50}, {2, 51}, {2, 52}, {0, 15}, {2, 53}, {2, 54}, {2, 58}, {2, 59}, {2, 60}, {2, 61}, {3, 3}, {3, 4}, {3, 9}, {3, 10}, {0, 18}, {3, 11}, {3, 12}, {3, 13}, {3, 14}, {3, 17}, {3, 18}, {3, 19}, {3, 20}, {3, 21}, {3, 22}, {0, 19}, {3, 24}, {3, 25}, {3, 26}, {3, 27}, {3, 28}, {3, 29}, {3, 30}, {3, 31}, {3, 33}, {3, 34}, {0, 20}, {3, 35}, {3, 36}, {3, 37}, {3, 38}, {3, 39}, {3, 41}, {3, 42}, {3, 43}, {3, 44}, {3, 45}, {0, 21}, {3, 46}, {3, 49}, {3, 50}, {3, 51}, {3, 52}, {3, 53}, {3, 54}, {3, 59}, {3, 60}, {9, 9}, {0, 22}, {9, 10}, {9, 11}, {9, 12}, {9, 13}, {9, 14}, {9, 18}, {9, 19}, {9, 20}, {9, 21}, {9, 22}, {0, 2}, {0, 23}, {9, 27}, {9, 28}, {9, 29}, {9, 30}, {9, 36}, {9, 37}, {9, 38}, {9, 45}, {9, 46}, {9, 54}, {0, 27}, {10, 10}, {10, 11}, {10, 12}, {10, 13}, {10, 17}, {10, 18}, {10, 19}, {10, 20}, {10, 21}, {10, 22}, {0, 28}, {10, 25}, {10, 26}, {10, 27}, {10, 28}, {10, 29}, {10, 30}, {10, 33}, {10, 34}, {10, 35}, {10, 36}, {0, 29}, {10, 37}, {10, 38}, {10, 42}, {10, 43}, {10, 44}, {10, 45}, {10, 46}, {10, 50}, {10, 51}, {10, 52}, {0, 30}, {10, 53}, {11, 11}, {11, 12}, {11, 18}, {11, 19}, {11, 20}, {11, 21}, {11, 25}, {11, 26}, {11, 27}, {0, 31}, {11, 28}, {11, 29}, {11, 30}, {11, 34}, {11, 35}, {11, 36}, {11, 37}, {11, 38}, {11, 42}, {11, 43}, {0, 36}, {11, 44}, {11, 45}, {11, 51}, {11, 52}, {18, 18}, {18, 19}, {18, 20}, {18, 21}, {18, 27}, {18, 28}, {0, 37}, {18, 29}, {18, 36}, {18, 37}, {18, 45}, {19, 19}, {19, 20}, {19, 26}, {19, 27}, {19, 28}, {19, 29}, {0, 38}, {19, 35}, {19, 36}, {19, 37}, {19, 43}, {19, 44}, {27, 27}, {27, 28}, {27, 36}, {0, 39}, {0, 3}, {0, 45}, {0, 46}, {0, 47}, {0, 54}, {0, 55}, {0, 63}, {1, 1}, {1, 2}, {1, 3}, {1, 4}, {0, 4}, {1, 5}, {1, 6}, {1, 8}, {1, 9}, {1, 10}, {1, 11}, {1, 12}, {1, 13}, {1, 14}, {1, 15}, {0, 5}, {1, 16}, {1, 17}, {1, 18}, {1, 19}, {1, 20}, {1, 21}, {1, 22}, {1, 23}, {1, 24}, {1, 25}, {0, 6}, {1, 26}, {1, 27}, {1, 28}, {1, 29}, {1, 30}, {1, 31}, {1, 32}, {1, 33}, {1, 34}, {1, 35}, {0, 7}, {1, 36}, {1, 37}, {1, 38}, {1, 39}, {1, 40}, {1, 41}, {1, 42}, {1, 43}, {1, 44}, {1, 45}, {0, 9}, {1, 46}, {1, 47}, {1, 49}, {1, 50}, {1, 51}, {1, 52}, {1, 53}, {1, 54}, {1, 55}, {1, 57}, {0, 10}, {1, 58}, {1, 59}, {1, 60}, {1, 61}, {1, 62}, {2, 2}, {2, 3}, {2, 4}, {2, 5}, {2, 9}};

        this->GreenSites_ = {
            {{0, 0}, {0, 1}, {0, 2}, {0, 3}, {0, 4}, {0, 5}, {0, 6}, {0, 7}, {0, 1}, {0, 9}, {0, 10}, {0, 11}, {0, 12}, {0, 13}, {0, 14}, {0, 15}, {0, 2}, {0, 10}, {0, 18}, {0, 19}, {0, 20}, {0, 21}, {0, 22}, {0, 23}, {0, 3}, {0, 11}, {0, 19}, {0, 27}, {0, 28}, {0, 29}, {0, 30}, {0, 31}, {0, 4}, {0, 12}, {0, 20}, {0, 28}, {0, 36}, {0, 37}, {0, 38}, {0, 39}, {0, 5}, {0, 13}, {0, 21}, {0, 29}, {0, 37}, {0, 45}, {0, 46}, {0, 47}, {0, 6}, {0, 14}, {0, 22}, {0, 30}, {0, 38}, {0, 46}, {0, 54}, {0, 55}, {0, 7}, {0, 15}, {0, 23}, {0, 31}, {0, 39}, {0, 47}, {0, 55}, {0, 63}},
            {{0, 1}, {1, 1}, {1, 2}, {1, 3}, {1, 4}, {1, 5}, {1, 6}, {0, 6}, {1, 8}, {1, 9}, {1, 10}, {1, 11}, {1, 12}, {1, 13}, {1, 14}, {1, 15}, {1, 16}, {1, 17}, {1, 18}, {1, 19}, {1, 20}, {1, 21}, {1, 22}, {1, 23}, {1, 24}, {1, 25}, {1, 26}, {1, 27}, {1, 28}, {1, 29}, {1, 30}, {1, 31}, {1, 32}, {1, 33}, {1, 34}, {1, 35}, {1, 36}, {1, 37}, {1, 38}, {1, 39}, {1, 40}, {1, 41}, {1, 42}, {1, 43}, {1, 44}, {1, 45}, {1, 46}, {1, 47}, {1, 15}, {1, 49}, {1, 50}, {1, 51}, {1, 52}, {1, 53}, {1, 54}, {1, 55}, {0, 15}, {1, 57}, {1, 58}, {1, 59}, {1, 60}, {1, 61}, {1, 62}, {0, 55}},
            {{0, 2}, {1, 2}, {2, 2}, {2, 3}, {2, 4}, {2, 5}, {1, 5}, {0, 5}, {1, 16}, {2, 9}, {2, 10}, {2, 11}, {2, 12}, {2, 13}, {2, 14}, {1, 40}, {2, 16}, {2, 17}, {2, 18}, {2, 19}, {2, 20}, {2, 21}, {2, 22}, {2, 23}, {2, 24}, {2, 25}, {2, 26}, {2, 27}, {2, 28}, {2, 29}, {2, 30}, {2, 31}, {2, 32}, {2, 33}, {2, 34}, {2, 35}, {2, 36}, {2, 37}, {2, 38}, {2, 39}, {2, 23}, {2, 41}, {2, 42}, {2, 43}, {2, 44}, {2, 45}, {2, 46}, {2, 47}, {1, 23}, {2, 49}, {2, 50}, {2, 51}, {2, 52}, {2, 53}, {2, 54}, {1, 47}, {0, 23}, {1, 58}, {2, 58}, {2, 59}, {2, 60}, {2, 61}, {1, 61}, {0, 47}},
            {{0, 3}, {1, 3}, {2, 3}, {3, 3}, {3, 4}, {2, 4}, {1, 4}, {0, 4}, {1, 24}, {3, 9}, {3, 10}, {3, 11}, {3, 12}, {3, 13}, {3, 14}, {1, 32}, {2, 24}, {3, 17}, {3, 18}, {3, 19}, {3, 20}, {3, 21}, {3, 22}, {2, 32}, {3, 24}, {3, 25}, {3, 26}, {3, 27}, {3, 28}, {3, 29}, {3, 30}, {3, 31}, {3, 31}, {3, 33}, {3, 34}, {3, 35}, {3, 36}, {3, 37}, {3, 38}, {3, 39}, {2, 31}, {3, 41}, {3, 42}, {3, 43}, {3, 44}, {3, 45}, {3, 46}, {2, 39}, {1, 31}, {3, 49}, {3, 50}, {3, 51}, {3, 52}, {3, 53}, {3, 54}, {1, 39}, {0, 31}, {1, 59}, {2, 59}, {3, 59}, {3, 60}, {2, 60}, {1, 60}, {0, 39}},
            {{0, 4}, {1, 4}, {2, 4}, {3, 4}, {3, 3}, {2, 3}, {1, 3}, {0, 3}, {1, 32}, {3, 14}, {3, 13}, {3, 12}, {3, 11}, {3, 10}, {3, 9}, {1, 24}, {2, 32}, {3, 22}, {3, 21}, {3, 20}, {3, 19}, {3, 18}, {3, 17}, {2, 24}, {3, 31}, {3, 30}, {3, 29}, {3, 28}, {3, 27}, {3, 26}, {3, 25}, {3, 24}, {3, 39}, {3, 38}, {3, 37}, {3, 36}, {3, 35}, {3, 34}, {3, 33}, {3, 31}, {2, 39}, {3, 46}, {3, 45}, {3, 44}, {3, 43}, {3, 42}, {3, 41}, {2, 31}, {1, 39}, {3, 54}, {3, 53}, {3, 52}, {3, 51}, {3, 50}, {3, 49}, {1, 31}, {0, 39}, {1, 60}, {2, 60}, {3, 60}, {3, 59}, {2, 59}, {1, 59}, {0, 31}},
            {{0, 5}, {1, 5}, {2, 5}, {2, 4}, {2, 3}, {2, 2}, {1, 2}, {0, 2}, {1, 40}, {2, 14}, {2, 13}, {2, 12}, {2, 11}, {2, 10}, {2, 9}, {1, 16}, {2, 23}, {2, 22}, {2, 21}, {2, 20}, {2, 19}, {2, 18}, {2, 17}, {2, 16}, {2, 31}, {2, 30}, {2, 29}, {2, 28}, {2, 27}, {2, 26}, {2, 25}, {2, 24}, {2, 39}, {2, 38}, {2, 37}, {2, 36}, {2, 35}, {2, 34}, {2, 33}, {2, 32}, {2, 47}, {2, 46}, {2, 45}, {2, 44}, {2, 43}, {2, 42}, {2, 41}, {2, 23}, {1, 47}, {2, 54}, {2, 53}, {2, 52}, {2, 51}, {2, 50}, {2, 49}, {1, 23}, {0, 47}, {1, 61}, {2, 61}, {2, 60}, {2, 59}, {2, 58}, {1, 58}, {0, 23}},
            {{0, 6}, {1, 6}, {1, 5}, {1, 4}, {1, 3}, {1, 2}, {1, 1}, {0, 1}, {1, 15}, {1, 14}, {1, 13}, {1, 12}, {1, 11}, {1, 10}, {1, 9}, {1, 8}, {1, 23}, {1, 22}, {1, 21}, {1, 20}, {1, 19}, {1, 18}, {1, 17}, {1, 16}, {1, 31}, {1, 30}, {1, 29}, {1, 28}, {1, 27}, {1, 26}, {1, 25}, {1, 24}, {1, 39}, {1, 38}, {1, 37}, {1, 36}, {1, 35}, {1, 34}, {1, 33}, {1, 32}, {1, 47}, {1, 46}, {1, 45}, {1, 44}, {1, 43}, {1, 42}, {1, 41}, {1, 40}, {1, 55}, {1, 54}, {1, 53}, {1, 52}, {1, 51}, {1, 50}, {1, 49}, {1, 15}, {0, 55}, {1, 62}, {1, 61}, {1, 60}, {1, 59}, {1, 58}, {1, 57}, {0, 15}},
            {{0, 7}, {0, 6}, {0, 5}, {0, 4}, {0, 3}, {0, 2}, {0, 1}, {0, 0}, {0, 15}, {0, 14}, {0, 13}, {0, 12}, {0, 11}, {0, 10}, {0, 9}, {0, 1}, {0, 23}, {0, 22}, {0, 21}, {0, 20}, {0, 19}, {0, 18}, {0, 10}, {0, 2}, {0, 31}, {0, 30}, {0, 29}, {0, 28}, {0, 27}, {0, 19}, {0, 11}, {0, 3}, {0, 39}, {0, 38}, {0, 37}, {0, 36}, {0, 28}, {0, 20}, {0, 12}, {0, 4}, {0, 47}, {0, 46}, {0, 45}, {0, 37}, {0, 29}, {0, 21}, {0, 13}, {0, 5}, {0, 55}, {0, 54}, {0, 46}, {0, 38}, {0, 30}, {0, 22}, {0, 14}, {0, 6}, {0, 63}, {0, 55}, {0, 47}, {0, 39}, {0, 31}, {0, 23}, {0, 15}, {0, 7}},
            {{0, 1}, {1, 8}, {1, 16}, {1, 24}, {1, 32}, {1, 40}, {1, 15}, {0, 15}, {1, 1}, {1, 9}, {1, 17}, {1, 25}, {1, 33}, {1, 41}, {1, 49}, {1, 57}, {1, 2}, {1, 10}, {1, 18}, {1, 26}, {1, 34}, {1, 42}, {1, 50}, {1, 58}, {1, 3}, {1, 11}, {1, 19}, {1, 27}, {1, 35}, {1, 43}, {1, 51}, {1, 59}, {1, 4}, {1, 12}, {1, 20}, {1, 28}, {1, 36}, {1, 44}, {1, 52}, {1, 60}, {1, 5}, {1, 13}, {1, 21}, {1, 29}, {1, 37}, {1, 45}, {1, 53}, {1, 61}, {1, 6}, {1, 14}, {1, 22}, {1, 30}, {1, 38}, {1, 46}, {1, 54}, {1, 62}, {0, 6}, {1, 15}, {1, 23}, {1, 31}, {1, 39}, {1, 47}, {1, 55}, {0, 55}},
            {{0, 9}, {1, 9}, {2, 9}, {3, 9}, {3, 14}, {2, 14}, {1, 14}, {0, 14}, {1, 9}, {9, 9}, {9, 10}, {9, 11}, {9, 12}, {9, 13}, {9, 14}, {1, 49}, {2, 9}, {9, 10}, {9, 18}, {9, 19}, {9, 20}, {9, 21}, {9, 22}, {2, 49}, {3, 9}, {9, 11}, {9, 19}, {9, 27}, {9, 28}, {9, 29}, {9, 30}, {3, 49}, {3, 14}, {9, 12}, {9, 20}, {9, 28}, {9, 36}, {9, 37}, {9, 38}, {3, 54}, {2, 14}, {9, 13}, {9, 21}, {9, 29}, {9, 37}, {9, 45}, {9, 46}, {2, 54}, {1, 14}, {9, 14}, {9, 22}, {9, 30}, {9, 38}, {9, 46}, {9, 54}, {1, 54}, {0, 14}, {1, 49}, {2, 49}, {3, 49}, {3, 54}, {2, 54}, {1, 54}, {0, 54}},
            {{0, 10}, {1, 10}, {2, 10}, {3, 10}, {3, 13}, {2, 13}, {1, 13}, {0, 13}, {1, 17}, {9, 10}, {10, 10}, {10, 11}, {10, 12}, {10, 13}, {9, 13}, {1, 41}, {2, 17}, {10, 17}, {10, 18}, {10, 19}, {10, 20}, {10, 21}, {10, 22}, {2, 41}, {3, 17}, {10, 25}, {10, 26}, {10, 27}, {10, 28}, {10, 29}, {10, 30}, {3, 41}, {3, 22}, {10, 33}, {10, 34}, {10, 35}, {10, 36}, {10, 37}, {10, 38}, {3, 46}, {2, 22}, {10, 22}, {10, 42}, {10, 43}, {10, 44}, {10, 45}, {10, 46}, {2, 46}, {1, 22}, {9, 22}, {10, 50}, {10, 51}, {10, 52}, {10, 53}, {9, 46}, {1, 46}, {0, 22}, {1, 50}, {2, 50}, {3, 50}, {3, 53}, {2, 53}, {1, 53}, {0, 46}},
            {{0, 11}, {1, 11}, {2, 11}, {3, 11}, {3, 12}, {2, 12}, {1, 12}, {0, 12}, {1, 25}, {9, 11}, {10, 11}, {11, 11}, {11, 12}, {10, 12}, {9, 12}, {1, 33}, {2, 25}, {10, 25}, {11, 18}, {11, 19}, {11, 20}, {11, 21}, {10, 33}, {2, 33}, {3, 25}, {11, 25}, {11, 26}, {11, 27}, {11, 28}, {11, 29}, {11, 30}, {3, 33}, {3, 30}, {11, 30}, {11, 34}, {11, 35}, {11, 36}, {11, 37}, {11, 38}, {3, 38}, {2, 30}, {10, 30}, {11, 42}, {11, 43}, {11, 44}, {11, 45}, {10, 38}, {2, 38}, {1, 30}, {9, 30}, {10, 51}, {11, 51}, {11, 52}, {10, 52}, {9, 38}, {1, 38}, {0, 30}, {1, 51}, {2, 51}, {3, 51}, {3, 52}, {2, 52}, {1, 52}, {0, 38}},
            {{0, 12}, {1, 12}, {2, 12}, {3, 12}, {3, 11}, {2, 11}, {1, 11}, {0, 11}, {1, 33}, {9, 12}, {10, 12}, {11, 12}, {11, 11}, {10, 11}, {9, 11}, {1, 25}, {2, 33}, {10, 33}, {11, 21}, {11, 20}, {11, 19}, {11, 18}, {10, 25}, {2, 25}, {3, 33}, {11, 30}, {11, 29}, {11, 28}, {11, 27}, {11, 26}, {11, 25}, {3, 25}, {3, 38}, {11, 38}, {11, 37}, {11, 36}, {11, 35}, {11, 34}, {11, 30}, {3, 30}, {2, 38}, {10, 38}, {11, 45}, {11, 44}, {11, 43}, {11, 42}, {10, 30}, {2, 30}, {1, 38}, {9, 38}, {10, 52}, {11, 52}, {11, 51}, {10, 51}, {9, 30}, {1, 30}, {0, 38}, {1, 52}, {2, 52}, {3, 52}, {3, 51}, {2, 51}, {1, 51}, {0, 30}},
            {{0, 13}, {1, 13}, {2, 13}, {3, 13}, {3, 10}, {2, 10}, {1, 10}, {0, 10}, {1, 41}, {9, 13}, {10, 13}, {10, 12}, {10, 11}, {10, 10}, {9, 10}, {1, 17}, {2, 41}, {10, 22}, {10, 21}, {10, 20}, {10, 19}, {10, 18}, {10, 17}, {2, 17}, {3, 41}, {10, 30}, {10, 29}, {10, 28}, {10, 27}, {10, 26}, {10, 25}, {3, 17}, {3, 46}, {10, 38}, {10, 37}, {10, 36}, {10, 35}, {10, 34}, {10, 33}, {3, 22}, {2, 46}, {10, 46}, {10, 45}, {10, 44}, {10, 43}, {10, 42}, {10, 22}, {2, 22}, {1, 46}, {9, 46}, {10, 53}, {10, 52}, {10, 51}, {10, 50}, {9, 22}, {1, 22}, {0, 46}, {1, 53}, {2, 53}, {3, 53}, {3, 50}, {2, 50}, {1, 50}, {0, 22}},
            {{0, 14}, {1, 14}, {2, 14}, {3, 14}, {3, 9}, {2, 9}, {1, 9}, {0, 9}, {1, 49}, {9, 14}, {9, 13}, {9, 12}, {9, 11}, {9, 10}, {9, 9}, {1, 9}, {2, 49}, {9, 22}, {9, 21}, {9, 20}, {9, 19}, {9, 18}, {9, 10}, {2, 9}, {3, 49}, {9, 30}, {9, 29}, {9, 28}, {9, 27}, {9, 19}, {9, 11}, {3, 9}, {3, 54}, {9, 38}, {9, 37}, {9, 36}, {9, 28}, {9, 20}, {9, 12}, {3, 14}, {2, 54}, {9, 46}, {9, 45}, {9, 37}, {9, 29}, {9, 21}, {9, 13}, {2, 14}, {1, 54}, {9, 54}, {9, 46}, {9, 38}, {9, 30}, {9, 22}, {9, 14}, {1, 14}, {0, 54}, {1, 54}, {2, 54}, {3, 54}, {3, 49}, {2, 49}, {1, 49}, {0, 14}},
            {{0, 15}, {1, 15}, {1, 40}, {1, 32}, {1, 24}, {1, 16}, {1, 8}, {0, 1}, {1, 57}, {1, 49}, {1, 41}, {1, 33}, {1, 25}, {1, 17}, {1, 9}, {1, 1}, {1, 58}, {1, 50}, {1, 42}, {1, 34}, {1, 26}, {1, 18}, {1, 10}, {1, 2}, {1, 59}, {1, 51}, {1, 43}, {1, 35}, {1, 27}, {1, 19}, {1, 11}, {1, 3}, {1, 60}, {1, 52}, {1, 44}, {1, 36}, {1, 28}, {1, 20}, {1, 12}, {1, 4}, {1, 61}, {1, 53}, {1, 45}, {1, 37}, {1, 29}, {1, 21}, {1, 13}, {1, 5}, {1, 62}, {1, 54}, {1, 46}, {1, 38}, {1, 30}, {1, 22}, {1, 14}, {1, 6}, {0, 55}, {1, 55}, {1, 47}, {1, 39}, {1, 31}, {1, 23}, {1, 15}, {0, 6}},
            {{0, 2}, {1, 16}, {2, 16}, {2, 24}, {2, 32}, {2, 23}, {1, 23}, {0, 23}, {1, 2}, {2, 9}, {2, 17}, {2, 25}, {2, 33}, {2, 41}, {2, 49}, {1, 58}, {2, 2}, {2, 10}, {2, 18}, {2, 26}, {2, 34}, {2, 42}, {2, 50}, {2, 58}, {2, 3}, {2, 11}, {2, 19}, {2, 27}, {2, 35}, {2, 43}, {2, 51}, {2, 59}, {2, 4}, {2, 12}, {2, 20}, {2, 28}, {2, 36}, {2, 44}, {2, 52}, {2, 60}, {2, 5}, {2, 13}, {2, 21}, {2, 29}, {2, 37}, {2, 45}, {2, 53}, {2, 61}, {1, 5}, {2, 14}, {2, 22}, {2, 30}, {2, 38}, {2, 46}, {2, 54}, {1, 61}, {0, 5}, {1, 40}, {2, 23}, {2, 31}, {2, 39}, {2, 47}, {1, 47}, {0, 47}},
            {{0, 10}, {1, 17}, {2, 17}, {3, 17}, {3, 22}, {2, 22}, {1, 22}, {0, 22}, {1, 10}, {9, 10}, {10, 17}, {10, 25}, {10, 33}, {10, 22}, {9, 22}, {1, 50}, {2, 10}, {10, 10}, {10, 18}, {10, 26}, {10, 34}, {10, 42}, {10, 50}, {2, 50}, {3, 10}, {10, 11}, {10, 19}, {10, 27}, {10, 35}, {10, 43}, {10, 51}, {3, 50}, {3, 13}, {10, 12}, {10, 20}, {10, 28}, {10, 36}, {10, 44}, {10, 52}, {3, 53}, {2, 13}, {10, 13}, {10, 21}, {10, 29}, {10, 37}, {10, 45}, {10, 53}, {2, 53}, {1, 13}, {9, 13}, {10, 22}, {10, 30}, {10, 38}, {10, 46}, {9, 46}, {1, 53}, {0, 13}, {1, 41}, {2, 41}, {3, 41}, {3, 46}, {2, 46}, {1, 46}, {0, 46}},
            {{0, 18}, {1, 18}, {2, 18}, {3, 18}, {3, 21}, {2, 21}, {1, 21}, {0, 21}, {1, 18}, {9, 18}, {10, 18}, {11, 18}, {11, 21}, {10, 21}, {9, 21}, {1, 42}, {2, 18}, {10, 18}, {18, 18}, {18, 19}, {18, 20}, {18, 21}, {10, 42}, {2, 42}, {3, 18}, {11, 18}, {18, 19}, {18, 27}, {18, 28}, {18, 29}, {11, 42}, {3, 42}, {3, 21}, {11, 21}, {18, 20}, {18, 28}, {18, 36}, {18, 37}, {11, 45}, {3, 45}, {2, 21}, {10, 21}, {18, 21}, {18, 29}, {18, 37}, {18, 45}, {10, 45}, {2, 45}, {1, 21}, {9, 21}, {10, 42}, {11, 42}, {11, 45}, {10, 45}, {9, 45}, {1, 45}, {0, 21}, {1, 42}, {2, 42}, {3, 42}, {3, 45}, {2, 45}, {1, 45}, {0, 45}},
            {{0, 19}, {1, 19}, {2, 19}, {3, 19}, {3, 20}, {2, 20}, {1, 20}, {0, 20}, {1, 26}, {9, 19}, {10, 19}, {11, 19}, {11, 20}, {10, 20}, {9, 20}, {1, 34}, {2, 26}, {10, 26}, {18, 19}, {19, 19}, {19, 20}, {18, 20}, {10, 34}, {2, 34}, {3, 26}, {11, 26}, {19, 26}, {19, 27}, {19, 28}, {19, 29}, {11, 34}, {3, 34}, {3, 29}, {11, 29}, {19, 29}, {19, 35}, {19, 36}, {19, 37}, {11, 37}, {3, 37}, {2, 29}, {10, 29}, {18, 29}, {19, 43}, {19, 44}, {18, 37}, {10, 37}, {2, 37}, {1, 29}, {9, 29}, {10, 43}, {11, 43}, {11, 44}, {10, 44}, {9, 37}, {1, 37}, {0, 29}, {1, 43}, {2, 43}, {3, 43}, {3, 44}, {2, 44}, {1, 44}, {0, 37}},
            {{0, 20}, {1, 20}, {2, 20}, {3, 20}, {3, 19}, {2, 19}, {1, 19}, {0, 19}, {1, 34}, {9, 20}, {10, 20}, {11, 20}, {11, 19}, {10, 19}, {9, 19}, {1, 26}, {2, 34}, {10, 34}, {18, 20}, {19, 20}, {19, 19}, {18, 19}, {10, 26}, {2, 26}, {3, 34}, {11, 34}, {19, 29}, {19, 28}, {19, 27}, {19, 26}, {11, 26}, {3, 26}, {3, 37}, {11, 37}, {19, 37}, {19, 36}, {19, 35}, {19, 29}, {11, 29}, {3, 29}, {2, 37}, {10, 37}, {18, 37}, {19, 44}, {19, 43}, {18, 29}, {10, 29}, {2, 29}, {1, 37}, {9, 37}, {10, 44}, {11, 44}, {11, 43}, {10, 43}, {9, 29}, {1, 29}, {0, 37}, {1, 44}, {2, 44}, {3, 44}, {3, 43}, {2, 43}, {1, 43}, {0, 29}},
            {{0, 21}, {1, 21}, {2, 21}, {3, 21}, {3, 18}, {2, 18}, {1, 18}, {0, 18}, {1, 42}, {9, 21}, {10, 21}, {11, 21}, {11, 18}, {10, 18}, {9, 18}, {1, 18}, {2, 42}, {10, 42}, {18, 21}, {18, 20}, {18, 19}, {18, 18}, {10, 18}, {2, 18}, {3, 42}, {11, 42}, {18, 29}, {18, 28}, {18, 27}, {18, 19}, {11, 18}, {3, 18}, {3, 45}, {11, 45}, {18, 37}, {18, 36}, {18, 28}, {18, 20}, {11, 21}, {3, 21}, {2, 45}, {10, 45}, {18, 45}, {18, 37}, {18, 29}, {18, 21}, {10, 21}, {2, 21}, {1, 45}, {9, 45}, {10, 45}, {11, 45}, {11, 42}, {10, 42}, {9, 21}, {1, 21}, {0, 45}, {1, 45}, {2, 45}, {3, 45}, {3, 42}, {2, 42}, {1, 42}, {0, 21}},
            {{0, 22}, {1, 22}, {2, 22}, {3, 22}, {3, 17}, {2, 17}, {1, 17}, {0, 10}, {1, 50}, {9, 22}, {10, 22}, {10, 33}, {10, 25}, {10, 17}, {9, 10}, {1, 10}, {2, 50}, {10, 50}, {10, 42}, {10, 34}, {10, 26}, {10, 18}, {10, 10}, {2, 10}, {3, 50}, {10, 51}, {10, 43}, {10, 35}, {10, 27}, {10, 19}, {10, 11}, {3, 10}, {3, 53}, {10, 52}, {10, 44}, {10, 36}, {10, 28}, {10, 20}, {10, 12}, {3, 13}, {2, 53}, {10, 53}, {10, 45}, {10, 37}, {10, 29}, {10, 21}, {10, 13}, {2, 13}, {1, 53}, {9, 46}, {10, 46}, {10, 38}, {10, 30}, {10, 22}, {9, 13}, {1, 13}, {0, 46}, {1, 46}, {2, 46}, {3, 46}, {3, 41}, {2, 41}, {1, 41}, {0, 13}},
            {{0, 23}, {1, 23}, {2, 23}, {2, 32}, {2, 24}, {2, 16}, {1, 16}, {0, 2}, {1, 58}, {2, 49}, {2, 41}, {2, 33}, {2, 25}, {2, 17}, {2, 9}, {1, 2}, {2, 58}, {2, 50}, {2, 42}, {2, 34}, {2, 26}, {2, 18}, {2, 10}, {2, 2}, {2, 59}, {2, 51}, {2, 43}, {2, 35}, {2, 27}, {2, 19}, {2, 11}, {2, 3}, {2, 60}, {2, 52}, {2, 44}, {2, 36}, {2, 28}, {2, 20}, {2, 12}, {2, 4}, {2, 61}, {2, 53}, {2, 45}, {2, 37}, {2, 29}, {2, 21}, {2, 13}, {2, 5}, {1, 61}, {2, 54}, {2, 46}, {2, 38}, {2, 30}, {2, 22}, {2, 14}, {1, 5}, {0, 47}, {1, 47}, {2, 47}, {2, 39}, {2, 31}, {2, 23}, {1, 40}, {0, 5}},
            {{0, 3}, {1, 24}, {2, 24}, {3, 24}, {3, 31}, {2, 31}, {1, 31}, {0, 31}, {1, 3}, {3, 9}, {3, 17}, {3, 25}, {3, 33}, {3, 41}, {3, 49}, {1, 59}, {2, 3}, {3, 10}, {3, 18}, {3, 26}, {3, 34}, {3, 42}, {3, 50}, {2, 59}, {3, 3}, {3, 11}, {3, 19}, {3, 27}, {3, 35}, {3, 43}, {3, 51}, {3, 59}, {3, 4}, {3, 12}, {3, 20}, {3, 28}, {3, 36}, {3, 44}, {3, 52}, {3, 60}, {2, 4}, {3, 13}, {3, 21}, {3, 29}, {3, 37}, {3, 45}, {3, 53}, {2, 60}, {1, 4}, {3, 14}, {3, 22}, {3, 30}, {3, 38}, {3, 46}, {3, 54}, {1, 60}, {0, 4}, {1, 32}, {2, 32}, {3, 31}, {3, 39}, {2, 39}, {1, 39}, {0, 39}},
            {{0, 11}, {1, 25}, {2, 25}, {3, 25}, {3, 30}, {2, 30}, {1, 30}, {0, 30}, {1, 11}, {9, 11}, {10, 25}, {11, 25}, {11, 30}, {10, 30}, {9, 30}, {1, 51}, {2, 11}, {10, 11}, {11, 18}, {11, 26}, {11, 34}, {11, 42}, {10, 51}, {2, 51}, {3, 11}, {11, 11}, {11, 19}, {11, 27}, {11, 35}, {11, 43}, {11, 51}, {3, 51}, {3, 12}, {11, 12}, {11, 20}, {11, 28}, {11, 36}, {11, 44}, {11, 52}, {3, 52}, {2, 12}, {10, 12}, {11, 21}, {11, 29}, {11, 37}, {11, 45}, {10, 52}, {2, 52}, {1, 12}, {9, 12}, {10, 33}, {11, 30}, {11, 38}, {10, 38}, {9, 38}, {1, 52}, {0, 12}, {1, 33}, {2, 33}, {3, 33}, {3, 38}, {2, 38}, {1, 38}, {0, 38}},
            {{0, 19}, {1, 26}, {2, 26}, {3, 26}, {3, 29}, {2, 29}, {1, 29}, {0, 29}, {1, 19}, {9, 19}, {10, 26}, {11, 26}, {11, 29}, {10, 29}, {9, 29}, {1, 43}, {2, 19}, {10, 19}, {18, 19}, {19, 26}, {19, 29}, {18, 29}, {10, 43}, {2, 43}, {3, 19}, {11, 19}, {19, 19}, {19, 27}, {19, 35}, {19, 43}, {11, 43}, {3, 43}, {3, 20}, {11, 20}, {19, 20}, {19, 28}, {19, 36}, {19, 44}, {11, 44}, {3, 44}, {2, 20}, {10, 20}, {18, 20}, {19, 29}, {19, 37}, {18, 37}, {10, 44}, {2, 44}, {1, 20}, {9, 20}, {10, 34}, {11, 34}, {11, 37}, {10, 37}, {9, 37}, {1, 44}, {0, 20}, {1, 34}, {2, 34}, {3, 34}, {3, 37}, {2, 37}, {1, 37}, {0, 37}},
            {{0, 27}, {1, 27}, {2, 27}, {3, 27}, {3, 28}, {2, 28}, {1, 28}, {0, 28}, {1, 27}, {9, 27}, {10, 27}, {11, 27}, {11, 28}, {10, 28}, {9, 28}, {1, 35}, {2, 27}, {10, 27}, {18, 27}, {19, 27}, {19, 28}, {18, 28}, {10, 35}, {2, 35}, {3, 27}, {11, 27}, {19, 27}, {27, 27}, {27, 28}, {19, 35}, {11, 35}, {3, 35}, {3, 28}, {11, 28}, {19, 28}, {27, 28}, {27, 36}, {19, 36}, {11, 36}, {3, 36}, {2, 28}, {10, 28}, {18, 28}, {19, 35}, {19, 36}, {18, 36}, {10, 36}, {2, 36}, {1, 28}, {9, 28}, {10, 35}, {11, 35}, {11, 36}, {10, 36}, {9, 36}, {1, 36}, {0, 28}, {1, 35}, {2, 35}, {3, 35}, {3, 36}, {2, 36}, {1, 36}, {0, 36}},
            {{0, 28}, {1, 28}, {2, 28}, {3, 28}, {3, 27}, {2, 27}, {1, 27}, {0, 27}, {1, 35}, {9, 28}, {10, 28}, {11, 28}, {11, 27}, {10, 27}, {9, 27}, {1, 27}, {2, 35}, {10, 35}, {18, 28}, {19, 28}, {19, 27}, {18, 27}, {10, 27}, {2, 27}, {3, 35}, {11, 35}, {19, 35}, {27, 28}, {27, 27}, {19, 27}, {11, 27}, {3, 27}, {3, 36}, {11, 36}, {19, 36}, {27, 36}, {27, 28}, {19, 28}, {11, 28}, {3, 28}, {2, 36}, {10, 36}, {18, 36}, {19, 36}, {19, 35}, {18, 28}, {10, 28}, {2, 28}, {1, 36}, {9, 36}, {10, 36}, {11, 36}, {11, 35}, {10, 35}, {9, 28}, {1, 28}, {0, 36}, {1, 36}, {2, 36}, {3, 36}, {3, 35}, {2, 35}, {1, 35}, {0, 28}},
            {{0, 29}, {1, 29}, {2, 29}, {3, 29}, {3, 26}, {2, 26}, {1, 26}, {0, 19}, {1, 43}, {9, 29}, {10, 29}, {11, 29}, {11, 26}, {10, 26}, {9, 19}, {1, 19}, {2, 43}, {10, 43}, {18, 29}, {19, 29}, {19, 26}, {18, 19}, {10, 19}, {2, 19}, {3, 43}, {11, 43}, {19, 43}, {19, 35}, {19, 27}, {19, 19}, {11, 19}, {3, 19}, {3, 44}, {11, 44}, {19, 44}, {19, 36}, {19, 28}, {19, 20}, {11, 20}, {3, 20}, {2, 44}, {10, 44}, {18, 37}, {19, 37}, {19, 29}, {18, 20}, {10, 20}, {2, 20}, {1, 44}, {9, 37}, {10, 37}, {11, 37}, {11, 34}, {10, 34}, {9, 20}, {1, 20}, {0, 37}, {1, 37}, {2, 37}, {3, 37}, {3, 34}, {2, 34}, {1, 34}, {0, 20}},
            {{0, 30}, {1, 30}, {2, 30}, {3, 30}, {3, 25}, {2, 25}, {1, 25}, {0, 11}, {1, 51}, {9, 30}, {10, 30}, {11, 30}, {11, 25}, {10, 25}, {9, 11}, {1, 11}, {2, 51}, {10, 51}, {11, 42}, {11, 34}, {11, 26}, {11, 18}, {10, 11}, {2, 11}, {3, 51}, {11, 51}, {11, 43}, {11, 35}, {11, 27}, {11, 19}, {11, 11}, {3, 11}, {3, 52}, {11, 52}, {11, 44}, {11, 36}, {11, 28}, {11, 20}, {11, 12}, {3, 12}, {2, 52}, {10, 52}, {11, 45}, {11, 37}, {11, 29}, {11, 21}, {10, 12}, {2, 12}, {1, 52}, {9, 38}, {10, 38}, {11, 38}, {11, 30}, {10, 33}, {9, 12}, {1, 12}, {0, 38}, {1, 38}, {2, 38}, {3, 38}, {3, 33}, {2, 33}, {1, 33}, {0, 12}},
            {{0, 31}, {1, 31}, {2, 31}, {3, 31}, {3, 24}, {2, 24}, {1, 24}, {0, 3}, {1, 59}, {3, 49}, {3, 41}, {3, 33}, {3, 25}, {3, 17}, {3, 9}, {1, 3}, {2, 59}, {3, 50}, {3, 42}, {3, 34}, {3, 26}, {3, 18}, {3, 10}, {2, 3}, {3, 59}, {3, 51}, {3, 43}, {3, 35}, {3, 27}, {3, 19}, {3, 11}, {3, 3}, {3, 60}, {3, 52}, {3, 44}, {3, 36}, {3, 28}, {3, 20}, {3, 12}, {3, 4}, {2, 60}, {3, 53}, {3, 45}, {3, 37}, {3, 29}, {3, 21}, {3, 13}, {2, 4}, {1, 60}, {3, 54}, {3, 46}, {3, 38}, {3, 30}, {3, 22}, {3, 14}, {1, 4}, {0, 39}, {1, 39}, {2, 39}, {3, 39}, {3, 31}, {2, 32}, {1, 32}, {0, 4}},
            {{0, 4}, {1, 32}, {2, 32}, {3, 31}, {3, 39}, {2, 39}, {1, 39}, {0, 39}, {1, 4}, {3, 14}, {3, 22}, {3, 30}, {3, 38}, {3, 46}, {3, 54}, {1, 60}, {2, 4}, {3, 13}, {3, 21}, {3, 29}, {3, 37}, {3, 45}, {3, 53}, {2, 60}, {3, 4}, {3, 12}, {3, 20}, {3, 28}, {3, 36}, {3, 44}, {3, 52}, {3, 60}, {3, 3}, {3, 11}, {3, 19}, {3, 27}, {3, 35}, {3, 43}, {3, 51}, {3, 59}, {2, 3}, {3, 10}, {3, 18}, {3, 26}, {3, 34}, {3, 42}, {3, 50}, {2, 59}, {1, 3}, {3, 9}, {3, 17}, {3, 25}, {3, 33}, {3, 41}, {3, 49}, {1, 59}, {0, 3}, {1, 24}, {2, 24}, {3, 24}, {3, 31}, {2, 31}, {1, 31}, {0, 31}},
            {{0, 12}, {1, 33}, {2, 33}, {3, 33}, {3, 38}, {2, 38}, {1, 38}, {0, 38}, {1, 12}, {9, 12}, {10, 33}, {11, 30}, {11, 38}, {10, 38}, {9, 38}, {1, 52}, {2, 12}, {10, 12}, {11, 21}, {11, 29}, {11, 37}, {11, 45}, {10, 52}, {2, 52}, {3, 12}, {11, 12}, {11, 20}, {11, 28}, {11, 36}, {11, 44}, {11, 52}, {3, 52}, {3, 11}, {11, 11}, {11, 19}, {11, 27}, {11, 35}, {11, 43}, {11, 51}, {3, 51}, {2, 11}, {10, 11}, {11, 18}, {11, 26}, {11, 34}, {11, 42}, {10, 51}, {2, 51}, {1, 11}, {9, 11}, {10, 25}, {11, 25}, {11, 30}, {10, 30}, {9, 30}, {1, 51}, {0, 11}, {1, 25}, {2, 25}, {3, 25}, {3, 30}, {2, 30}, {1, 30}, {0, 30}},
            {{0, 20}, {1, 34}, {2, 34}, {3, 34}, {3, 37}, {2, 37}, {1, 37}, {0, 37}, {1, 20}, {9, 20}, {10, 34}, {11, 34}, {11, 37}, {10, 37}, {9, 37}, {1, 44}, {2, 20}, {10, 20}, {18, 20}, {19, 29}, {19, 37}, {18, 37}, {10, 44}, {2, 44}, {3, 20}, {11, 20}, {19, 20}, {19, 28}, {19, 36}, {19, 44}, {11, 44}, {3, 44}, {3, 19}, {11, 19}, {19, 19}, {19, 27}, {19, 35}, {19, 43}, {11, 43}, {3, 43}, {2, 19}, {10, 19}, {18, 19}, {19, 26}, {19, 29}, {18, 29}, {10, 43}, {2, 43}, {1, 19}, {9, 19}, {10, 26}, {11, 26}, {11, 29}, {10, 29}, {9, 29}, {1, 43}, {0, 19}, {1, 26}, {2, 26}, {3, 26}, {3, 29}, {2, 29}, {1, 29}, {0, 29}},
            {{0, 28}, {1, 35}, {2, 35}, {3, 35}, {3, 36}, {2, 36}, {1, 36}, {0, 36}, {1, 28}, {9, 28}, {10, 35}, {11, 35}, {11, 36}, {10, 36}, {9, 36}, {1, 36}, {2, 28}, {10, 28}, {18, 28}, {19, 35}, {19, 36}, {18, 36}, {10, 36}, {2, 36}, {3, 28}, {11, 28}, {19, 28}, {27, 28}, {27, 36}, {19, 36}, {11, 36}, {3, 36}, {3, 27}, {11, 27}, {19, 27}, {27, 27}, {27, 28}, {19, 35}, {11, 35}, {3, 35}, {2, 27}, {10, 27}, {18, 27}, {19, 27}, {19, 28}, {18, 28}, {10, 35}, {2, 35}, {1, 27}, {9, 27}, {10, 27}, {11, 27}, {11, 28}, {10, 28}, {9, 28}, {1, 35}, {0, 27}, {1, 27}, {2, 27}, {3, 27}, {3, 28}, {2, 28}, {1, 28}, {0, 28}},
            {{0, 36}, {1, 36}, {2, 36}, {3, 36}, {3, 35}, {2, 35}, {1, 35}, {0, 28}, {1, 36}, {9, 36}, {10, 36}, {11, 36}, {11, 35}, {10, 35}, {9, 28}, {1, 28}, {2, 36}, {10, 36}, {18, 36}, {19, 36}, {19, 35}, {18, 28}, {10, 28}, {2, 28}, {3, 36}, {11, 36}, {19, 36}, {27, 36}, {27, 28}, {19, 28}, {11, 28}, {3, 28}, {3, 35}, {11, 35}, {19, 35}, {27, 28}, {27, 27}, {19, 27}, {11, 27}, {3, 27}, {2, 35}, {10, 35}, {18, 28}, {19, 28}, {19, 27}, {18, 27}, {10, 27}, {2, 27}, {1, 35}, {9, 28}, {10, 28}, {11, 28}, {11, 27}, {10, 27}, {9, 27}, {1, 27}, {0, 28}, {1, 28}, {2, 28}, {3, 28}, {3, 27}, {2, 27}, {1, 27}, {0, 27}},
            {{0, 37}, {1, 37}, {2, 37}, {3, 37}, {3, 34}, {2, 34}, {1, 34}, {0, 20}, {1, 44}, {9, 37}, {10, 37}, {11, 37}, {11, 34}, {10, 34}, {9, 20}, {1, 20}, {2, 44}, {10, 44}, {18, 37}, {19, 37}, {19, 29}, {18, 20}, {10, 20}, {2, 20}, {3, 44}, {11, 44}, {19, 44}, {19, 36}, {19, 28}, {19, 20}, {11, 20}, {3, 20}, {3, 43}, {11, 43}, {19, 43}, {19, 35}, {19, 27}, {19, 19}, {11, 19}, {3, 19}, {2, 43}, {10, 43}, {18, 29}, {19, 29}, {19, 26}, {18, 19}, {10, 19}, {2, 19}, {1, 43}, {9, 29}, {10, 29}, {11, 29}, {11, 26}, {10, 26}, {9, 19}, {1, 19}, {0, 29}, {1, 29}, {2, 29}, {3, 29}, {3, 26}, {2, 26}, {1, 26}, {0, 19}},
            {{0, 38}, {1, 38}, {2, 38}, {3, 38}, {3, 33}, {2, 33}, {1, 33}, {0, 12}, {1, 52}, {9, 38}, {10, 38}, {11, 38}, {11, 30}, {10, 33}, {9, 12}, {1, 12}, {2, 52}, {10, 52}, {11, 45}, {11, 37}, {11, 29}, {11, 21}, {10, 12}, {2, 12}, {3, 52}, {11, 52}, {11, 44}, {11, 36}, {11, 28}, {11, 20}, {11, 12}, {3, 12}, {3, 51}, {11, 51}, {11, 43}, {11, 35}, {11, 27}, {11, 19}, {11, 11}, {3, 11}, {2, 51}, {10, 51}, {11, 42}, {11, 34}, {11, 26}, {11, 18}, {10, 11}, {2, 11}, {1, 51}, {9, 30}, {10, 30}, {11, 30}, {11, 25}, {10, 25}, {9, 11}, {1, 11}, {0, 30}, {1, 30}, {2, 30}, {3, 30}, {3, 25}, {2, 25}, {1, 25}, {0, 11}},
            {{0, 39}, {1, 39}, {2, 39}, {3, 39}, {3, 31}, {2, 32}, {1, 32}, {0, 4}, {1, 60}, {3, 54}, {3, 46}, {3, 38}, {3, 30}, {3, 22}, {3, 14}, {1, 4}, {2, 60}, {3, 53}, {3, 45}, {3, 37}, {3, 29}, {3, 21}, {3, 13}, {2, 4}, {3, 60}, {3, 52}, {3, 44}, {3, 36}, {3, 28}, {3, 20}, {3, 12}, {3, 4}, {3, 59}, {3, 51}, {3, 43}, {3, 35}, {3, 27}, {3, 19}, {3, 11}, {3, 3}, {2, 59}, {3, 50}, {3, 42}, {3, 34}, {3, 26}, {3, 18}, {3, 10}, {2, 3}, {1, 59}, {3, 49}, {3, 41}, {3, 33}, {3, 25}, {3, 17}, {3, 9}, {1, 3}, {0, 31}, {1, 31}, {2, 31}, {3, 31}, {3, 24}, {2, 24}, {1, 24}, {0, 3}},
            {{0, 5}, {1, 40}, {2, 23}, {2, 31}, {2, 39}, {2, 47}, {1, 47}, {0, 47}, {1, 5}, {2, 14}, {2, 22}, {2, 30}, {2, 38}, {2, 46}, {2, 54}, {1, 61}, {2, 5}, {2, 13}, {2, 21}, {2, 29}, {2, 37}, {2, 45}, {2, 53}, {2, 61}, {2, 4}, {2, 12}, {2, 20}, {2, 28}, {2, 36}, {2, 44}, {2, 52}, {2, 60}, {2, 3}, {2, 11}, {2, 19}, {2, 27}, {2, 35}, {2, 43}, {2, 51}, {2, 59}, {2, 2}, {2, 10}, {2, 18}, {2, 26}, {2, 34}, {2, 42}, {2, 50}, {2, 58}, {1, 2}, {2, 9}, {2, 17}, {2, 25}, {2, 33}, {2, 41}, {2, 49}, {1, 58}, {0, 2}, {1, 16}, {2, 16}, {2, 24}, {2, 32}, {2, 23}, {1, 23}, {0, 23}},
            {{0, 13}, {1, 41}, {2, 41}, {3, 41}, {3, 46}, {2, 46}, {1, 46}, {0, 46}, {1, 13}, {9, 13}, {10, 22}, {10, 30}, {10, 38}, {10, 46}, {9, 46}, {1, 53}, {2, 13}, {10, 13}, {10, 21}, {10, 29}, {10, 37}, {10, 45}, {10, 53}, {2, 53}, {3, 13}, {10, 12}, {10, 20}, {10, 28}, {10, 36}, {10, 44}, {10, 52}, {3, 53}, {3, 10}, {10, 11}, {10, 19}, {10, 27}, {10, 35}, {10, 43}, {10, 51}, {3, 50}, {2, 10}, {10, 10}, {10, 18}, {10, 26}, {10, 34}, {10, 42}, {10, 50}, {2, 50}, {1, 10}, {9, 10}, {10, 17}, {10, 25}, {10, 33}, {10, 22}, {9, 22}, {1, 50}, {0, 10}, {1, 17}, {2, 17}, {3, 17}, {3, 22}, {2, 22}, {1, 22}, {0, 22}},
            {{0, 21}, {1, 42}, {2, 42}, {3, 42}, {3, 45}, {2, 45}, {1, 45}, {0, 45}, {1, 21}, {9, 21}, {10, 42}, {11, 42}, {11, 45}, {10, 45}, {9, 45}, {1, 45}, {2, 21}, {10, 21}, {18, 21}, {18, 29}, {18, 37}, {18, 45}, {10, 45}, {2, 45}, {3, 21}, {11, 21}, {18, 20}, {18, 28}, {18, 36}, {18, 37}, {11, 45}, {3, 45}, {3, 18}, {11, 18}, {18, 19}, {18, 27}, {18, 28}, {18, 29}, {11, 42}, {3, 42}, {2, 18}, {10, 18}, {18, 18}, {18, 19}, {18, 20}, {18, 21}, {10, 42}, {2, 42}, {1, 18}, {9, 18}, {10, 18}, {11, 18}, {11, 21}, {10, 21}, {9, 21}, {1, 42}, {0, 18}, {1, 18}, {2, 18}, {3, 18}, {3, 21}, {2, 21}, {1, 21}, {0, 21}},
            {{0, 29}, {1, 43}, {2, 43}, {3, 43}, {3, 44}, {2, 44}, {1, 44}, {0, 37}, {1, 29}, {9, 29}, {10, 43}, {11, 43}, {11, 44}, {10, 44}, {9, 37}, {1, 37}, {2, 29}, {10, 29}, {18, 29}, {19, 43}, {19, 44}, {18, 37}, {10, 37}, {2, 37}, {3, 29}, {11, 29}, {19, 29}, {19, 35}, {19, 36}, {19, 37}, {11, 37}, {3, 37}, {3, 26}, {11, 26}, {19, 26}, {19, 27}, {19, 28}, {19, 29}, {11, 34}, {3, 34}, {2, 26}, {10, 26}, {18, 19}, {19, 19}, {19, 20}, {18, 20}, {10, 34}, {2, 34}, {1, 26}, {9, 19}, {10, 19}, {11, 19}, {11, 20}, {10, 20}, {9, 20}, {1, 34}, {0, 19}, {1, 19}, {2, 19}, {3, 19}, {3, 20}, {2, 20}, {1, 20}, {0, 20}},
            {{0, 37}, {1, 44}, {2, 44}, {3, 44}, {3, 43}, {2, 43}, {1, 43}, {0, 29}, {1, 37}, {9, 37}, {10, 44}, {11, 44}, {11, 43}, {10, 43}, {9, 29}, {1, 29}, {2, 37}, {10, 37}, {18, 37}, {19, 44}, {19, 43}, {18, 29}, {10, 29}, {2, 29}, {3, 37}, {11, 37}, {19, 37}, {19, 36}, {19, 35}, {19, 29}, {11, 29}, {3, 29}, {3, 34}, {11, 34}, {19, 29}, {19, 28}, {19, 27}, {19, 26}, {11, 26}, {3, 26}, {2, 34}, {10, 34}, {18, 20}, {19, 20}, {19, 19}, {18, 19}, {10, 26}, {2, 26}, {1, 34}, {9, 20}, {10, 20}, {11, 20}, {11, 19}, {10, 19}, {9, 19}, {1, 26}, {0, 20}, {1, 20}, {2, 20}, {3, 20}, {3, 19}, {2, 19}, {1, 19}, {0, 19}},
            {{0, 45}, {1, 45}, {2, 45}, {3, 45}, {3, 42}, {2, 42}, {1, 42}, {0, 21}, {1, 45}, {9, 45}, {10, 45}, {11, 45}, {11, 42}, {10, 42}, {9, 21}, {1, 21}, {2, 45}, {10, 45}, {18, 45}, {18, 37}, {18, 29}, {18, 21}, {10, 21}, {2, 21}, {3, 45}, {11, 45}, {18, 37}, {18, 36}, {18, 28}, {18, 20}, {11, 21}, {3, 21}, {3, 42}, {11, 42}, {18, 29}, {18, 28}, {18, 27}, {18, 19}, {11, 18}, {3, 18}, {2, 42}, {10, 42}, {18, 21}, {18, 20}, {18, 19}, {18, 18}, {10, 18}, {2, 18}, {1, 42}, {9, 21}, {10, 21}, {11, 21}, {11, 18}, {10, 18}, {9, 18}, {1, 18}, {0, 21}, {1, 21}, {2, 21}, {3, 21}, {3, 18}, {2, 18}, {1, 18}, {0, 18}},
            {{0, 46}, {1, 46}, {2, 46}, {3, 46}, {3, 41}, {2, 41}, {1, 41}, {0, 13}, {1, 53}, {9, 46}, {10, 46}, {10, 38}, {10, 30}, {10, 22}, {9, 13}, {1, 13}, {2, 53}, {10, 53}, {10, 45}, {10, 37}, {10, 29}, {10, 21}, {10, 13}, {2, 13}, {3, 53}, {10, 52}, {10, 44}, {10, 36}, {10, 28}, {10, 20}, {10, 12}, {3, 13}, {3, 50}, {10, 51}, {10, 43}, {10, 35}, {10, 27}, {10, 19}, {10, 11}, {3, 10}, {2, 50}, {10, 50}, {10, 42}, {10, 34}, {10, 26}, {10, 18}, {10, 10}, {2, 10}, {1, 50}, {9, 22}, {10, 22}, {10, 33}, {10, 25}, {10, 17}, {9, 10}, {1, 10}, {0, 22}, {1, 22}, {2, 22}, {3, 22}, {3, 17}, {2, 17}, {1, 17}, {0, 10}},
            {{0, 47}, {1, 47}, {2, 47}, {2, 39}, {2, 31}, {2, 23}, {1, 40}, {0, 5}, {1, 61}, {2, 54}, {2, 46}, {2, 38}, {2, 30}, {2, 22}, {2, 14}, {1, 5}, {2, 61}, {2, 53}, {2, 45}, {2, 37}, {2, 29}, {2, 21}, {2, 13}, {2, 5}, {2, 60}, {2, 52}, {2, 44}, {2, 36}, {2, 28}, {2, 20}, {2, 12}, {2, 4}, {2, 59}, {2, 51}, {2, 43}, {2, 35}, {2, 27}, {2, 19}, {2, 11}, {2, 3}, {2, 58}, {2, 50}, {2, 42}, {2, 34}, {2, 26}, {2, 18}, {2, 10}, {2, 2}, {1, 58}, {2, 49}, {2, 41}, {2, 33}, {2, 25}, {2, 17}, {2, 9}, {1, 2}, {0, 23}, {1, 23}, {2, 23}, {2, 32}, {2, 24}, {2, 16}, {1, 16}, {0, 2}},
            {{0, 6}, {1, 15}, {1, 23}, {1, 31}, {1, 39}, {1, 47}, {1, 55}, {0, 55}, {1, 6}, {1, 14}, {1, 22}, {1, 30}, {1, 38}, {1, 46}, {1, 54}, {1, 62}, {1, 5}, {1, 13}, {1, 21}, {1, 29}, {1, 37}, {1, 45}, {1, 53}, {1, 61}, {1, 4}, {1, 12}, {1, 20}, {1, 28}, {1, 36}, {1, 44}, {1, 52}, {1, 60}, {1, 3}, {1, 11}, {1, 19}, {1, 27}, {1, 35}, {1, 43}, {1, 51}, {1, 59}, {1, 2}, {1, 10}, {1, 18}, {1, 26}, {1, 34}, {1, 42}, {1, 50}, {1, 58}, {1, 1}, {1, 9}, {1, 17}, {1, 25}, {1, 33}, {1, 41}, {1, 49}, {1, 57}, {0, 1}, {1, 8}, {1, 16}, {1, 24}, {1, 32}, {1, 40}, {1, 15}, {0, 15}},
            {{0, 14}, {1, 49}, {2, 49}, {3, 49}, {3, 54}, {2, 54}, {1, 54}, {0, 54}, {1, 14}, {9, 14}, {9, 22}, {9, 30}, {9, 38}, {9, 46}, {9, 54}, {1, 54}, {2, 14}, {9, 13}, {9, 21}, {9, 29}, {9, 37}, {9, 45}, {9, 46}, {2, 54}, {3, 14}, {9, 12}, {9, 20}, {9, 28}, {9, 36}, {9, 37}, {9, 38}, {3, 54}, {3, 9}, {9, 11}, {9, 19}, {9, 27}, {9, 28}, {9, 29}, {9, 30}, {3, 49}, {2, 9}, {9, 10}, {9, 18}, {9, 19}, {9, 20}, {9, 21}, {9, 22}, {2, 49}, {1, 9}, {9, 9}, {9, 10}, {9, 11}, {9, 12}, {9, 13}, {9, 14}, {1, 49}, {0, 9}, {1, 9}, {2, 9}, {3, 9}, {3, 14}, {2, 14}, {1, 14}, {0, 14}},
            {{0, 22}, {1, 50}, {2, 50}, {3, 50}, {3, 53}, {2, 53}, {1, 53}, {0, 46}, {1, 22}, {9, 22}, {10, 50}, {10, 51}, {10, 52}, {10, 53}, {9, 46}, {1, 46}, {2, 22}, {10, 22}, {10, 42}, {10, 43}, {10, 44}, {10, 45}, {10, 46}, {2, 46}, {3, 22}, {10, 33}, {10, 34}, {10, 35}, {10, 36}, {10, 37}, {10, 38}, {3, 46}, {3, 17}, {10, 25}, {10, 26}, {10, 27}, {10, 28}, {10, 29}, {10, 30}, {3, 41}, {2, 17}, {10, 17}, {10, 18}, {10, 19}, {10, 20}, {10, 21}, {10, 22}, {2, 41}, {1, 17}, {9, 10}, {10, 10}, {10, 11}, {10, 12}, {10, 13}, {9, 13}, {1, 41}, {0, 10}, {1, 10}, {2, 10}, {3, 10}, {3, 13}, {2, 13}, {1, 13}, {0, 13}},
            {{0, 30}, {1, 51}, {2, 51}, {3, 51}, {3, 52}, {2, 52}, {1, 52}, {0, 38}, {1, 30}, {9, 30}, {10, 51}, {11, 51}, {11, 52}, {10, 52}, {9, 38}, {1, 38}, {2, 30}, {10, 30}, {11, 42}, {11, 43}, {11, 44}, {11, 45}, {10, 38}, {2, 38}, {3, 30}, {11, 30}, {11, 34}, {11, 35}, {11, 36}, {11, 37}, {11, 38}, {3, 38}, {3, 25}, {11, 25}, {11, 26}, {11, 27}, {11, 28}, {11, 29}, {11, 30}, {3, 33}, {2, 25}, {10, 25}, {11, 18}, {11, 19}, {11, 20}, {11, 21}, {10, 33}, {2, 33}, {1, 25}, {9, 11}, {10, 11}, {11, 11}, {11, 12}, {10, 12}, {9, 12}, {1, 33}, {0, 11}, {1, 11}, {2, 11}, {3, 11}, {3, 12}, {2, 12}, {1, 12}, {0, 12}},
            {{0, 38}, {1, 52}, {2, 52}, {3, 52}, {3, 51}, {2, 51}, {1, 51}, {0, 30}, {1, 38}, {9, 38}, {10, 52}, {11, 52}, {11, 51}, {10, 51}, {9, 30}, {1, 30}, {2, 38}, {10, 38}, {11, 45}, {11, 44}, {11, 43}, {11, 42}, {10, 30}, {2, 30}, {3, 38}, {11, 38}, {11, 37}, {11, 36}, {11, 35}, {11, 34}, {11, 30}, {3, 30}, {3, 33}, {11, 30}, {11, 29}, {11, 28}, {11, 27}, {11, 26}, {11, 25}, {3, 25}, {2, 33}, {10, 33}, {11, 21}, {11, 20}, {11, 19}, {11, 18}, {10, 25}, {2, 25}, {1, 33}, {9, 12}, {10, 12}, {11, 12}, {11, 11}, {10, 11}, {9, 11}, {1, 25}, {0, 12}, {1, 12}, {2, 12}, {3, 12}, {3, 11}, {2, 11}, {1, 11}, {0, 11}},
            {{0, 46}, {1, 53}, {2, 53}, {3, 53}, {3, 50}, {2, 50}, {1, 50}, {0, 22}, {1, 46}, {9, 46}, {10, 53}, {10, 52}, {10, 51}, {10, 50}, {9, 22}, {1, 22}, {2, 46}, {10, 46}, {10, 45}, {10, 44}, {10, 43}, {10, 42}, {10, 22}, {2, 22}, {3, 46}, {10, 38}, {10, 37}, {10, 36}, {10, 35}, {10, 34}, {10, 33}, {3, 22}, {3, 41}, {10, 30}, {10, 29}, {10, 28}, {10, 27}, {10, 26}, {10, 25}, {3, 17}, {2, 41}, {10, 22}, {10, 21}, {10, 20}, {10, 19}, {10, 18}, {10, 17}, {2, 17}, {1, 41}, {9, 13}, {10, 13}, {10, 12}, {10, 11}, {10, 10}, {9, 10}, {1, 17}, {0, 13}, {1, 13}, {2, 13}, {3, 13}, {3, 10}, {2, 10}, {1, 10}, {0, 10}},
            {{0, 54}, {1, 54}, {2, 54}, {3, 54}, {3, 49}, {2, 49}, {1, 49}, {0, 14}, {1, 54}, {9, 54}, {9, 46}, {9, 38}, {9, 30}, {9, 22}, {9, 14}, {1, 14}, {2, 54}, {9, 46}, {9, 45}, {9, 37}, {9, 29}, {9, 21}, {9, 13}, {2, 14}, {3, 54}, {9, 38}, {9, 37}, {9, 36}, {9, 28}, {9, 20}, {9, 12}, {3, 14}, {3, 49}, {9, 30}, {9, 29}, {9, 28}, {9, 27}, {9, 19}, {9, 11}, {3, 9}, {2, 49}, {9, 22}, {9, 21}, {9, 20}, {9, 19}, {9, 18}, {9, 10}, {2, 9}, {1, 49}, {9, 14}, {9, 13}, {9, 12}, {9, 11}, {9, 10}, {9, 9}, {1, 9}, {0, 14}, {1, 14}, {2, 14}, {3, 14}, {3, 9}, {2, 9}, {1, 9}, {0, 9}},
            {{0, 55}, {1, 55}, {1, 47}, {1, 39}, {1, 31}, {1, 23}, {1, 15}, {0, 6}, {1, 62}, {1, 54}, {1, 46}, {1, 38}, {1, 30}, {1, 22}, {1, 14}, {1, 6}, {1, 61}, {1, 53}, {1, 45}, {1, 37}, {1, 29}, {1, 21}, {1, 13}, {1, 5}, {1, 60}, {1, 52}, {1, 44}, {1, 36}, {1, 28}, {1, 20}, {1, 12}, {1, 4}, {1, 59}, {1, 51}, {1, 43}, {1, 35}, {1, 27}, {1, 19}, {1, 11}, {1, 3}, {1, 58}, {1, 50}, {1, 42}, {1, 34}, {1, 26}, {1, 18}, {1, 10}, {1, 2}, {1, 57}, {1, 49}, {1, 41}, {1, 33}, {1, 25}, {1, 17}, {1, 9}, {1, 1}, {0, 15}, {1, 15}, {1, 40}, {1, 32}, {1, 24}, {1, 16}, {1, 8}, {0, 1}},
            {{0, 7}, {0, 15}, {0, 23}, {0, 31}, {0, 39}, {0, 47}, {0, 55}, {0, 63}, {0, 6}, {0, 14}, {0, 22}, {0, 30}, {0, 38}, {0, 46}, {0, 54}, {0, 55}, {0, 5}, {0, 13}, {0, 21}, {0, 29}, {0, 37}, {0, 45}, {0, 46}, {0, 47}, {0, 4}, {0, 12}, {0, 20}, {0, 28}, {0, 36}, {0, 37}, {0, 38}, {0, 39}, {0, 3}, {0, 11}, {0, 19}, {0, 27}, {0, 28}, {0, 29}, {0, 30}, {0, 31}, {0, 2}, {0, 10}, {0, 18}, {0, 19}, {0, 20}, {0, 21}, {0, 22}, {0, 23}, {0, 1}, {0, 9}, {0, 10}, {0, 11}, {0, 12}, {0, 13}, {0, 14}, {0, 15}, {0, 0}, {0, 1}, {0, 2}, {0, 3}, {0, 4}, {0, 5}, {0, 6}, {0, 7}},
            {{0, 15}, {1, 57}, {1, 58}, {1, 59}, {1, 60}, {1, 61}, {1, 62}, {0, 55}, {1, 15}, {1, 49}, {1, 50}, {1, 51}, {1, 52}, {1, 53}, {1, 54}, {1, 55}, {1, 40}, {1, 41}, {1, 42}, {1, 43}, {1, 44}, {1, 45}, {1, 46}, {1, 47}, {1, 32}, {1, 33}, {1, 34}, {1, 35}, {1, 36}, {1, 37}, {1, 38}, {1, 39}, {1, 24}, {1, 25}, {1, 26}, {1, 27}, {1, 28}, {1, 29}, {1, 30}, {1, 31}, {1, 16}, {1, 17}, {1, 18}, {1, 19}, {1, 20}, {1, 21}, {1, 22}, {1, 23}, {1, 8}, {1, 9}, {1, 10}, {1, 11}, {1, 12}, {1, 13}, {1, 14}, {1, 15}, {0, 1}, {1, 1}, {1, 2}, {1, 3}, {1, 4}, {1, 5}, {1, 6}, {0, 6}},
            {{0, 23}, {1, 58}, {2, 58}, {2, 59}, {2, 60}, {2, 61}, {1, 61}, {0, 47}, {1, 23}, {2, 49}, {2, 50}, {2, 51}, {2, 52}, {2, 53}, {2, 54}, {1, 47}, {2, 23}, {2, 41}, {2, 42}, {2, 43}, {2, 44}, {2, 45}, {2, 46}, {2, 47}, {2, 32}, {2, 33}, {2, 34}, {2, 35}, {2, 36}, {2, 37}, {2, 38}, {2, 39}, {2, 24}, {2, 25}, {2, 26}, {2, 27}, {2, 28}, {2, 29}, {2, 30}, {2, 31}, {2, 16}, {2, 17}, {2, 18}, {2, 19}, {2, 20}, {2, 21}, {2, 22}, {2, 23}, {1, 16}, {2, 9}, {2, 10}, {2, 11}, {2, 12}, {2, 13}, {2, 14}, {1, 40}, {0, 2}, {1, 2}, {2, 2}, {2, 3}, {2, 4}, {2, 5}, {1, 5}, {0, 5}},
            {{0, 31}, {1, 59}, {2, 59}, {3, 59}, {3, 60}, {2, 60}, {1, 60}, {0, 39}, {1, 31}, {3, 49}, {3, 50}, {3, 51}, {3, 52}, {3, 53}, {3, 54}, {1, 39}, {2, 31}, {3, 41}, {3, 42}, {3, 43}, {3, 44}, {3, 45}, {3, 46}, {2, 39}, {3, 31}, {3, 33}, {3, 34}, {3, 35}, {3, 36}, {3, 37}, {3, 38}, {3, 39}, {3, 24}, {3, 25}, {3, 26}, {3, 27}, {3, 28}, {3, 29}, {3, 30}, {3, 31}, {2, 24}, {3, 17}, {3, 18}, {3, 19}, {3, 20}, {3, 21}, {3, 22}, {2, 32}, {1, 24}, {3, 9}, {3, 10}, {3, 11}, {3, 12}, {3, 13}, {3, 14}, {1, 32}, {0, 3}, {1, 3}, {2, 3}, {3, 3}, {3, 4}, {2, 4}, {1, 4}, {0, 4}},
            {{0, 39}, {1, 60}, {2, 60}, {3, 60}, {3, 59}, {2, 59}, {1, 59}, {0, 31}, {1, 39}, {3, 54}, {3, 53}, {3, 52}, {3, 51}, {3, 50}, {3, 49}, {1, 31}, {2, 39}, {3, 46}, {3, 45}, {3, 44}, {3, 43}, {3, 42}, {3, 41}, {2, 31}, {3, 39}, {3, 38}, {3, 37}, {3, 36}, {3, 35}, {3, 34}, {3, 33}, {3, 31}, {3, 31}, {3, 30}, {3, 29}, {3, 28}, {3, 27}, {3, 26}, {3, 25}, {3, 24}, {2, 32}, {3, 22}, {3, 21}, {3, 20}, {3, 19}, {3, 18}, {3, 17}, {2, 24}, {1, 32}, {3, 14}, {3, 13}, {3, 12}, {3, 11}, {3, 10}, {3, 9}, {1, 24}, {0, 4}, {1, 4}, {2, 4}, {3, 4}, {3, 3}, {2, 3}, {1, 3}, {0, 3}},
            {{0, 47}, {1, 61}, {2, 61}, {2, 60}, {2, 59}, {2, 58}, {1, 58}, {0, 23}, {1, 47}, {2, 54}, {2, 53}, {2, 52}, {2, 51}, {2, 50}, {2, 49}, {1, 23}, {2, 47}, {2, 46}, {2, 45}, {2, 44}, {2, 43}, {2, 42}, {2, 41}, {2, 23}, {2, 39}, {2, 38}, {2, 37}, {2, 36}, {2, 35}, {2, 34}, {2, 33}, {2, 32}, {2, 31}, {2, 30}, {2, 29}, {2, 28}, {2, 27}, {2, 26}, {2, 25}, {2, 24}, {2, 23}, {2, 22}, {2, 21}, {2, 20}, {2, 19}, {2, 18}, {2, 17}, {2, 16}, {1, 40}, {2, 14}, {2, 13}, {2, 12}, {2, 11}, {2, 10}, {2, 9}, {1, 16}, {0, 5}, {1, 5}, {2, 5}, {2, 4}, {2, 3}, {2, 2}, {1, 2}, {0, 2}},
            {{0, 55}, {1, 62}, {1, 61}, {1, 60}, {1, 59}, {1, 58}, {1, 57}, {0, 15}, {1, 55}, {1, 54}, {1, 53}, {1, 52}, {1, 51}, {1, 50}, {1, 49}, {1, 15}, {1, 47}, {1, 46}, {1, 45}, {1, 44}, {1, 43}, {1, 42}, {1, 41}, {1, 40}, {1, 39}, {1, 38}, {1, 37}, {1, 36}, {1, 35}, {1, 34}, {1, 33}, {1, 32}, {1, 31}, {1, 30}, {1, 29}, {1, 28}, {1, 27}, {1, 26}, {1, 25}, {1, 24}, {1, 23}, {1, 22}, {1, 21}, {1, 20}, {1, 19}, {1, 18}, {1, 17}, {1, 16}, {1, 15}, {1, 14}, {1, 13}, {1, 12}, {1, 11}, {1, 10}, {1, 9}, {1, 8}, {0, 6}, {1, 6}, {1, 5}, {1, 4}, {1, 3}, {1, 2}, {1, 1}, {0, 1}},
            {{0, 63}, {0, 55}, {0, 47}, {0, 39}, {0, 31}, {0, 23}, {0, 15}, {0, 7}, {0, 55}, {0, 54}, {0, 46}, {0, 38}, {0, 30}, {0, 22}, {0, 14}, {0, 6}, {0, 47}, {0, 46}, {0, 45}, {0, 37}, {0, 29}, {0, 21}, {0, 13}, {0, 5}, {0, 39}, {0, 38}, {0, 37}, {0, 36}, {0, 28}, {0, 20}, {0, 12}, {0, 4}, {0, 31}, {0, 30}, {0, 29}, {0, 28}, {0, 27}, {0, 19}, {0, 11}, {0, 3}, {0, 23}, {0, 22}, {0, 21}, {0, 20}, {0, 19}, {0, 18}, {0, 10}, {0, 2}, {0, 15}, {0, 14}, {0, 13}, {0, 12}, {0, 11}, {0, 10}, {0, 9}, {0, 1}, {0, 7}, {0, 6}, {0, 5}, {0, 4}, {0, 3}, {0, 2}, {0, 1}, {0, 0}}};

        FinishConstructor();
    }
};

// template <size_t TNX, size_t TNY>
// const size_t Base_IOModel<TNX, TNY>::Nc = TNX *TNY;
} // namespace IO
