/* A basic Velocity Analytics User-Plugin to export a new Tcl command.
 */

#ifndef __SPOON_TCL_HH__
#define __SPOON_TCL_HH__

/* Boost noncopyable base class */
#include <boost/utility.hpp>

/* Boost Date Time */
#include <boost/date_time/local_time/local_time.hpp>

/* Boost Posix Time */
#include <boost/date_time/posix_time/posix_time.hpp>

/* Velocity Analytics Plugin Framework */
#include <vpf/vpf.h>

#include "config.hh"

namespace spoon
{
	class tcl_plugin_t :
		public vpf::AbstractUserPlugin,
		public vpf::Command,
		boost::noncopyable
	{
	public:
		tcl_plugin_t() : is_shutdown_ (false) {}

		virtual void init (const vpf::UserPluginConfig& config) override;
		virtual void destroy() override;
		virtual int execute (const vpf::CommandInfo& cmdInfo, vpf::TCLCommandData& cmdData) override;

	protected:
		bool Init();
		int OnFlexRecord (FRTreeCallbackInfo* info);

/* Application configuration. */
		config_t config_;

/* Significant failure has occurred, so ignore all runtime events flag. */
		bool is_shutdown_;

		boost::local_time::tz_database tzdb_;
		boost::local_time::time_zone_ptr default_timezone_;
	};

} /* namespace spoon */

#endif /* __SPOON_TCL_HH__ */

/* eof */
