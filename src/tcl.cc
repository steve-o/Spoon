/* A basic Velocity Analytics User-Plugin to export a new Tcl command.
 */

/* special usage of sprintf to prevent overflow, thus ignore warnings */
#define _CRT_SECURE_NO_WARNINGS

#include "tcl.hh"

#include <algorithm>
#include <cstdint>

/* C++11 Chrono */
#include <boost/chrono.hpp>

/* Boost Posix Time */
#include <boost/date_time/gregorian/gregorian_types.hpp>

/* Velocity Analytics Plugin Framework */
#include <vpf/vpf.h>
#include <FlexRecReader.h>

#include "chromium/command_line.hh"
#include "chromium/logging.hh"

#include "version.hh"

static const char* kFunctionName	= "get_spoon";

#if 0	/* test environment */
static const char kVhBaseTime[]		= "VhBaseTime";
static const char kLastTradePrice[]	= "LastPrice";
static const char kCumulativeVolume[]	= "CummulativeVolume";
static const char kNetChange[]		= "BidPrice";
static const char kPercentChange[]	= "AskPrice";
#else
static const char kVhBaseTime[]		= "VhBaseTime";
static const char kLastTradePrice[]	= "LastTradePrice";
static const char kCumulativeVolume[]	= "CumulativeVolume";
static const char kNetChange[]		= "NetChange";
static const char kPercentChange[]	= "PercentChange";
#endif

namespace switches {

static const char kSymbolName[]		= "ric";
static const char kStartTime[]		= "start";
static const char kEndTime[]		= "end";
static const char kDirection[]		= "direction";
static const char kDefinitionName[]	= "record";
static const char kFieldList[]		= "fields";
static const char kRecordLimit[]	= "limit";
static const char kQueryProperty[]	= "query-property";
static const char kUseTimeT[]		= "use-time_t";
static const char kTimezone[]		= "tz";
static const char kUseHoliday[]		= "use-holiday";

} // namespace switches

/* http://en.wikipedia.org/wiki/Unix_epoch */
static const boost::gregorian::date kUnixEpoch (1970, 1, 1);

/* Convert Posix time to Unix Epoch time.
 */
template< typename TimeT >
inline
TimeT
to_unix_epoch (
	const boost::posix_time::ptime t
	)
{
	return (t - boost::posix_time::ptime (kUnixEpoch)).total_seconds();
}

/* Is today<date> a business day, per TBSDK.  Assumes local calendar as per TBSDK.
 */
static
bool
is_business_day (
	const boost::gregorian::date local_date,
	const boost::local_time::time_zone_ptr zone
	)
{
	BusinessDayInfo bd;
	CHECK (!local_date.is_not_a_date());
	const boost::local_time::local_date_time ldt (local_date, boost::posix_time::time_duration (0, 0, 0), zone, boost::local_time::local_date_time::NOT_DATE_TIME_ON_ERROR);
	const auto time32 = to_unix_epoch<__time32_t> (ldt.utc_time());
/* time32 is taken as UTC but interpreted in OS time zone for the actual calendar */
	return (0 != TBPrimitives::BusinessDay (time32, &bd));
}

void
spoon::tcl_plugin_t::init (
	const vpf::UserPluginConfig& vpf_config
	)
{
/* Thunk to VA user-plugin base class. */
	AbstractUserPlugin::init (vpf_config);

	LOG(INFO) << "{ "
		  "\"pluginType\": \"" << vpf_config.getPluginType() << "\""
		", \"pluginId\": \"" << vpf_config.getPluginId() << "\""
		", \"version\": \"" << version_major << '.' << version_minor << '.' << version_build << "\""
		", \"build\": { "
			  "\"date\": \"" << build_date << "\""
			", \"time\": \"" << build_time << "\""
			", \"system\": \"" << build_system << "\""
			", \"machine\": \"" << build_machine << "\""
			" }"
		" }";

	CommandLine::Init (0, nullptr);

	if (!config_.ParseDomElement (vpf_config.getXmlConfigData())) {
		is_shutdown_ = true;
		throw vpf::UserPluginException ("Invalid configuration, aborting.");
	}
	if (!Init()) {
		is_shutdown_ = true;
		throw vpf::UserPluginException ("Initialization failed, aborting.");
	} else {
		LOG(INFO) << "Init complete, awaiting queries.";
	}
}

