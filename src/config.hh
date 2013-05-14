/* User-configurable settings.
 *
 * NB: all strings are locale bound.
 */

#ifndef SPOON_CONFIG_HH__
#define SPOON_CONFIG_HH__

#include <string>

/* Velocity Analytics Plugin Framework */
#include <vpf/vpf.h>

namespace spoon
{
	struct config_t
	{
		config_t();

		bool ParseDomElement (const xercesc::DOMElement* elem);
		bool ParseConfigNode (const xercesc::DOMNode* node);
		bool ParseSpoonNode (const xercesc::DOMNode* node);
		bool Validate();

//  File path for time zone database that Boost::DateTimes likes.
		std::string tzdb;

//  Default time zone.
		std::string tz;
	};

	inline
	std::ostream& operator<< (std::ostream& o, const config_t& config) {
		o << "config_t: { "
			  "\"tz\": \"" << config.tz << "\""
			", \"tzdb\": \"" << config.tzdb << "\""
			" ] }";
		return o;
	}

} /* namespace spoon */

#endif /* SPOON_CONFIG_HH__ */

/* eof */