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


#define SEISCOMP_COMPONENT EventScore

#include <seiscomp/logging/log.h>
#include <seiscomp/core/plugin.h>
#include <seiscomp/core/strings.h>
#include <seiscomp/datamodel/origin.h>
#include <seiscomp/plugins/events/scoreprocessor.h>


namespace {


using namespace std;
using namespace Seiscomp;


/**
 * @brief The ScoreProcessor class implements the ScoreProcessor interface
 *        used by scevent to select preferred entities.
 *
 * This class is usually compiled as a shared library and loaded as a
 * plugin into scevent. Furthermore the score processor is configured
 * to use this interface, see REGISTER_ORIGINSCOREPROCESSOR.
 *
 * @code
 * plugins = ${plugins}, tmplevscore
 * eventAssociation.score = template
 * eventAssociation.priorities = SCORE
 * @endcode
 */
class ScoreProcessor : public Client::ScoreProcessor {
	// ------------------------------------------------------------------
	//  X'truction
	// ------------------------------------------------------------------
	public:
		//! C'tor
		ScoreProcessor() {
			// Initialize your plugin here
		}


	// ------------------------------------------------------------------
	//  Public ScoreProcessor interface
	// ------------------------------------------------------------------
	public:
		//! Setup all configuration parameters
		bool setup(const Config::Config &config) override {
			try {
				_param1 = config.getDouble("scoreProcessors.template.param1");
			}
			catch ( ... ) {}

			try {
				_param2 = config.getDouble("scoreProcessors.template.param2");
			}
			catch ( ... ) {}

			return true;
		}

		/**
		 * @brief Evaluates an origin.
		 * This method should return a score value. The higher the score
		 * the higher the origins priority in the process of selecting the
		 * preferred origin.
		 * @param origin The origin to be evaluated
		 * @return The score
		 */
		double evaluate(DataModel::Origin *origin) override {
			return 0;
		}

		/**
		 * @brief Evaluates a focal mechanism.
		 * This method should return a score value. The higher the score
		 * the higher the focal mechanisms priority in the process of selecting
		 * the preferred focal mechanism.
		 * @param fm The focal mechanism to be evaluated
		 * @return The score
		 */
		double evaluate(DataModel::FocalMechanism *fm) override {
			return 0;
		}


	// ------------------------------------------------------------------
	//  Private members
	// ------------------------------------------------------------------
	private:
		string _param1;
		double _param2{0.0};
};


}


ADD_SC_PLUGIN(
	"scevent score plugin template, it does actually nothing",
	"Jan Becker, gempa GmbH",
	0, 0, 1
)


REGISTER_ORIGINSCOREPROCESSOR(ScoreProcessor, "template");
