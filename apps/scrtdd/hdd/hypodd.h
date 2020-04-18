/***************************************************************************
 *   Copyright (C) by ETHZ/SED                                             *
 *                                                                         *
 * This program is free software: you can redistribute it and/or modify    *
 * it under the terms of the GNU Affero General Public License as published*
 * by the Free Software Foundation, either version 3 of the License, or    *
 * (at your option) any later version.                                     *
 *                                                                         *
 * This program is distributed in the hope that it will be useful,         *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU Affero General Public License for more details.                     *
 *                                                                         *
 *                                                                         *
 *   Developed by Luca Scarabello <luca.scarabello@sed.ethz.ch>            *
 ***************************************************************************/

#ifndef __RTDD_APPLICATIONS_HYPODD_H__
#define __RTDD_APPLICATIONS_HYPODD_H__

#include "catalog.h"
#include "wfmngr.h"
#include "solver.h"
#include "xcorrcache.ipp"

#include <seiscomp3/core/baseobject.h>
#include <seiscomp3/seismology/ttt.h>

#include <unordered_map>
#include <map>
#include <unordered_set>
#include <vector>

namespace Seiscomp {
namespace HDD {

struct Config {

    std::vector<std::string> validPphases = {"Pg","P","Px"};
    std::vector<std::string> validSphases = {"Sg","S","Sx"};

    // differential travel time specific
    struct {
        double minWeight      = 0;  // Min weight of phases required (0-1)
        double minEStoIEratio = 0;  // Min epi-sta to  interevent distance ration required
        double minESdist      = 0;  // Min epi-sta distance required
        double maxESdist      =-1;  // Max epi-sta distance allowed
        int minNumNeigh       = 1;  // Min neighbors required
        int maxNumNeigh       =-1;  // Max neighbors allowed (furthest events are discarded)
        int minDTperEvt       = 1;  // Min differential times per event pair required (Including P+S)
        int maxDTperEvt       =-1;  // Max differential times per event pair required (Including P+S)
        // From Waldhauser 2009: to assure a spatially homogeneous subsampling, reference
        // events are selected within each of five concentric, vertically longated
        // ellipsoidal layers of increasing thickness. Each layer has 8 quadrants.
        int numEllipsoids       = 5;
        double maxEllipsoidSize = 10; // km
    } step1Clustering;

    // cross correlation specific
    struct {
        std::string recordStreamURL;

        double minWeight      = 0;  // Min weight of phases required (0-1)
        double minEStoIEratio = 0;  // Min epi-sta to  interevent distance ration required
        double minESdist      = 0;  // Min epi-sta distance required
        double maxESdist      =-1;  // Max epi-sta distance allowed
        int minNumNeigh       = 1;  // Min neighbors required
        int maxNumNeigh       =-1;  // Max neighbors allowed (furthest events are discarded)
        int minDTperEvt       = 1;  // Min differential times per event pair required (Including P+S)
        int maxDTperEvt       =-1;  // Max differential times per event pair required (Including P+S)
        // From Waldhauser 2009: to assure a spatially homogeneous subsampling, reference
        // events are selected within each of five concentric, vertically longated
        // ellipsoidal layers of increasing thickness. Each layer has 8 quadrants.
        int numEllipsoids       = 5;
        double maxEllipsoidSize = 10; // km
    } step2Clustering;

    struct XCorr {
        double minCoef;     // Min xcorr coefficient required (0-1)
        double startOffset; // secs
        double endOffset;   // secs
        double maxDelay;    //secs
        std::vector<std::string> components; // priority list of components to use
    };
    std::map<Catalog::Phase::Type,struct XCorr> xcorr = {
        {Catalog::Phase::Type::P, {}},
        {Catalog::Phase::Type::S, {}}
    };

    // artificial phases
    struct {
        bool enable           = true;
    } artificialPhases;

    struct {
        std::string filterStr = "";
        double resampleFreq = 0;
    } wfFilter;

    struct {
        double minSnr = 0;
        double noiseStart = 0;
        double noiseEnd = 0;
        double signalStart = 0;
        double signalEnd = 0;
    } snr;

