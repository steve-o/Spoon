/* A basic Velocity Analytics User-Plugin to export a new Tcl command.
 */

#ifndef __SPOON_TCL_HH__
#define __SPOON_TCL_HH__

/* Boost noncopyable base class */
#include <boost/utility.hpp>

/* Velocity Analytics Plugin Framework */
#include <vpf/vpf.h>

namespace spoon
{
	class tcl_plugin_t :
		public vpf::AbstractUserPlugin,
		public vpf::Command,
		boost::noncopyable
	{
	public:
		tcl_plugin_t() {}

		virtual void init (const vpf::UserPluginConfig& config) override;
		virtual void destroy() override;
		virtual int execute (const vpf::CommandInfo& cmdInfo, vpf::TCLCommandData& cmdData) override;

	protected:
		int OnFlexRecord (FRTreeCallbackInfo* info);
	};

} /* namespace spoon */

#endif /* __SPOON_TCL_HH__ */

/* eof */
