/* User-configurable settings.
 */

#include "config.hh"

#include "chromium/logging.hh"

spoon::config_t::config_t()
/* default values */
{
/* C++11 initializer lists not supported in MSVC2010 */
}

/* Minimal error handling parsing of an Xml node pulled from the
 * Analytics Engine.
 *
 * Returns true if configuration is valid, returns false on invalid content.
 */

using namespace xercesc;

/** L"" prefix is used in preference to u"" because of MSVC2010 **/

bool
spoon::config_t::Validate()
{
	if (tzdb.empty()) {
		LOG(ERROR) << "Undefined time zone database.";
		return false;
	}
	if (calendar_time_zone.empty()) {
		LOG(ERROR) << "Undefined calendar time zone.";
		return false;
	}
	if (feed_time_zone.empty()) {
		LOG(ERROR) << "Undefined feed time zone.";
		return false;
	}
	return true;
}

bool
spoon::config_t::ParseDomElement (
	const DOMElement*	root
	)
{
	vpf::XMLStringPool xml;
	const DOMNodeList* nodeList;

	LOG(INFO) << "Parsing configuration ...";
/* Plugin configuration wrapped within a <config> node. */
	nodeList = root->getElementsByTagName (L"config");

	for (int i = 0; i < nodeList->getLength(); i++) {
		if (!ParseConfigNode (nodeList->item (i))) {
			LOG(ERROR) << "Failed parsing <config> nth-node #" << (1 + i) << '.';
			return false;
		}
	}
	if (0 == nodeList->getLength())
		LOG(WARNING) << "No <config> nodes found in configuration.";

	if (!Validate()) {
		LOG(ERROR) << "Failed validation, malformed configuration file requires correction.";
		return false;
	}

	LOG(INFO) << "Parsing complete.";
	return true;
}

bool
spoon::config_t::ParseConfigNode (
	const DOMNode*		node
	)
{
	const DOMElement* elem = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;
	const DOMNodeList* nodeList;

/* <Spoon> */
	nodeList = elem->getElementsByTagName (L"Spoon");
	for (int i = 0; i < nodeList->getLength(); i++) {
		if (!ParseSpoonNode (nodeList->item (i))) {
			LOG(ERROR) << "Failed parsing <Spoon> nth-node #" << (1 + i) << '.';
			return false;
		}
	}
	if (0 == nodeList->getLength())
		LOG(WARNING) << "No <Spoon> nodes found in configuration.";
	return true;
}

/* <Spoon> */
bool
spoon::config_t::ParseSpoonNode (
	const DOMNode*		node
	)
{
	const DOMElement* elem = static_cast<const DOMElement*>(node);
	vpf::XMLStringPool xml;
	std::string attr;

/* calendarTimeZone="text" */
	attr = xml.transcode (elem->getAttribute (L"calendarTimeZone"));
	if (!attr.empty())
		calendar_time_zone = attr;
/* feedTimeZone="text" */
	attr = xml.transcode (elem->getAttribute (L"feedTimeZone"));
	if (!attr.empty())
		feed_time_zone = attr;
/* tzdb="file" */
	attr = xml.transcode (elem->getAttribute (L"TZDB"));
	if (!attr.empty())
		tzdb = attr;
	return true;
}

/* </Spoon> */
/* </config> */

/* eof */