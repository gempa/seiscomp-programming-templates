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
#define SEISCOMP_COMPONENT PGA

// Include the respective headers
// - Logging: to use the SEISCOMP_* logging macros
#include <seiscomp/logging/log.h>
// - Plugin: to enable building a plugin
#include <seiscomp/core/plugin.h>
// - Filtering
#include <seiscomp/math/filter/chainfilter.h>
#include <seiscomp/math/filter/iirdifferentiate.h>
#include <seiscomp/math/filter/iirintegrate.h>
#include <seiscomp/math/filter/rmhp.h>
// - AmplitudeProcessor interface required to derive from it
#include <seiscomp/processing/amplitudeprocessor.h>
// - For channel data combiners
#include <seiscomp/processing/operator/ncomps.h>


// Everything is implemented in a private namespace to not export any symbols to keep
// the global symbol space clean when loading the plugin.
namespace {


// Use some standard namespaces to write more compact code.
using namespace std;
using namespace Seiscomp;
using namespace Seiscomp::Processing;


#define AMPLITUDE_TYPE "template_pga"


/**
 * @brief Defines the generic component combiner class.
 * This class is being used to combined multiple components sample wise. A concrete
 * two or three component combiner is implemented in the particular spezialization.
 */
template <typename T, int N>
class ComponentCombiner {
	// Process N traces in place of length n
	void operator()(const Record *, T *data[N], int n, const Core::Time &stime, double sfreq) const;

	// publishs a processed component
	bool publish(int c) const;

	void reset();
};


/**
 * @brief The two-component combiner.
 * This combiner spezialized the implementation for two components and calculates
 * the L2 norm (length of a two dimensional vector).
 */
template <typename T>
struct ComponentCombiner<T, 2> {
	void operator()(const Record *, T *data[2], int n, const Core::Time &stime, double sfreq) const {
		for ( int i = 0; i < n; ++i )
			data[0][i] = sqrt(data[0][i] * data[0][i] +
			                  data[1][i] * data[1][i]);
	}

	bool publish(int c) const { return c == 0; }

	void reset() {}
};



class PGAProcessor : public AmplitudeProcessor {
	// ----------------------------------------------------------------------
	//  X'truction
	// ----------------------------------------------------------------------
	public:
		/**
		 * @brief The constructor.
		 * The amplitude type can be an arbitrary name but should not conflict
		 * with any other existing name. It should be the same as the registered
		 * interface name, see the REGISTER_AMPLITUDEPROCESSOR statement at the
		 * end of this file.
		 */
		PGAProcessor()
		: AmplitudeProcessor(AMPLITUDE_TYPE) {
			// Configure the relative time window of the data with respect to
			// the set trigger time.
			setNoiseStart(-10);
			setNoiseEnd(-2);
			setSignalStart(-2);
			setSignalEnd("max(150, R / 3.5)");
			// Data should be prepared that we receive m/s**2 for PGA
			setDataUnit(MeterPerSecondSquared);

			// Tell the amplitude processor to feed data for both horizontal
			// components.
			setDataComponents(Horizontal);
			setTargetComponent(FirstHorizontalComponent);
		}


