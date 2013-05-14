/* Boilerplate for exporting a data type to the Analytics Engine.
 */

/* VA leaks a dependency upon _MAX_PATH */
#include <cstdlib>

#include <windows.h>

/* Boost string algorithms */
#include <boost/algorithm/string.hpp>

/* Velocity Analytics Plugin Framework */
#include <vpf/vpf.h>

#include "chromium/chromium_switches.hh"
#include "chromium/command_line.hh"
#include "chromium/logging.hh"
#include "tcl.hh"

static const char* kPluginType = "Spoon Type";

namespace { /* anonymous */

class env_t
{
public:
	env_t (const char* varname)
	{
/* startup from clean string */
		CommandLine::Init (0, nullptr);
/* the program name */
		std::string command_line (kPluginType);
/* parameters from environment */
		char* buffer;
		size_t numberOfElements;
		const errno_t err = _dupenv_s (&buffer, &numberOfElements, varname);
		if (0 == err && numberOfElements > 0) {
			command_line.append (" ");
			command_line.append (buffer);
			free (buffer);
		}
		std::string log_path = GetLogFileName();
/* update command line */
		CommandLine::ForCurrentProcess()->ParseFromString (command_line);
/* forward onto logging */
		logging::InitLogging(
			log_path.c_str(),
			DetermineLogMode (*CommandLine::ForCurrentProcess()),
			logging::DONT_LOCK_LOG_FILE,
			logging::APPEND_TO_OLD_LOG_FILE,
			logging::ENABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS
			);
		logging::SetLogMessageHandler (log_handler);
	}

protected:
	std::string GetLogFileName() {
		const std::string log_filename ("/Spoon.log");
		return log_filename;
	}

	logging::LoggingDestination DetermineLogMode (const CommandLine& command_line) {
#ifdef NDEBUG
		const logging::LoggingDestination kDefaultLoggingMode = logging::LOG_NONE;
#else
		const logging::LoggingDestination kDefaultLoggingMode = logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG;
#endif

		logging::LoggingDestination log_mode;
// Let --enable-logging=file force file logging, particularly useful for
// non-debug builds where otherwise you can't get logs on fault at all.
		if (command_line.GetSwitchValueASCII (switches::kEnableLogging) == "file")
			log_mode = logging::LOG_ONLY_TO_FILE;
		else
			log_mode = kDefaultLoggingMode;
		return log_mode;
	}

/* Vhayu log system wrapper */
	static bool log_handler (int severity, const char* file, int line, size_t message_start, const std::string& str)
	{
		int priority;
		switch (severity) {
		default:
		case logging::LOG_INFO:		priority = eMsgInfo; break;
		case logging::LOG_WARNING:	priority = eMsgLow; break;
		case logging::LOG_ERROR:	priority = eMsgMedium; break;
		case logging::LOG_FATAL:	priority = eMsgFatal; break;
		}
/* Yay, broken APIs */
		std::string str1 (boost::algorithm::trim_right_copy (str));
		MsgLog (priority, 0, const_cast<char*> ("%s"), str1.c_str());
/* allow additional log targets */
		return false;
	}
};

class factory_t :
	public vpf::ObjectFactory
{
	env_t env_;

public:
	factory_t() :	
		env_ ("TR_DEBUG")
	{
		LOG(INFO) << "Registering plugin type \"" << kPluginType << "\".";
		registerType (kPluginType);
	}

/* no API to unregister type. */

	virtual void* newInstance (const char* type) override
	{
		LOG(INFO) << "Requested new instance of type \"" << type << "\".";
		assert (0 == strcmp (kPluginType, type));
		return static_cast<vpf::AbstractUserPlugin*> (new spoon::tcl_plugin_t());
	}
};

static factory_t g_factory_instance;

} /* anonymous namespace */

/* eof */