    struct {
        std::string type  = "LOCSAT";
        std::string model = "iasp91";
    } ttt;

    struct {
        std::string type = "LSMR"; // LSMR or LSQR
        bool useObservationWeghts = true;
        double dampingFactor = 0.;
        double meanShiftConstrainWeight = 0.;
        unsigned solverIterations = 100;
        unsigned algoIterations = 20;
    } solver;
};



DEFINE_SMARTPOINTER(HypoDD);

class HypoDD : public Core::BaseObject {

    public:

        HypoDD(const CatalogCPtr& catalog, const Config& cfg, const std::string& workingDir);
        virtual ~HypoDD();

        void preloadData();

        CatalogCPtr getCatalog() { return _srcCat; }
        void setCatalog(const CatalogCPtr& catalog);

        CatalogPtr relocateCatalog();
        CatalogPtr relocateSingleEvent(const CatalogCPtr& orgToRelocate);
        void evalXCorr();

        void setWorkingDirCleanup(bool cleanup) { _workingDirCleanup = cleanup; }
        bool workingDirCleanup() { return _workingDirCleanup; }

        void setUseCatalogDiskCache(bool cache) { _useCatalogDiskCache = cache; }
        bool useCatalogDiskCache() { return _useCatalogDiskCache; }

        void setWaveformCacheAll(bool all) { _waveformCacheAll = all; }
        bool waveformCacheAll() { return _waveformCacheAll; }

        void setWaveformDebug(bool debug);
        bool waveformDebug() { return _waveformDebug; }

        static std::string relocationReport(const CatalogCPtr& relocatedEv);

    private:
        std::string generateWorkingSubDir(const Catalog::Event& ev) const;

        CatalogPtr relocateEventSingleStep(const CatalogCPtr& evToRelocateCat, const std::string& workingDir,
                                bool doXcorr, bool computeTheoreticalPhases, double minPhaseWeight,
                                double minESdist, double maxESdist, double minEStoIEratio,
                                int minDTperEvt, int maxDTperEvt, int minNumNeigh, int maxNumNeigh,
                                int numEllipsoids, double maxEllipsoidSize);

        CatalogPtr selectNeighbouringEvents(const CatalogCPtr& catalog,
                                            const Catalog::Event& refEv,
                                            const CatalogCPtr& refEvCatalog,
                                            double minPhaseWeight = 0, double minESdis=0,
                                            double maxESdis=-1, double minEStoIEratio=0,
                                            int minDTperEvt=1, int maxDTperEvt=-1,
                                            int minNumNeigh=1, int maxNumNeigh=-1,
                                            int numEllipsoids=5, double maxEllipsoidSize=10,
                                            bool keepUnmatched=false, int *numNeigh=nullptr) const;
        std::map<unsigned,CatalogPtr> 
        selectNeighbouringEventsCatalog(const CatalogCPtr& catalog, double minPhaseWeight,
                                        double minESdis, double maxESdis, double minEStoIEratio,
                                        int minDTperEvt, int maxDTperEvt,
                                        int minNumNeigh, int maxNumNeigh,
                                        int numEllipsoids, double maxEllipsoidSize,
                                        bool keepUnmatched) const;

        struct ObservationParams {
            struct Entry {
                Catalog::Event event;
                Catalog::Station station;
                char phaseType;
                double travelTime;
            };
            void add(TravelTimeTableInterfacePtr ttt, const Catalog::Event& event,
                     const Catalog::Station& station, char phaseType);
            const Entry& get(unsigned eventId, const std::string stationId, char phaseType ) const;
            void addToSolver(Solver& solver) const;
            private:
            std::unordered_map<std::string,Entry> _entries;
        }; 
        void addObservations(Solver& solver, CatalogPtr& catalog, unsigned evId,
                             bool fixedNeighbours, const XCorrCache& xcorr,
                             ObservationParams& obsparams ) const;
        CatalogPtr loadRelocatedCatalog(const Solver& solver, const CatalogCPtr& originalCatalog,
                                        std::unordered_set<unsigned> eventsToRelocate,
                                        ObservationParams& obsparams ) const;

