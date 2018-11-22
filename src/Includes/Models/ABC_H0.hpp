#pragma once
#include "../Utilities/Utilities.hpp"

namespace Models
{
template <size_t TNX, size_t TNY, size_t TNZ = 1>
class ABC_H0
{

  public:
    static const size_t Nx;
    static const size_t Ny;
    static const size_t Nz;
    static const size_t Nc;

    ABC_H0(const ABC_H0 &abc_h0) = default;
    ABC_H0(const Json &tJson) : RSites_(Nc),
                                KWaveVectors_(Nc),
                                NOrb_(tJson["NOrb"].get<size_t>())

    {
        assert(TNX == TNY);

        for (size_t i = 0; i < TNX; i++)
        {
            for (size_t j = 0; j < TNY; j++)
            {
                for (size_t k = 0; k < TNZ; k++)
                {

                    const size_t index = i + TNY * j + TNZ * k;
                    RSites_.at(index) = {static_cast<double>(i), static_cast<double>(j), static_cast<double>(k)};
                    KWaveVectors_.at(index) = {static_cast<double>(i) * 2.0 * M_PI / static_cast<double>(TNX), static_cast<double>(j) * 2.0 * M_PI / static_cast<double>(TNY), static_cast<double>(k) * 2.0 * M_PI / static_cast<double>(TNZ)};
                }
            }
        }

        ReadInHoppings(tJson);
    }

    ~ABC_H0()
    {
    }

    std::vector<double> tIntraOrbitalVec() const { return tIntraOrbitalVec_; };
    std::vector<double> txVec() const { return txVec_; };
    std::vector<double> tyVec() const { return tyVec_; };
    std::vector<double> tzVec() const { return tzVec_; };
    std::vector<double> txyVec() const { return txyVec_; };
    std::vector<double> tx_yVec() const { return tx_yVec_; };
    std::vector<double> txzVec() const { return txzVec_; };
    std::vector<double> tx_zVec() const { return tx_zVec_; };
    std::vector<double> tyzVec() const { return tyzVec_; };
    std::vector<double> ty_zVec() const { return ty_zVec_; };
    std::vector<double> t2xVec() const { return t2xVec_; };
    std::vector<double> t2yVec() const { return t2yVec_; };
    std::vector<double> t2zVec() const { return t2zVec_; };
    std::vector<double> t3Vec() const { return t3Vec_; };

    size_t n_rows() const { return Nc * NOrb_; };
    size_t n_cols() const { return Nc * NOrb_; };
    ClusterSites_t RSites() const { return RSites_; };
    ClusterSites_t KWaveVectors() const { return KWaveVectors_; };

    void ReadInHoppings(const Json &tJson)
    {

        assert(tJson["tParameters"].size() == NOrb_ * (NOrb_ + 1) / 2);

        for (size_t o1 = 0; o1 < NOrb_; o1++)
        {
            for (size_t o2 = o1; o2 < NOrb_; o2++)
            {

                const std::string o1o2Name = std::to_string(o1) + std::to_string(o2);
                const Json &jj = tJson["tParameters"][o1o2Name];

                tIntraOrbitalVec_.push_back(jj["tIntra"].get<double>());
                txVec_.push_back(jj["tx"].get<double>());
                tyVec_.push_back(jj["ty"].get<double>());
                tzVec_.push_back(jj["tz"].get<double>());

                txyVec_.push_back(jj["tx=y"].get<double>());
                tx_yVec_.push_back(jj["tx=-y"].get<double>());
                txzVec_.push_back(jj["tx=z"].get<double>());
                tx_zVec_.push_back(jj["tx=-z"].get<double>());
                tyzVec_.push_back(jj["ty=z"].get<double>());
                ty_zVec_.push_back(jj["ty=-z"].get<double>());

                t2xVec_.push_back(jj["t2x"].get<double>());
                t2yVec_.push_back(jj["t2y"].get<double>());
                t2zVec_.push_back(jj["t2z"].get<double>());

                t3Vec_.push_back(jj["t3"].get<double>());
            }
        }
    }