bool
spoon::tcl_plugin_t::Init()
{
	LOG(INFO) << config_;

/* Boost time zone database. */
	try {
		tzdb_.load_from_file (config_.tzdb);
/* calendar time zone */
		calendar_time_zone_ = tzdb_.time_zone_from_region (config_.calendar_time_zone);
		if (nullptr == calendar_time_zone_)
			calendar_time_zone_.reset (new boost::local_time::posix_time_zone (config_.calendar_time_zone));
		LOG(INFO) << "calendar time zone: " << calendar_time_zone_->to_posix_string();
/* feed time zone */
		feed_time_zone_ = tzdb_.time_zone_from_region (config_.feed_time_zone);
		if (nullptr == feed_time_zone_)
			feed_time_zone_.reset (new boost::local_time::posix_time_zone (config_.feed_time_zone));
		LOG(INFO) << "feed time zone: " << feed_time_zone_->to_posix_string();
	} catch (const boost::local_time::data_not_accessible& e) {
		LOG(ERROR) << "Time zone specifications cannot be loaded: " << e.what();
		return false;
	} catch (const boost::local_time::bad_field_count& e) {
		LOG(ERROR) << "Time zone specifications malformed: " << e.what();
		return false;
	} catch (const std::exception& e) {
		LOG(ERROR) << "Unhandled exception: " << e.what();
		return false;
	}

/* Time zone conversion tests */
	{
		using namespace boost;
		using namespace local_time;
		using namespace posix_time;

		struct {
			__time32_t	tt;
			char*		name;
			bool		is_cached_date;
			bool		is_cached_date_a_holiday;
			bool		is_calculated_holiday;
		} tests[] = {
			{ 1368208800, "Test #1.1: Fri 10    6pm EST", false, true,  false },
			{ 1368210600, "Test #1.2: Fri 10 6:30pm EST", true,  false, false },
			{ 1368295200, "Test #2.1: Sat 11    6pm EST", false, true,  true  },
			{ 1368297000, "Test #2.2: Sat 11 6:30pm EST", true,  true,  true  },
			{ 1368381600, "Test #3.1: Sun 12    6pm EST", false, true,  true  },
			{ 1368383400, "Test #3.2: Sun 12 6:30pm EST", true,  true,  true  },
			{ 1368468000, "Test #4.1: Mon 13    6pm EST", false, true,  false },
			{ 1368469800, "Test #4.2: Mon 13 6:30pm EST", true,  false, false }
		};
		auto est_time_zone = tzdb_.time_zone_from_region ("EST-05");
		for (size_t i = 0; i < _countof (tests); ++i)
		{
			boost::gregorian::date previous_date (not_a_date_time);
			bool previous_date_is_holiday = true;

			LOG(INFO) << tests[i].name;
			__time32_t tt = tests[i].tt;
			{
				const ptime feed_time (from_time_t (tt));
				const local_date_time feed_ldt (feed_time.date(), feed_time.time_of_day(), est_time_zone, local_date_time::NOT_DATE_TIME_ON_ERROR);

				const local_date_time query_ldt (feed_ldt.utc_time(), est_time_zone);
				const auto query_date = query_ldt.local_time().date();

				const bool is_cached_date = (query_date == previous_date);
				const bool is_calculated_holiday = !(is_business_day (query_date, calendar_time_zone_));

				if (is_cached_date == tests[i].is_cached_date)
					LOG(INFO) << "SUCCESS: tt is " << (is_cached_date ? "" : "not ") << "a cached date";
				else
					LOG(ERROR) << "FAILURE: cached date mismatch, cache=" << previous_date << " query_date=" << query_date << ", is_cached_date=" << std::boolalpha << is_cached_date;
				if (previous_date_is_holiday == tests[i].is_cached_date_a_holiday)
					LOG(INFO) << "SUCCESS: cached previous date as " << (previous_date_is_holiday ? "" : "not ") << "a holiday";
				else
					LOG(ERROR) << "FAILURE: cached previous date state does not match";
				if (is_calculated_holiday == tests[i].is_calculated_holiday)
					LOG(INFO) << "SUCCESS: tt is " << (is_calculated_holiday ? "" : "not ") << "a calculated holiday";
				else
					LOG(ERROR) << "FAILURE: calculated holiday mismatch";

				previous_date = query_date;
				previous_date_is_holiday = is_calculated_holiday;
			}

			LOG(INFO) << tests[++i].name;
			tt = tests[i].tt;
			{
				const ptime feed_time (from_time_t (tt));
				const local_date_time feed_ldt (feed_time.date(), feed_time.time_of_day(), est_time_zone, local_date_time::NOT_DATE_TIME_ON_ERROR);

				const local_date_time query_ldt (feed_ldt.utc_time(), est_time_zone);
				const auto query_date = query_ldt.local_time().date();

				const bool is_cached_date = (query_date == previous_date);
				const bool is_calculated_holiday = !(is_business_day (query_date, calendar_time_zone_));

				if (is_cached_date == tests[i].is_cached_date)
					LOG(INFO) << "SUCCESS: tt is " << (is_cached_date ? "" : "not ") << "a cached date";
				else
					LOG(INFO) << "FAILURE: cached date mismatch, cache=" << previous_date << " query_date=" << query_date << ", is_cached_date=" << std::boolalpha << is_cached_date;
				if (previous_date_is_holiday == tests[i].is_cached_date_a_holiday)
					LOG(INFO) << "SUCCESS: cached previous date as " << (previous_date_is_holiday ? "" : "not ") << "a holiday";
				else
					LOG(ERROR) << "FAILURE: cached previous date state does not match";
				if (is_calculated_holiday == tests[i].is_calculated_holiday)
					LOG(INFO) << "SUCCESS: tt is " << (is_calculated_holiday ? "" : "not ") << "a calculated holiday";
				else
					LOG(INFO) << "FAILURE: calculated holiday mismatch";
			}
		}
	}

/* Register Tcl API. */
	registerCommand (getId(), kFunctionName);
	LOG(INFO) << "Registered Tcl API \"" << kFunctionName << "\"";
	return true;
}