        void addMissingEventPhases(const CatalogCPtr& searchCatalog,
                                   const Catalog::Event& refEv,
                                   CatalogPtr& refEvCatalog);
        std::vector<Catalog::Phase> findMissingEventPhases(const CatalogCPtr& searchCatalog,
                                                           const Catalog::Event& refEv,
                                                           const CatalogPtr& refEvCatalog);
        typedef std::pair<std::string,Catalog::Phase::Type> MissingStationPhase;
        std::vector<MissingStationPhase> getMissingPhases(const CatalogCPtr& searchCatalog,
                                                          const Catalog::Event& refEv,
                                                          const CatalogPtr& refEvCatalog) const;
        typedef std::pair<Catalog::Event, Catalog::Phase> PhasePeer;
        std::vector<PhasePeer> findPhasePeers(const Catalog::Station& station,
                                              const Catalog::Phase::Type& phaseType,
                                              const CatalogCPtr& searchCatalog) const;
        Catalog::Phase createThoreticalPhase(const Catalog::Station& station,
                                             const Catalog::Phase::Type& phaseType,
                                             const Catalog::Event& refEv,
                                             const std::vector<HypoDD::PhasePeer>& peers,
                                             double phaseVelocity); 

        XCorrCache buildXCorrCache(std::map<unsigned,CatalogPtr>& neighbourCats,
                                   bool computeTheoreticalPhases);
        XCorrCache buildXCorrCache(CatalogPtr& catalog, unsigned evToRelocateId,
                                   bool computeTheoreticalPhases);
        void buildXcorrDiffTTimePairs(CatalogPtr& catalog, const Catalog::Event& refEv,
                                      XCorrCache& xcorr);

        void fixPhases(CatalogPtr& catalog, const Catalog::Event& refEv, XCorrCache& xcorr);

        struct PhaseXCorrCfg {
            WfMngr::CacheType type;
            WfMngr::WfCache* cache;
            bool allowSnrCheck;
        };
        bool xcorrPhases(const Catalog::Event& event1, const Catalog::Phase& phase1, PhaseXCorrCfg& phCfg1,
                         const Catalog::Event& event2, const Catalog::Phase& phase2, PhaseXCorrCfg& phCfg2,
                         double& coeffOut, double& lagOut);
        bool _xcorrPhases(const Catalog::Event& event1, const Catalog::Phase& phase1, PhaseXCorrCfg& phCfg1,
                          const Catalog::Event& event2, const Catalog::Phase& phase2, PhaseXCorrCfg& phCfg2,
                          double& coeffOut, double& lagOut);
        bool xcorr(const GenericRecordCPtr& tr1, const GenericRecordCPtr& tr2, double maxDelay,
                   bool qualityCheck, double& delayOut, double& coeffOut) const;
        Core::TimeWindow xcorrTimeWindowLong(const Catalog::Phase& phase) const;
        Core::TimeWindow xcorrTimeWindowShort(const Catalog::Phase& phase) const;

        void printCounters();

    private:
        bool _workingDirCleanup = true;
        std::string _workingDir;
        std::string _cacheDir;
        std::string _tmpCacheDir;
        std::string _wfDebugDir;

        CatalogCPtr _srcCat;
        CatalogCPtr _ddbgc;

        const Config _cfg;

        WfMngrPtr  _wf;
        WfMngr::WfCache _wfCache;
        bool _useCatalogDiskCache;
        bool _waveformCacheAll;
        bool _waveformDebug;

        TravelTimeTableInterfacePtr _ttt;

        struct {
            unsigned xcorr_performed;
            unsigned xcorr_performed_theo;
            unsigned xcorr_performed_s;
            unsigned xcorr_performed_s_theo;
            unsigned xcorr_good_cc;
            unsigned xcorr_good_cc_theo;
            unsigned xcorr_good_cc_s;
            unsigned xcorr_good_cc_s_theo;
        } mutable _counters;
};

}
}

#endif