    double Eps0k(const double &kx, const double &ky, const double &kz, const size_t &NIndepOrbIndex)
    {
        const double eps0k =

            // On site energy
            tIntraOrbitalVec_.at(NIndepOrbIndex) +

            //First neighbor hopping
            2.0 * txVec_.at(NIndepOrbIndex) * std::cos(kx) +
            2.0 * tyVec_.at(NIndepOrbIndex) * std::cos(ky) +
            2.0 * tzVec_.at(NIndepOrbIndex) * std::cos(kz) +

            //Second neigbor hopping in diagonal
            2.0 * txyVec_.at(NIndepOrbIndex) * std::cos(kx + ky) +
            2.0 * tx_yVec_.at(NIndepOrbIndex) * std::cos(kx - ky) +
            2.0 * txzVec_.at(NIndepOrbIndex) * std::cos(kx + kz) +
            2.0 * tx_zVec_.at(NIndepOrbIndex) * std::cos(kx - kz) +
            2.0 * tyzVec_.at(NIndepOrbIndex) * std::cos(ky + kz) +
            2.0 * ty_zVec_.at(NIndepOrbIndex) * std::cos(ky - kz) +

            //second neighbor hopping in straight line
            2.0 * t2xVec_.at(NIndepOrbIndex) * std::cos(2.0 * kx) +
            2.0 * t2yVec_.at(NIndepOrbIndex) * std::cos(2.0 * ky) +
            2.0 * t2zVec_.at(NIndepOrbIndex) * std::cos(2.0 * kz) +

            //Third neihbor hopping
            2.0 * t3Vec_.at(NIndepOrbIndex) * (std::cos(kx + ky + kz) + std::cos(kx + ky - kz) + std::cos(kx - ky + kz) + std::cos(-kx + ky + kz));

        return eps0k;
    }

    ClusterMatrixCD_t operator()(const double &kTildeX, const double &kTildeY, const double &kTildeZ) //return t(ktilde)
    {
        const cd_t im = cd_t(0.0, 1.0);
        const SiteVector_t ktilde = {kTildeX, kTildeY, kTildeZ};
        const size_t NS = Nc * NOrb_;
        ClusterMatrixCD_t HoppingKTilde(NS, NS);
        HoppingKTilde.zeros();

        // std::cout << "Here " << std::endl;
        for (size_t o1 = 0; o1 < NOrb_; o1++)
        {
            for (size_t o2 = 0; o2 < NOrb_; o2++)
            {
                const size_t NIndepOrbIndex = Utilities::GetIndepOrbitalIndex(o1, o2, NOrb_);
                for (size_t i = 0; i < Nc; i++)
                {
                    for (size_t j = 0; j < Nc; j++)
                    {
                        for (const SiteVector_t &K : this->KWaveVectors_)
                        {
#ifdef DCA
                            HoppingKTilde(i + o1 * Nc, j + o2 * Nc) += std::exp(im * dot(K, RSites_.at(i) - RSites_[j])) * Eps0k(K(0) + kTildeX, K(1) + kTildeY, K(2) + kTildeZ, NIndepOrbIndex);

#else
                            HoppingKTilde(i + o1 * Nc, j + o2 * Nc) += std::exp(im * dot(K + ktilde, RSites_.at(i) - RSites_[j])) * Eps0k(K(0) + kTildeX, K(1) + kTildeY, K(2) + kTildeZ, NIndepOrbIndex);
#endif
                        }
                    }
                }
            }
        }
        // std::cout << "Here 2" << std::endl;

        return (HoppingKTilde / static_cast<double>(Nc));
    }

