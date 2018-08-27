/***************************************************************************
 *   Copyright (C) by ETHZ/SED                                             *
 *                                                                         *
 *   You can redistribute and/or modify this program under the             *
 *   terms of the SeisComP Public License.                                 *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   SeisComP Public License for more details.                             *
 *                                                                         *
 *   Developed by Luca Scarabello <luca.scarabello@sed.ethz.ch>            *
 ***************************************************************************/

#ifndef __SEISCOMP_APPLICATIONS_HYPODD_H__
#define __SEISCOMP_APPLICATIONS_HYPODD_H__

#include <seiscomp3/core/baseobject.h>
#include <seiscomp3/core/genericrecord.h>
#include <seiscomp3/core/recordsequence.h>
#include <seiscomp3/datamodel/databasequery.h>

#include <map>
#include <vector>

#include "hypodd.h"

namespace Seiscomp {
namespace HDD {

DEFINE_SMARTPOINTER(Catalog);

// DD background catalog
class Catalog : public Core::BaseObject {
	public:
		struct Station {
			std::string id;
			double latitude;
			double longitude;
			double elevation; // km
			// seiscomp info
			std::string networkCode;
			std::string stationCode;
		};

		struct Event {
			std::string id;
			Core::Time time;
			double latitude;
			double longitude;
			double depth;   // km
			double magnitude;
			double horiz_err;
			double depth_err;
			double tt_residual;
			// seiscomp info
			std::string originId;
			std::string eventId;
		};

		struct Phase {
			std::string id;
			std::string event_id;
			std::string station_id;
			Core::Time time;
			double weight;       // 0-1 interval
			std::string type;
			// seiscomp info
			std::string networkCode;
			std::string stationCode;
			std::string locationCode;
			std::string channelCode;
		};

		Catalog(const std::map<std::string,Station>& stations,
                  const std::map<std::string,Event>& events,
                  const std::multimap<std::string,Phase>& phases);
		Catalog(const std::string& stationFile,
		          const std::string& catalogFile,
		          const std::string& phaFile);
		Catalog(const std::vector<std::string>& ids, DataModel::DatabaseQuery* query);
		Catalog(const std::string& idFile, DataModel::DatabaseQuery* query);
		const std::map<std::string,Station>& getStations() { return _stations;}
		const std::map<std::string,Event>& getEvents() { return _events;}
		const std::multimap<std::string,Phase>& getPhases() { return _phases;}
		CatalogPtr merge(const CatalogPtr& other);

	private:
		void initFromIds(const std::vector<std::string>& ids, DataModel::DatabaseQuery* query);
		DataModel::Station* findStation(const std::string& netCode, const std::string& staCode,
		                                Seiscomp::Core::Time, DataModel::DatabaseQuery* query);

		std::map<std::string,Station> _stations; // indexed by station id
		std::map<std::string,Event> _events; //indexed by event id
		std::multimap<std::string,Phase> _phases; //indexed by event id
};

struct Config {

	// ph2dt config specifig (catalog relocation only)
	struct {
		std::string exec = "ph2dt";
		int minwght; // MINWGHT: min. pick weight allowed [-1]
		int maxdist; // MAXDIST: max. distance in km between event pair and stations [200]
		int maxsep;  // MAXSEP: max. hypocentral separation in km [10]
		int maxngh;  // MAXNGH: max. number of neighbors per event [10]
		int minlnk;  // MINLNK: min. number of links required to define a neighbor [8]
		int minobs;  // MINOBS: min. number of links per pair saved [8]
		int maxobs;  // MAXOBS: max. number of links per pair saved [20]
	} ph2dt;

	// hypodd executable specific
	struct {
		std::string exec = "hypodd";
		std::string ctrlFile;
	} hypodd;

	std::vector<std::string> allowedPhases = {"P", "S"};

	// differential travel time specific
	struct {
		double minWeight = 0.05;  // Min weight of phases to be considered for CT (0-1)
		double maxESdist = 80.0;   // Max epi-sta epidistance to be considered for ct
		double maxIEdist = 20.0;   // Max interevent-distance for ct (km)
		int minNumNeigh = 6;      // Min neighbors in DDBGC for ct (fail if not enough)
		int maxNumNeigh = 20;     // Max neighbors in DDBGC for ct (furthest events are discarded)
		int minDTperEvt = 6;      // Min dt to use an event for ct (Including P+S)
	} dtt;

