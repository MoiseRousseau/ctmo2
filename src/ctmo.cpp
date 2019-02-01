
#include "Includes/IS/MonteCarloBuilder.hpp"
#include "Includes/Utilities/SelfConsistencyBuilder.hpp"
#include "Includes/Utilities/FS.hpp"
#include "Includes/PrintVersion.hpp"
#include "Includes/Utilities/Logging.hpp"
#include "Includes/Utilities/CMDParser.hpp"

int main(int argc, char **argv)
{

#ifdef HAVEMPI
    mpi::environment env;
    mpi::communicator world;
#endif

    const CMDParser::CMDInfo cmdInfo = CMDParser::GetProgramOptions(argc, argv);
    if (cmdInfo.exitFromCMD())
    {
        return EXIT_SUCCESS;
    }

    const int ITER = cmdInfo.iter();
    const std::string fnameParams = cmdInfo.fileName();
    Json jjSim;

#ifndef HAVEMPI
    std::ifstream fin(fnameParams);
    fin >> jjSim;
    fin.close();

    Logging::Init(jjSim["logging"]);
    Logging::Info(PrintVersion::GetVersion());
    Logging::Info("Iteration " + std::to_string(ITER));
    const size_t seed = jjSim["monteCarlo"]["seed"].get<size_t>();

    //init a model, to make sure all the files are present and that not all proc write to the same files

    Logging::Trace("ABC_MonteCarlo Creation...");
    const std::unique_ptr<MC::ABC_MonteCarlo> monteCarloMachinePtr = MC::MonteCarloBuilder(jjSim, seed);
    Logging::Trace("ABC_MonteCarlo Created !");

    monteCarloMachinePtr->RunMonteCarlo();

    if (cmdInfo.doSC())
    {
        Logging::Trace("ABC_SelfConsistency Creation...");
        const std::unique_ptr<SelfCon::ABC_SelfConsistency> selfconUpPtr = SelfCon::SelfConsistencyBuilder(jjSim, FermionSpin_t::Up);
        Logging::Trace("ABC_SelfConsistency Created...");

        selfconUpPtr->DoSCGrid();

        Logging::Trace("PrepareNextIter...");
        IO::FS::PrepareNextIter(cmdInfo);
        Logging::Trace("End PrepareNextIter...");
    }

#endif

#ifdef HAVEMPI

    std::string jjSimStr;

    if (mpiUt::Tools::Rank() == mpiUt::Tools::master)
    {
        std::ifstream fin(fnameParams);
        fin >> jjSim;
        jjSimStr = jjSim.dump();
        fin.close();
    }

    mpi::broadcast(world, jjSimStr, mpiUt::Tools::master);
    jjSim = Json::parse(jjSimStr);

    Logging::Init(jjSim["logging"]);
    Logging::Info(PrintVersion::GetVersion());
    Logging::Info("Iteration " + std::to_string(ITER));
    world.barrier();
    //wait_all

    const size_t rank = world.rank();
    const size_t seed = jjSim["monteCarlo"]["seed"].get<size_t>() + 2797 * rank;

    Logging::Trace("ABC_MonteCarlo Creation...");
    std::unique_ptr<MC::ABC_MonteCarlo> monteCarloMachinePtr = MC::MonteCarloBuilder(jjSim, seed);
    Logging::Trace("ABC_MonteCarlo Created...");

    monteCarloMachinePtr->RunMonteCarlo();

    world.barrier();

    if (CMDInfo.doSC())
    {
        Logging::Trace("ABC_SelfConsistency Creation...");
        const std::unique_ptr<SelfCon::ABC_SelfConsistency> selfconUpPtr = SelfCon::SelfConsistencyBuilder(jjSim, FermionSpin_t::Up);
        Logging::Trace("ABC_SelfConsistency Created...");
        selfconUpPtr->DoSCGrid();

        if (mpiUt::Tools::Rank() == mpiUt::Tools::master)
        {
            Logging::Trace("PrepareNextIter...");
            IO::FS::PrepareNextIter(cmdInfo);
            Logging::Trace("End PrepareNextIter...");
        }
    }
#endif

    return EXIT_SUCCESS;
}
