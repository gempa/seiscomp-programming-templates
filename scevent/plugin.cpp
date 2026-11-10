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
#include <seiscomp/datamodel/event.h>
#include <seiscomp/datamodel/origin.h>
#include <seiscomp/io/httpclient.h>

#include <seiscomp/plugins/events/eventprocessor.h>

#include <3rd-party/rapidjson/document.h>


namespace {


using namespace std;
using namespace Seiscomp;
using namespace Seiscomp::DataModel;


/**
 * @brief The EventRESTProcessor class implements the EventProcessor
 *        interface used by scevent to process a new or updated event.
 *
 * This class is usually compiled as a shared library and loaded as a
 * plugin into scevent.
 *
 * @code
 * plugins = ${plugins}, tmplevupdrest
 * @endcode
 */
class EventRESTProcessor : public Client::EventProcessor {
	// ------------------------------------------------------------------
	//  X'truction
	// ------------------------------------------------------------------
	public:
		//! C'tor
		EventRESTProcessor() {
			// Initialize your plugin here
		}


	// ------------------------------------------------------------------
	//  Public ScoreProcessor interface
	// ------------------------------------------------------------------
	public:
		//! Setup all configuration parameters
		bool setup(const Config::Config &config) override {
			return true;
		}

		// Selecting the preferred magnitude is not implemented in this processor.
		Magnitude *preferredMagnitude(const Origin *) override {
			return {};
		}

		/**
		 * @brief Processes an event.
		 * The preferred object (Origin, Magnitude, FocalMechanism) are
		 * guaranteed to be found with *::Find(id) methods.
		 * This method should return true if the event objects needs an
		 * update.
		 * @param event The event to be processed.
		 * @param isNewEvent Whether the event is a new event or an updated event.
		 * @param journals The list of current event journals.
		 * @return Flag indicating that the event has updated.
		 */
		bool process(Event *event, bool isNewEvent, const Journal &journals) override {
			IO::HTTPClient http;
			auto *org = Origin::Find(event->preferredOriginID());

			if ( !org ) {
				SEISCOMP_WARNING(" + evupdrest: no origin information found");
				return false;
			}

			try {
				// Timeout of 5 seconds to not delay event processing too much.
				http.setTimeout(5);
				http.get(Core::stringify("http://localhost:8000/event_description/local_time?lat=%.4f&lon=%.4f",
				                         org->latitude().value(), org->longitude().value()));
				// One megabyte max
				auto content = http.readBinary(1024 * 1024);
				SEISCOMP_DEBUG("%s", content);

				rapidjson::Document doc;
				doc.ParseInsitu(content.data());
				if ( doc.HasParseError() ) {
					SEISCOMP_ERROR(" + evupdrest: returned REST document cannot be parsed");
					return false;
				}

				if ( !doc.HasMember("time_zones") ) {
					SEISCOMP_ERROR(" + evupdrest: API did not return the region attribute");
					return false;
				}

				auto &val = doc["time_zones"];
				if ( !val.IsArray() ) {
					SEISCOMP_ERROR(" + evupdrest: API did not return time zones as array");
					return false;
				}

				string timeZones;

				const auto &arr = val.GetArray();
				for ( const auto &val : arr ) {
					if ( !val.IsString() ) {
						SEISCOMP_WARNING(" + evupdrest: ignored time zone entry, expected string");
						continue;
					}
					if ( !timeZones.empty() ) {
						timeZones += ",";
					}
					timeZones += val.GetString();
				}

				auto *desc = event->eventDescription(EventDescriptionIndex(LOCAL_TIME));
				if ( !desc ) {
					desc = new EventDescription;
					desc->setType(LOCAL_TIME);
					event->add(desc);
				}
				else if ( desc->text() == timeZones ) {
					return false;
				}

				desc->setText(timeZones);
				desc->update();
			}
			catch ( exception &e ) {
				SEISCOMP_ERROR(" + evupdrest: %s", e.what());
				return false;
			}

			return true;
		}


	// ------------------------------------------------------------------
	//  Private members
	// ------------------------------------------------------------------
	private:
};


}


ADD_SC_PLUGIN(
	"scevent process plugin template using REST APIs",
	"Jan Becker, gempa GmbH",
	0, 0, 1
)


REGISTER_EVENTPROCESSOR(EventRESTProcessor, "updrestapi");
