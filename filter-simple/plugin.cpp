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


#define SEISCOMP_COMPONENT FilterSimple

#include <seiscomp/logging/log.h>
#include <seiscomp/core/plugin.h>
#include <seiscomp/math/filter.h>


namespace {


using namespace std;
using namespace Seiscomp;


/**
 * @brief The SimpleFilter class implements the InPlaceFilter interface
 *        used by SeisComP to filter traces in any application.
 *
 * This class is usually compiled as a shared library and loaded as a
 * plugin. Furthermore the filter is configured to use this implementation,
 * see REGISTER_INPLACE_FILTER.
 *
 * @code
 * plugins = ${plugins}, tmplfilter
 * filter = "SIMPLE(1,0)"
 * @endcode
 */
template <typename T>
class SimpleFilter : public Math::Filtering::InPlaceFilter<T> {
	// ------------------------------------------------------------------
	//  X'truction
	// ------------------------------------------------------------------
	public:
		//! C'tor
		SimpleFilter(double scale = 1.0, double offset = 0.0)
		: _scale(scale), _offset(offset) {}


	// ------------------------------------------------------------------
	//  Public InplaceFilter interface
	// ------------------------------------------------------------------
	public:
		void setSamplingFrequency(double fsamp) override {
			// Set internal variables based on expected sampling rate.
		}

		int setParameters(int n, const double *params) override {
			// Set parameters. This is usually done from parsing a filter string
			// and forwarding the parameters of the filter as double array.
			// Example: SIMPLE(1, 2) -> setParameters(2, [1, 2])
			// The function returns a positive value to indicate the number of
			// required parameters or handled parameters or a negative number to
			// indicate an error at the position abs(r)-1.
			if ( n != 2 ) {
				return 2;
			}

			// Set the internal parameters according to the input parameters.
			_scale = params[0];
			_offset = params[1];

			return 2;
		}

		void apply(int n, T *inout) override {
			// Simply apply the parameters to the input data.
			for ( int i = 0; i < n; ++i, ++inout ) {
				*inout = *inout * _scale + _offset;
			}
		}

		Seiscomp::Math::Filtering::InPlaceFilter<T>* clone() const override {
			return new SimpleFilter<T>(_scale, _offset);
		}


	// ------------------------------------------------------------------
	//  Private members
	// ------------------------------------------------------------------
	private:
		double _scale{1.0};
		double _offset{0.0};
};


INSTANTIATE_INPLACE_FILTER(SimpleFilter, SC_SYSTEM_CORE_API);
REGISTER_INPLACE_FILTER(SimpleFilter, "SIMPLE");


}


ADD_SC_PLUGIN(
	"Filter plugin template, it implements a simple scale and offset filter",
	"Jan Becker, gempa GmbH",
	0, 0, 1
)