void
spoon::tcl_plugin_t::destroy()
{
/* Unregister Tcl API. */
	deregisterCommand (getId(), kFunctionName);
	LOG(INFO) << "Unregistered Tcl API \"" << kFunctionName << "\"";

	AbstractUserPlugin::destroy();
}

/* Tcl boilerplate.
 *
 * get_spoon --ric=symName 
 *         --start=timeBgn --end=timeEnd
 *         --direction=direction
 *         --limit=numofrec
 *         --record=definitionName
 *         --property=qryprops
 */

#define TclFreeObj \
	(tclStubsPtr->PTclFreeObj)		/* 30 */
#define Tcl_GetLongFromObj \
	(tclStubsPtr->PTcl_GetLongFromObj)	/* 39 */
#define Tcl_GetStringFromObj \
	(tclStubsPtr->PTcl_GetStringFromObj)	/* 41 */
#define Tcl_ListObjAppendElement \
	(tclStubsPtr->PTcl_ListObjAppendElement)/* 44 */
#define Tcl_ListObjIndex \
	(tclStubsPtr->PTcl_ListObjIndex)	/* 46 */
#define Tcl_ListObjLength \
	(tclStubsPtr->PTcl_ListObjLength)	/* 47 */
#define Tcl_NewDoubleObj \
	(tclStubsPtr->PTcl_NewDoubleObj)	/* 51 */
#define Tcl_NewListObj \
	(tclStubsPtr->PTcl_NewListObj)		/* 53 */
#define Tcl_NewLongObj \
	(tclStubsPtr->PTcl_NewLongObj)		/* 54 */
#define Tcl_NewStringObj \
	(tclStubsPtr->PTcl_NewStringObj)	/* 56 */
