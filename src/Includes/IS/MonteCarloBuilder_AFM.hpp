#pragma once

#include "MonteCarlo.hpp"

#include "MarkovChain.hpp"
#include "../Utilities/MPITools.hpp"

namespace MC
{

std::unique_ptr<ABC_MonteCarlo> MonteCarloBuilder_AFM(const Json &jj, const size_t &seed)
{
#ifdef HAVEMPI
    mpi::environment env;
    mpi::communicator world;
#endif

    const std::string modelType = jj["modelType"].get<std::string>();

    if (modelType == "Square2x2_AFM")
    {
        using Model_t = Models::ModelSquare2x2_AFM;
        using IOModel_t = IO::IOSquare2x2_AFM;
        using MarkovInt_t = Markov::MarkovChain<IOModel_t, Model_t>;

        //Init a dummy model just to be sure that all files are present:
        if (mpiUt::Tools::Rank() == mpiUt::Tools::master)
        {
            const Model_t modelDummy(jj);
        }
#ifdef HAVEMPI
        world.barrier();
#endif

        return std::make_unique<MC::MonteCarlo<MarkovInt_t>>(std::make_shared<MarkovInt_t>(jj, seed), jj);
    }
    else if (modelType == "Triangle2x2_AFM")
    {
        using Model_t = Models::ModelTriangle2x2;
        using IOModel_t = IO::IOTriangle2x2;
        using MarkovInt_t = Markov::MarkovChain<IOModel_t, Model_t>;
        //Init a dummy model just to be sure that all files are present:

        if (mpiUt::Tools::Rank() == mpiUt::Tools::master)
        {
            const Model_t modelDummy(jj);
        }
#ifdef HAVEMPI
        world.barrier();
#endif

        return std::make_unique<MC::MonteCarlo<MarkovInt_t>>(std::make_shared<MarkovInt_t>(jj, seed), jj);
    }

    throw std::runtime_error("Miseria: solver and or modelType error in params file. Stupido !");

    return NULL;
}

} // namespace MC