	// cross correlation specific
	struct {
		double minWeight = 0.05;  // Min weight of phases to be considered for CC (0-1)
		double maxESdist = 80.0;   // Max epi-sta epidistance to be considered for CC
		double maxIEdist = 10.0;   // Max interevent-distance for cc (km)
		int minNumNeigh = 3;      // Min neighbors in DDBGC for cc (fail if not enough)
		int maxNumNeigh = 20;     // Max neighbors in DDBGC for cc (furthest events are discarded)
		int minDTperEvt = 4;      // Min pairs to use an event for cc
		double minCoef = 0.40;    // Min xcorr coefficient to keep a phase pair   (0-1)
		std::string recordStreamURL;
		int filterOrder = 3;
		double filterFmin = -1;
		double filterFmax = -1;
		double filterFsamp = 0;
		double timeBeforePick = 3; // secs
		double timeAfterPick = 3;  // secs
		double maxDelay = 1; //secs
	} xcorr;
};

DEFINE_SMARTPOINTER(HypoDD);

class HypoDD : public Core::BaseObject {
	public:
		HypoDD(const CatalogPtr& input, const Config& cfg, const std::string& workingDir);
		~HypoDD();
		CatalogPtr relocateCatalog();
		CatalogPtr relocateSingleEvent(DataModel::Origin *org);
		void setWorkingDirCleanup(bool cleanup) { _workingDirCleanup = cleanup; }
	private:
		void createStationDatFile(const std::string& staFileName, const CatalogPtr& catalog);
		void createPhaseDatFile(const std::string& catFileName, const CatalogPtr& catalog);
		void createEventDatFile(const std::string& eventFileName, const CatalogPtr& catalog);
		void createDtCtFile(const CatalogPtr& catalog, const CatalogPtr& org, const std::string& dtctFile);
		void xcorrCatalog(const std::string& dtctFile, const std::string& dtccFile);
		void xcorrSingleEvent(const CatalogPtr& catalog, const CatalogPtr& org, const std::string& dtccFile);
		bool xcorr(const GenericRecordPtr& tr1, const GenericRecordPtr& tr2, double maxDelay,
              double& delayOut, double& coeffOut);
		void runPh2dt(const std::string& workingDir, const std::string& stationFile, const std::string& phaseFile);
		void runHypodd(const std::string& workingDir, const std::string& dtccFile, const std::string& dtctFile,
		               const std::string& eventFile, const std::string& stationFile);
		CatalogPtr loadRelocatedCatalog(const std::string& ddrelocFile, const CatalogPtr& originalCatalog);
		double computeDistance(double lat1, double lon1, double depth1,
		                       double lat2, double lon2, double depth2);
		CatalogPtr selectNeighbouringEvents(const CatalogPtr& catalog, const CatalogPtr& org,
		                                      double maxESdis, double maxIEdis, int minNumNeigh=0,
		                                      int maxNumNeigh=0, int minDTperEvt=0);
		CatalogPtr extractEvent(const CatalogPtr& catalog, const std::string& eventId);
		GenericRecordPtr getWaveform(const Catalog::Event& ev, const Catalog::Phase& ph,
		                             std::map<std::string,GenericRecordPtr>& cache);
		GenericRecordPtr loadWaveform(const Core::Time& starttime,
		                              double duration,
		                              const std::string& networkCode,
		                              const std::string& stationCode,
		                              const std::string& locationCode,
		                              const std::string& channelCode);
		bool merge(GenericRecord &trace, const RecordSequence& seq);
		bool trim(GenericRecord &trace, const Core::TimeWindow& tw);
		void filter(GenericRecord &trace, bool demeaning=true,
                    int order=3, double fmin=-1, double fmax=-1, double fsamp=0);
		std::string generateWorkingSubDir(const DataModel::Origin *org);
	private:
		std::string _workingDir;
		CatalogPtr _ddbgc;
		Config _cfg;
		bool _workingDirCleanup = true;
		std::map<std::string, GenericRecordPtr> _wfCache;
};

}
}

#endif