	// ----------------------------------------------------------------------
	//  Public AmplitudeProcessor interface
	// ----------------------------------------------------------------------
	public:
		bool setup(const Settings &settings) override {
			// Reset operator and filter
			setOperator(nullptr);
			setFilter(nullptr);

			// Call base class implementation
			if ( !AmplitudeProcessor::setup(settings) ) {
				// If the setup of the base class fails, then it does not make sense
				// to continue.
				return false;
			}

			// Check the horizontal components for valid gains
			for ( int i = FirstHorizontal; i <= SecondHorizontal; ++i ) {
				if ( _streamConfig[i].code().empty() ) {
					SEISCOMP_ERROR("Component[%d] code is empty", i);
					setStatus(Error, i);
					return false;
				}

				if ( _streamConfig[i].gain == 0.0 ) {
					SEISCOMP_ERROR("Component[%d] gain is missing (actually zero)", i);
					setStatus(MissingGain, i);
					return false;
				}
			}

			if ( _streamConfig[FirstHorizontal].gainUnit != _streamConfig[SecondHorizontal].gainUnit ) {
				SEISCOMP_ERROR("Both components do not have the same gain unit: %s != %s",
				               _streamConfig[FirstHorizontal].gainUnit,
				               _streamConfig[SecondHorizontal].gainUnit);
				setStatus(ConfigurationError, 1);
				return false;
			}

			string preFilter, postFilter;

			try { preFilter = settings.getString("amplitudes." + type() + ".preFilter"); }
			catch ( ... ) {}

			try { postFilter = settings.getString("amplitudes." + type() + ".filter"); }
			catch ( ... ) {}

			SEISCOMP_DEBUG("  + pre-filter = %s", preFilter);
			SEISCOMP_DEBUG("  + filter = %s", postFilter);

			using OpWrapper = Operator::StreamConfigWrapper<double, 2, ComponentCombiner>;

			if ( !preFilter.empty() ) {
				// Create a filter instance from the provided string.
				string error;
				auto filter = Filter::Create(preFilter, &error);
				if ( !filter ) {
					// If the string is wrong
					SEISCOMP_ERROR("Failed to create pre-filter: %s: %s", preFilter, error);
					setStatus(ConfigurationError, 2);
					return false;
				}

				// Create a waveform operator that combines the two horizontal channels
				// and computes L2 of each horizontal component filtered sample.
				using FilterL2Norm = Operator::FilterWrapper<double, 2, OpWrapper>;
				setOperator(
					new NCompsOperator<double, 2, FilterL2Norm>(
						FilterL2Norm(
							filter, OpWrapper(
								_streamConfig + FirstHorizontal,
								ComponentCombiner<double, 2>()
							)
						)
					)
				);
			}
			else {
				// Create a waveform operator that combines the two horizontal channels
				// and computes L2 of each horizontal component sample.
				setOperator(
					new NCompsOperator<double, 2, OpWrapper>(
						OpWrapper(
							_streamConfig + FirstHorizontal,
							ComponentCombiner<double, 2>()
						)
					)
				);
			}


			if ( !postFilter.empty() ) {
				string error;
				auto filter = Filter::Create(postFilter, &error);
				if ( !filter ) {
					// If the string is wrong
					SEISCOMP_ERROR("Failed to create filter: %s: %s", preFilter, error);
					setStatus(ConfigurationError, 3);
					return false;
				}

				setFilter(filter);
			}

			return true;
		}


		bool feed(const Record *rec) override {
			if ( !getOperator() ) {
				SEISCOMP_ERROR("No operator set, has setup() been called?");
				return false;
			}

			return AmplitudeProcessor::feed(rec);
		}


		//! See Seiscomp::Processing::AmplitudeProcessor::computeAmplitude for
		//! more documentation of this function. It actually computes the
		//! amplitude.
		bool computeAmplitude(const DoubleArray &data,
		                      size_t i1, size_t i2,
		                      size_t si1, size_t si2,
		                      double offset,
		                      AmplitudeIndex *dt,
		                      AmplitudeValue *amplitude,
		                      double *period, double *snr) override {
			// Data is in acceleration: m/s**2
			*period = -1;
			*snr = -1;
			dt->index = find_absmax(data.size(), data.typedData(), si1, si2, offset);
			amplitude->value = abs(data[dt->index] - offset);

			if ( *_noiseAmplitude == 0. ) {
				*snr = -1;
			}
			else {
				*snr = amplitude->value / *_noiseAmplitude;
			}

			if ( *snr < _config.snrMin ) {
				setStatus(LowSNR, *snr);
				return false;
			}

			return true;
		}
};


}


// This defines the entry point after loading this library dynamically. That is
// mandatory if a plugin should be loaded with a SeisComP application.
ADD_SC_PLUGIN(
	"Amplitude PGA plugin template, it just computes the PGA.",
	"Jan Becker, gempa GmbH",
	0, 0, 1
)


// Bind the class PGAProcessor to the name "template_pga". This allows later to
// instantiate this class via the amplitude name. This is different to the generic
// class factory as it registeres a concrete interface, here, the AmplitudeProcessor.
REGISTER_AMPLITUDEPROCESSOR(PGAProcessor, AMPLITUDE_TYPE);
