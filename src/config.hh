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

		std::string calendar_time_zone;
		std::string feed_time_zone;
	};

	inline
	std::ostream& operator<< (std::ostream& o, const config_t& config) {
		o << "config_t: { "
			  "\"calendarTimeZone\": \"" << config.calendar_time_zone << "\""
			", \"feedTimeZone\": \"" << config.feed_time_zone << "\""
			", \"tzdb\": \"" << config.tzdb << "\""
			" ] }";
		return o;
	}

} /* namespace spoon */

#endif /* SPOON_CONFIG_HH__ */

/* eof */