    void SaveTKTildeAndHybFM()
    {
        //check if  file exists:
        using boost::filesystem::exists;
        if ((exists("tktilde.arma") && exists("tloc.arma")) && exists("hybFM.arma"))
        {
            ClusterMatrixCD_t tmp;
            tmp.load("tloc.arma");
            if (tmp.n_cols == Nc * NOrb_)
            {
                return;
            }
        }
        std::cout << "Calculating tktilde, tloc and hybFM. " << std::endl;

        const double deltak = (std::abs(tzVec_.at(0)) < 1e-10) ? 0.01 : 0.04;
        const size_t kxtildepts = 2.0 * M_PI / deltak / static_cast<double>(std::min(Nx, Ny));
        const size_t kytildepts = 2.0 * M_PI / deltak / static_cast<double>(std::min(Nx, Ny));
        const size_t kztildepts = (std::abs(tzVec_.at(0)) < 1e-10) ? 1 : 2.0 * M_PI / deltak / static_cast<double>(Nz);

        const size_t NS = Nc * NOrb_;
        ClusterCubeCD_t tKTildeGrid(NS, NS, kxtildepts * kytildepts * kztildepts);
        tKTildeGrid.zeros();
        ClusterMatrixCD_t tLoc(NS, NS);
        tLoc.zeros();

        size_t sliceindex = 0;
        for (size_t kx = 0; kx < kxtildepts; kx++)
        {
            const double kTildeX = static_cast<double>(kx) / static_cast<double>(kxtildepts) * 2.0 * M_PI / static_cast<double>(Nx);
            for (size_t ky = 0; ky < kytildepts; ky++)
            {
                const double kTildeY = static_cast<double>(ky) / static_cast<double>(kytildepts) * 2.0 * M_PI / static_cast<double>(Ny);
                for (size_t kz = 0; kz < kztildepts; kz++)
                {
                    const double kTildeZ = static_cast<double>(kz) / static_cast<double>(kztildepts) * 2.0 * M_PI / static_cast<double>(Nz);
                    tKTildeGrid.slice(sliceindex) = (*this)(kTildeX, kTildeY, kTildeZ);
                    tLoc += tKTildeGrid.slice(sliceindex);
                    sliceindex++;
                }
            }
        }

        tKTildeGrid.save("tktilde.arma");
        tLoc /= static_cast<double>(tKTildeGrid.n_slices);
        tLoc.save("tloc.arma", arma::arma_ascii);

        //First moment of hyb
        ClusterMatrixCD_t hybFM(NS, NS);
        hybFM.zeros();

        const size_t Nkpts = tKTildeGrid.n_slices;
        for (size_t nn = 0; nn < Nkpts; nn++)
        {
            hybFM += tKTildeGrid.slice(nn) * tKTildeGrid.slice(nn);
        }
        hybFM /= Nkpts;
        hybFM -= tLoc * tLoc;
        hybFM.save("hybFM.arma", arma::arma_ascii);
    }

  protected:
    ClusterSites_t RSites_;
    ClusterSites_t KWaveVectors_;

    std::vector<double> tIntraOrbitalVec_;
    std::vector<double> txVec_;
    std::vector<double> tyVec_;
    std::vector<double> tzVec_;
    std::vector<double> txyVec_;
    std::vector<double> tx_yVec_; //along x=-y diagonal
    std::vector<double> txzVec_;
    std::vector<double> tx_zVec_;
    std::vector<double> tyzVec_;
    std::vector<double> ty_zVec_;
    std::vector<double> t2xVec_;
    std::vector<double> t2yVec_;
    std::vector<double> t2zVec_;

    std::vector<double> t3Vec_;

    const size_t NOrb_;
};

template <size_t TNX, size_t TNY, size_t TNZ>
const size_t ABC_H0<TNX, TNY, TNZ>::Nx = TNX;

template <size_t TNX, size_t TNY, size_t TNZ>
const size_t ABC_H0<TNX, TNY, TNZ>::Ny = TNY;

template <size_t TNX, size_t TNY, size_t TNZ>
const size_t ABC_H0<TNX, TNY, TNZ>::Nz = TNZ;

template <size_t TNX, size_t TNY, size_t TNZ>
const size_t ABC_H0<TNX, TNY, TNZ>::Nc = TNX *TNY;

} // namespace Models
