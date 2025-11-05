/***************************************************************************
 * Copyright (C) Jan Becker, gempa GmbH                                    *
 * All rights reserved.                                                    *
 * Contact: jabe@gempa.de                                                  *
 *                                                                         *
 * GNU Affero General Public License Usage                                 *
 * This file may be used under the terms of the GNU Affero                 *
 * Public License version 3.0 as published by the Free Software Foundation *
 * and appearing in the file LICENSE included in the packaging of this     *
 * file. Please review the following information to ensure the GNU Affero  *
 * Public License version 3.0 requirements will be met:                    *
 * https://www.gnu.org/licenses/agpl-3.0.html.                             *
 ***************************************************************************/


// Defines the logging component which can be shown when using
// --print-component 1 with each SeisComP application.
#define SEISCOMP_COMPONENT IDCINFRA


// Include the respective headers
// - Logging: to use the SEISCOMP_* logging macros
#include <seiscomp/logging/log.h>
// - Plugin: to enable building a plugin
#include <seiscomp/core/plugin.h>
// - TravelTimeTableInterface interface required to derive from it
#include <seiscomp/seismology/ttt.h>
// - Function to compute distance
#include <seiscomp/math/geo.h>
// - For global application instance to read configuration from
#include <seiscomp/system/application.h>

// - lower_bound, is_sorted
#include <algorithm>
// - strcmp
#include <cstring>


// Everything is implemented in a private namespace to not export any symbols to keep
// the global symbol space clean when loading the plugin.
namespace {


#define PHASE_NAME "Is"


// Use some standard namespaces to write more compact code.
using namespace std;
using namespace Seiscomp;


struct Node {
	double distance; // Degrees
	double celerity; // km/s

	bool operator<(const Node &other) const {
		return distance < other.distance;
	}

	bool operator<(double dist) const {
		return distance < dist;
	}
};

using Table = vector<Node>;

Table IDCDefault = {
	{ 0.0, 0.33 },
	{ 1.2, 0.295 },
	{ 20.0, 0.303 }
};

using Models = map<string, Table>;


class IDCInfra : public TravelTimeTableInterface {
	// ----------------------------------------------------------------------
	//  X'truction
	// ----------------------------------------------------------------------
	public:
		//! C'tor
		IDCInfra() = default;


	// ----------------------------------------------------------------------
	//  Public TravelTimeTable interface
	// ----------------------------------------------------------------------
	public:
		bool setModel(const string &model) override {
			if ( model == "IDC_2010" ) {
				_model = model;
				return true;
			}

			loadConfig();

			auto it = _tables.find(model);
			if ( it == _tables.end() ) {
				return false;
			}

			_model = it->first;
			_table = &it->second;
			return true;
		}

		const string &model() const override {
			return _model;
		}

		TravelTimeList *compute(double lat1, double lon1, double dep1,
		                        double lat2, double lon2, double alt2,
		                        int /*ellc*/) override {
			double distDeg = 0;
			Math::Geo::delazi(lat1, lon1, lat2, lon2, &distDeg);

			auto *list = new TravelTimeList;

			list->delta = distDeg;
			list->depth = dep1;
			list->push_back(get(distDeg, alt2));

			return list;
		}

		TravelTime compute(const char *phase,
		                   double lat1, double lon1, double dep1,
		                   double lat2, double lon2, double alt2,
		                   int /*ellc*/) override {
			if ( !phase || (strcmp(phase, PHASE_NAME) != 0) ) {
				throw NoPhaseError();
			}

			double distDeg = 0;
			Math::Geo::delazi(lat1, lon1, lat2, lon2, &distDeg);

			return get(distDeg, alt2);
		}

		TravelTime computeFirst(double lat1, double lon1, double dep1,
		                        double lat2, double lon2, double alt2,
		                        int ellc) override {
			return compute(PHASE_NAME, lat1, lon1, dep1, lat2, lon2, alt2, ellc);
		}


	private:
		void loadConfig() {
			if ( _tablesInitialized ) {
				return;
			}

			_tablesInitialized = true;

			auto *app = System::Application::Instance();
			if ( !app ) {
				return;
			}

			try {
				auto tables = app->configGetStrings("ttt.idcinfra.tables");
				for ( auto &table : tables ) {
					if ( table == "IDC_2010" ) {
						// This is the default and hard-coded.
						continue;
					}

					try {
						auto dists = app->configGetDoubles("ttt.idcinfra." + table + ".distances");
						auto celerities = app->configGetDoubles("ttt.idcinfra." + table + ".celerities");

						if ( dists.size() < 1 || (dists.size() != celerities.size()) ) {
							SEISCOMP_ERROR("%s: invalid configuration", table);
							continue;
						}

						if ( !is_sorted(dists.begin(), dists.end()) ) {
							SEISCOMP_WARNING("%s: distances not sorted", table);
							continue;
						}

						SEISCOMP_INFO("%s: loaded %d distances / %d celerities",
						              table, dists.size(), celerities.size());

						auto &ttt = _tables[table];
						for ( size_t i = 0; i < dists.size(); ++i ) {
							SEISCOMP_DEBUG("%s: %f %f", table, dists[i], celerities[i]);
							ttt.push_back({ dists[i], celerities[i] });
						}
					}
					catch ( ... ) {
						SEISCOMP_ERROR("%s: incomplete table configuration", table);
					}
				}
			}
			catch ( ... ) {}
		}

		double findCelerity(double distDeg) const {
			auto it = lower_bound(_table->begin(), _table->end(), distDeg);
			if ( it == _table->end() ) {
				return _table->back().celerity;
			}
			return it->celerity;
		}

		TravelTime get(double distDeg, double alt2) {
			double c = findCelerity(distDeg);
			double distKm = Math::Geo::deg2km(distDeg);

			// main horizontal travel time
			double t = distKm / c;

			// optional altitude correction (disabled by default)
			// if (alt2 != 0.0)
			//     t += std::fabs(alt2) / 1000.0 / c;

			SEISCOMP_DEBUG("%s: dist=%.3f° (%.1f km) c=%.3f km/s -> t=%.1f s",
			               _model, distDeg, distKm, c, t);

			return { PHASE_NAME, t, -1, -1, -1, -1 };
		}


	private:
		string              _model{"IDC_2010"};
		Table              *_table{&IDCDefault};
		map<string, Table>  _tables;
		bool                _tablesInitialized{false};
};


}


// This defines the entry point after loading this library dynamically. That is
// mandatory if a plugin should be loaded with a SeisComP application.
ADD_SC_PLUGIN(
	"TravelTimeTable for infrasound template which utilizes parameters of the IDC model.",
	"Bernd Weber, gempa GmbH",
	0, 0, 1
)


// Bind the class IDCInfra to the name "idcinfra". This allows later to
// instantiate this class via the interface name. This is different to the generic
// class factory as it registeres a concrete interface, here, the TravelTimeTableInterface.
REGISTER_TRAVELTIMETABLE(IDCInfra, "idcinfra");