#define Tcl_SetResult \
	(tclStubsPtr->PTcl_SetResult)		/* 232 */
#define Tcl_SetObjResult \
	(tclStubsPtr->PTcl_SetObjResult)	/* 235 */
#define Tcl_WrongNumArgs \
	(tclStubsPtr->PTcl_WrongNumArgs)	/* 264 */

int
spoon::tcl_plugin_t::execute (
	const vpf::CommandInfo& cmdInfo,
	vpf::TCLCommandData& cmdData
	)
{
	TCLLibPtrs* tclStubsPtr = static_cast<TCLLibPtrs*> (cmdData.mClientData);
	Tcl_Interp* interp = cmdData.mInterp;		/* Current interpreter. */
	int objc = cmdData.mObjc;			/* Number of arguments. */
	Tcl_Obj** CONST objv = cmdData.mObjv;		/* Argument strings. */

	Tcl_Obj* tcl_result = nullptr;
	char error_text[1024];

	if (is_shutdown_) {
		Tcl_SetResult (interp, "Plugin has shutdown.", TCL_STATIC);
		return TCL_ERROR;
	}

	try {
		boost::chrono::high_resolution_clock::time_point t0, t1;
		if (VLOG_IS_ON(1)) t0 = boost::chrono::high_resolution_clock::now();

/* Parse Tcl arguments as a command line */
		std::vector<std::string> argv;
		for (size_t i = 0; i < objc; ++i) {
			int len = 0; char* text = Tcl_GetStringFromObj (objv[i], &len);
			const std::string arg (text, len);
			argv.push_back (arg);
		}
		CommandLine tcl_args (argv);

		VLOG(1) << "execute (" << tcl_args.GetCommandLineString() << ")";

		if (!tcl_args.HasSwitch (switches::kSymbolName)) {
			Tcl_SetResult (interp, "Symbol name is required.", TCL_STATIC);
			return TCL_ERROR;
		}
		const std::string symbol_name (tcl_args.GetSwitchValueASCII (switches::kSymbolName));
		if (symbol_name.empty()) {
			Tcl_SetResult (interp, "Symbol name is empty.", TCL_STATIC);
			return TCL_ERROR;
		}

/* FlexRecord definition name */
		if (!tcl_args.HasSwitch (switches::kDefinitionName)) {
			Tcl_SetResult (interp, "FlexRecord definition name is required.", TCL_STATIC);
			return TCL_ERROR;
		}
		const std::string record_name (tcl_args.GetSwitchValueASCII (switches::kDefinitionName));
		if (record_name.empty()) {
			Tcl_SetResult (interp, "FlexRecord definition name is empty.", TCL_STATIC);
			return TCL_ERROR;
		}

/* Query start time */
		__time32_t from = 0;
		if (tcl_args.HasSwitch (switches::kStartTime)) {
			const std::string start_time (tcl_args.GetSwitchValueASCII (switches::kStartTime));
			if (!start_time.empty())
				from = std::stoi (start_time.c_str());
		}

/* Query end time */
		__time32_t till = 0;
		if (tcl_args.HasSwitch (switches::kEndTime)) {
			const std::string end_time (tcl_args.GetSwitchValueASCII (switches::kEndTime));
			if (!end_time.empty())
				till = std::stoi (end_time.c_str());
		}

/* 1 == Decreasing timeorder, 0 == increasing timeorder */
		int direction = 0;
		if (tcl_args.HasSwitch (switches::kDirection)) {
			const std::string direction_string (tcl_args.GetSwitchValueASCII (switches::kDirection));
			if (!direction_string.empty())
				direction = std::stoi (direction_string.c_str());
		}

/* Total number of records to return */
		long limit = 0;
		if (tcl_args.HasSwitch (switches::kRecordLimit)) {
			const std::string record_limit (tcl_args.GetSwitchValueASCII (switches::kRecordLimit));
			if (!record_limit.empty())
				limit = std::stol (record_limit.c_str());
		}

/* FlexRecord query properties */
		std::string query_property (tcl_args.GetSwitchValueASCII (switches::kQueryProperty));

/* Time for holidays */
		boost::local_time::time_zone_ptr query_time_zone (feed_time_zone_);
		const bool use_holiday = tcl_args.HasSwitch (switches::kUseHoliday);
		if (use_holiday) {
			const std::string region (tcl_args.GetSwitchValueASCII (switches::kTimezone));
			if (!region.empty()) {
				const boost::local_time::time_zone_ptr tzptr = tzdb_.time_zone_from_region (region);
				if (nullptr != tzptr) query_time_zone = tzptr;
			}
		}

		if (VLOG_IS_ON(2)) {
			VLOG(2) << "symbol name: " << symbol_name;
			VLOG(2) << "record name: " << record_name;
			VLOG(2) << "from: " << from;
			VLOG(2) << "till: " << till;
			VLOG(2) << "direction: " << direction;
			VLOG(2) << "limit: " << limit;
			VLOG(2) << "query property: " << query_property;
			VLOG(2) << "holidays: " << std::boolalpha << use_holiday;
			VLOG(2) << "timezone: " << query_time_zone->std_zone_name();
		}

		Tcl_Obj* tcl_result = Tcl_NewListObj (0, nullptr);

#ifdef USE_FLEXRECORD_PRIMITIVES
		FlexRecDefinitionManager* manager = FlexRecDefinitionManager::GetInstance (nullptr);
		std::unique_ptr<FlexRecWorkAreaElement> work_area (manager->AcquireWorkArea(), [this](FlexRecWorkAreaElement* work_area_){ manager->ReleaseWorkArea (work_area_); });
		std::unique_ptr<FlexRecViewElement> view_element (manager->AcquireView(), [this](FlexRecViewElement* view_element_){ manager->ReleaseView (view_element_); });
		if (!query_property.empty()) {
			QueryProps qp;
			if (1 != qp.ParseProps (query_property.c_str(), error_text)) {
				Tcl_SetResult (interp, error_text, TCL_VOLATILE);
				return TCL_ERROR;
			}
		}
		U64 numRecs = FlexRecPrimitives::GetFlexRecords (symbol_name.c_str(),
								 const_cast<char*> (record_name.c_str()),
								 from, till, direction,
								 limit,
								 view_element->view,
								 work_area->data,
								 OnFlexRecord,
								 this); /* closure */
#else /* USE_FLEXRECORD_CURSOR */
/* Symbol names */
		std::set<std::string> symbol_set;
		symbol_set.insert (symbol_name);

/* FlexRecord fields */
		int64_t VhBaseTime;
		double  LastTradePrice;
		uint64_t CumulativeVolume;
		double  NetChange;
		double  PercentChange;
		std::set<FlexRecBinding> binding_set;
		FlexRecBinding binding (record_name.c_str());
		binding.Bind (kVhBaseTime, &VhBaseTime);
		binding.Bind (kLastTradePrice, &LastTradePrice);
		binding.Bind (kCumulativeVolume, &CumulativeVolume);
		binding.Bind (kNetChange, &NetChange);
		binding.Bind (kPercentChange, &PercentChange);
		binding_set.insert (binding);

/* Open FlexRecord cursor */
		FlexRecReader fr;
		const int cursor_status = fr.Open (symbol_set,
						   binding_set,
						   from, till, direction,
						   limit,
						   error_text,
						   nullptr /* For internal use: always NULL */,
						   nullptr /* For internal use: always NULL */,
						   query_property.c_str());
		if (1 != cursor_status) {
			if (nullptr != tcl_result) TclFreeObj (tcl_result);
			Tcl_SetResult (interp, error_text, TCL_VOLATILE);
			return TCL_ERROR;
		}

		const bool use_time_t = tcl_args.HasSwitch (switches::kUseTimeT);

/* Iterate through all ticks */
		boost::gregorian::date previous_date (boost::gregorian::not_a_date_time);
		bool previous_date_is_holiday = true;

		while (fr.Next()) {
/* Convert timestamp, time_t will be in local time zone */
			__time32_t tt;
			VHTimeProcessor::VHTimeToTT (&VhBaseTime, &tt);
/* Skip holidays */
			if (use_holiday) {
				using namespace boost;
				using namespace local_time;
				using namespace posix_time;

				const ptime feed_time (from_time_t (tt));
				const local_date_time feed_ldt (feed_time.date(), feed_time.time_of_day(), feed_time_zone_, local_date_time::NOT_DATE_TIME_ON_ERROR);

				const local_date_time query_ldt (feed_ldt.utc_time(), query_time_zone);
				const auto query_date = query_ldt.local_time().date();

				if (query_date == previous_date) {
					if (previous_date_is_holiday)
						continue;
				} else {
					previous_date = query_date;
					previous_date_is_holiday = !is_business_day (query_date, calendar_time_zone_);
					if (previous_date_is_holiday)
						continue;
				}
			}
/* Pass to Tcl */
			if (use_time_t) {
				Tcl_Obj* tcl_element[] = {
					Tcl_NewLongObj (tt),
					Tcl_NewDoubleObj (LastTradePrice),
					Tcl_NewLongObj (static_cast<long>(CumulativeVolume)),
					Tcl_NewDoubleObj (NetChange),
					Tcl_NewDoubleObj (PercentChange)
				};
				Tcl_ListObjAppendElement (interp, tcl_result, Tcl_NewListObj (_countof (tcl_element), tcl_element));
			} else {
/* Promote timestamp to string to workaround old Tcl lack of 64-bit support */
				static const int max_size = std::numeric_limits<unsigned long>::digits10 + 1;
				char bignum[max_size] = {0};
				sprintf (bignum, "%lu", static_cast<unsigned long>(VhBaseTime));
				Tcl_Obj* tcl_element[] = {
					Tcl_NewStringObj (bignum, -1),
					Tcl_NewDoubleObj (LastTradePrice),
					Tcl_NewLongObj (static_cast<long>(CumulativeVolume)),
					Tcl_NewDoubleObj (NetChange),
					Tcl_NewDoubleObj (PercentChange)
				};
				Tcl_ListObjAppendElement (interp, tcl_result, Tcl_NewListObj (_countof (tcl_element), tcl_element));
			}
		}

/* Cleanup */
		fr.Close();
#endif
		Tcl_SetObjResult (interp, tcl_result);

		if (VLOG_IS_ON(1)) {
			t1 = boost::chrono::high_resolution_clock::now();
			VLOG(1) << "execute complete in " << boost::chrono::duration_cast<boost::chrono::microseconds>(t1 - t0).count() << "us";
		}
		return TCL_OK;
	}
/* FlexRecord exceptions */
	catch (const vpf::PluginFrameworkException& e) {
		if (nullptr != tcl_result) TclFreeObj (tcl_result);
		/* yay broken Tcl API */
		Tcl_SetResult (interp, const_cast<char*> (e.what()), TCL_VOLATILE);
	}
	catch (const std::exception& e) {
		if (nullptr != tcl_result) TclFreeObj (tcl_result);
		Tcl_SetResult (interp, const_cast<char*> (e.what()), TCL_VOLATILE);
	}
	catch (...) {
		if (nullptr != tcl_result) TclFreeObj (tcl_result);
		Tcl_SetResult (interp, "Unresolved exception.", TCL_STATIC);
	}
	return TCL_ERROR;
}

/* Returns <1> to continue processing, <2> to halt processing due to an error.
 */
int
spoon::tcl_plugin_t::OnFlexRecord (
	FRTreeCallbackInfo* info
	)
{
	CHECK(nullptr != info->callersData);

/* Append to Tcl result list */
//	Tcl_Obj* tcl_element[] = {};
//	Tcl_Obj* tcl_row = Tcl_NewListObj (_countof (tcl_element), tcl_element);
//	if (nullptr == tcl_row)
//		return 2;

//	Tcl_ListObjAppendElement (interp, tcl_result, tcl_row);

/* Continue processing */
	return 1;
}

/* eof */
